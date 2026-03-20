#include "SFSoldierController.h"

#include "SFSoldierCharacter.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ScoreFrenzy/Perception/SFPerceptionSystem.h"
#include "ScoreFrenzy/Perception/SFTargetComponent.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ASFSoldierController::ASFSoldierController()
{
	PrimaryActorTick.bCanEverTick = true;

	PathComp = CreateDefaultSubobject<USFPathComponent>(TEXT("PathComponent"));
	PerceptionComp = CreateDefaultSubobject<USFPerceptionComponent>(TEXT("PerceptionComponent"));
}

// ---------------------------------------------------------------------------
// AAIController overrides
// ---------------------------------------------------------------------------

void ASFSoldierController::BeginPlay()
{
	Super::BeginPlay();
	SetDifficulty(Difficulty);
}

void ASFSoldierController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Reset state when possessing a new pawn
	CurrentState      = ESFSoldierState::Patrol;
	bCanShoot         = false;
	bAlertInitiated   = false;
	AlertShareTimer   = AlertShareInterval;
	CurrentWaypointIndex = 0;
	bWaypointInProgress  = false;
}

void ASFSoldierController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ASFSoldierCharacter* Soldier = Cast<ASFSoldierCharacter>(GetPawn());
	if (!Soldier || Soldier->IsDead()) return;

	// --- Footstep awareness (Medium / Hard) ---
	if (bFootstepAwareness)
	{
		UpdateFootstepAwareness(DeltaSeconds);
	}

	// --- Alert sharing timer (Medium / Hard, while aware of player) ---
	if (bAlertSharing &&
		(CurrentState == ESFSoldierState::Chase || CurrentState == ESFSoldierState::Attack))
	{
		AlertShareTimer -= DeltaSeconds;
		if (AlertShareTimer <= 0.0f)
		{
			ShareAlert(AlertShareCount);
			AlertShareTimer = AlertShareInterval;
		}
	}

	// --- State transitions ---
	CheckStateTransitions();

	// --- Per-state update ---
	switch (CurrentState)
	{
	case ESFSoldierState::Patrol:      UpdatePatrol(DeltaSeconds);      break;
	case ESFSoldierState::Chase:       UpdateChase(DeltaSeconds);       break;
	case ESFSoldierState::Attack:      UpdateAttack(DeltaSeconds);      break;
	case ESFSoldierState::Investigate: UpdateInvestigate(DeltaSeconds); break;
	case ESFSoldierState::Retreat:     UpdateRetreat(DeltaSeconds);     break;
	case ESFSoldierState::Heal:        UpdateHeal(DeltaSeconds);        break;
	}
}

// ---------------------------------------------------------------------------
// Difficulty setup
// ---------------------------------------------------------------------------

void ASFSoldierController::SetDifficulty(ESFSoldierDifficulty NewDifficulty)
{
	Difficulty = NewDifficulty;

	switch (Difficulty)
	{
	case ESFSoldierDifficulty::Easy:
		bRandomPatrol        = true;
		bFootstepAwareness   = false;
		bAlertSharing        = false;
		bAlertShareImmediate = false;
		bInvestigate         = false;
		bCoverAttack         = false;
		bCanRetreatAndHeal   = false;
		bFindCoverBeforeHeal = false;
		bSpreadPositions     = false;
		AttackPhaseDuration  = 0.0f;
		HidePhaseDuration    = 0.0f;
		AlertShareInterval   = 0.0f;
		AlertShareCount      = 0;
		break;

	case ESFSoldierDifficulty::Medium:
		bRandomPatrol        = false;
		bFootstepAwareness   = true;
		bAlertSharing        = true;
		bAlertShareImmediate = false;
		bInvestigate         = true;
		bCoverAttack         = true;
		bCanRetreatAndHeal   = true;
		bFindCoverBeforeHeal = false;
		bSpreadPositions     = false;
		AttackPhaseDuration  = 3.0f;
		HidePhaseDuration    = 2.0f;
		AlertShareInterval   = 5.0f;
		AlertShareCount      = 1;
		break;

	case ESFSoldierDifficulty::Hard:
		bRandomPatrol        = false;
		bFootstepAwareness   = true;
		bAlertSharing        = true;
		bAlertShareImmediate = true;
		bInvestigate         = true;
		bCoverAttack         = true;
		bCanRetreatAndHeal   = true;
		bFindCoverBeforeHeal = true;
		bSpreadPositions     = true;
		AttackPhaseDuration  = 1.0f;
		HidePhaseDuration    = 1.0f;
		AlertShareInterval   = 3.0f;
		AlertShareCount      = 3;
		break;
	}

	AlertShareTimer = AlertShareInterval;
}

// ---------------------------------------------------------------------------
// State transition logic
// ---------------------------------------------------------------------------

void ASFSoldierController::CheckStateTransitions()
{
	ASFSoldierCharacter* Soldier = Cast<ASFSoldierCharacter>(GetPawn());
	if (!Soldier) return;

	FVector PlayerPos;
	bool bImmediate;
	bool bHasInfo = GetPlayerInfo(PlayerPos, bImmediate);

	// Awareness of this specific AI (per-AI, not the global target state)
	float IndividualAwareness = 0.0f;
	{
		FSFTargetState TS;
		FSFTargetView  TV;
		if (PerceptionComp->GetCurrentTargetState(TS, TV))
		{
			IndividualAwareness = TV.Awareness;
		}
	}

	const bool bAware   = IndividualAwareness >= AwarenessThreshold;
	const bool bLowHP   = Soldier->IsLowHealth();
	const float DistToPlayer = GetDistanceToPlayer();

	switch (CurrentState)
	{
	// -------- PATROL --------
	case ESFSoldierState::Patrol:
		if (bAware)
		{
			TransitionTo(ESFSoldierState::Chase);
		}
		break;

	// -------- CHASE --------
	case ESFSoldierState::Chase:
		if (!bAware)
		{
			// Lost track completely
			TransitionTo(ESFSoldierState::Patrol);
		}
		else if (bCanRetreatAndHeal && bLowHP)
		{
			TransitionTo(ESFSoldierState::Retreat);
		}
		else if (bHasInfo && DistToPlayer <= AttackRange)
		{
			TransitionTo(ESFSoldierState::Attack);
		}
		break;

	// -------- ATTACK --------
	case ESFSoldierState::Attack:
		if (!bAware)
		{
			// Lost all knowledge of player
			TransitionTo(bInvestigate ? ESFSoldierState::Investigate : ESFSoldierState::Patrol);
		}
		else if (bCanRetreatAndHeal && bLowHP)
		{
			TransitionTo(ESFSoldierState::Retreat);
		}
		else if (bHasInfo && DistToPlayer > AttackRange * 1.2f)
		{
			// Player moved out of range; chase again (1.2x hysteresis to avoid flicker)
			TransitionTo(ESFSoldierState::Chase);
		}
		break;

	// -------- INVESTIGATE --------
	case ESFSoldierState::Investigate:
		if (bImmediate)
		{
			TransitionTo(ESFSoldierState::Attack);
		}
		else if (PathComp->State == SFPS_Finished || PathComp->State == SFPS_Invalid)
		{
			// Arrived at last known position; resume patrol
			TransitionTo(ESFSoldierState::Patrol);
		}
		break;

	// -------- RETREAT --------
	case ESFSoldierState::Retreat:
		if (PathComp->State == SFPS_Finished || PathComp->State == SFPS_Invalid)
		{
			if (!bFindCoverBeforeHeal)
			{
				// Reached far enough from player?
				if (DistToPlayer >= RetreatDistance)
				{
					TransitionTo(ESFSoldierState::Heal);
				}
				else
				{
					// Recalculate a farther retreat point
					bRetreatPathSet = false;
				}
			}
			else
			{
				// Hard mode: arrived at cover – now heal
				TransitionTo(ESFSoldierState::Heal);
			}
		}
		break;

	// -------- HEAL --------
	case ESFSoldierState::Heal:
		// Only exit on full health (death is handled outside via OnDeath)
		if (Soldier->GetHealthPercent() >= 1.0f)
		{
			TransitionTo(ESFSoldierState::Patrol);
		}
		break;
	}
}

void ASFSoldierController::TransitionTo(ESFSoldierState NewState)
{
	if (NewState == CurrentState) return;

	// --- Exit old state ---
	switch (CurrentState)
	{
	case ESFSoldierState::Heal:
	{
		ASFSoldierCharacter* Soldier = Cast<ASFSoldierCharacter>(GetPawn());
		if (Soldier) Soldier->StopHealing();
		break;
	}
	default: break;
	}

	bCanShoot = false;
	CurrentState = NewState;

	// --- Enter new state ---
	switch (NewState)
	{
	case ESFSoldierState::Patrol:
		bWaypointInProgress = false;
		bAlertInitiated     = false;
		AlertShareTimer     = AlertShareInterval;
		break;

	case ESFSoldierState::Chase:
		// First detection: Hard mode fires the immediate alert burst
		if (bAlertSharing && bAlertShareImmediate && !bAlertInitiated)
		{
			bAlertInitiated = true;
			ShareAlert(AlertShareCount);
			AlertShareTimer = AlertShareInterval;
		}
		else if (bAlertSharing && !bAlertInitiated)
		{
			bAlertInitiated = true;
			AlertShareTimer = AlertShareInterval;
		}
		break;

	case ESFSoldierState::Attack:
		bHasCover          = false;
		CoverPhase         = ESFCoverCyclePhase::MovingToCover;
		CoverCycleTimer    = 0.0f;
		break;

	case ESFSoldierState::Investigate:
	{
		// Pathfind to the last known player position
		FVector PlayerPos;
		bool bImmediate;
		if (GetPlayerInfo(PlayerPos, bImmediate))
		{
			PathComp->SetDestination(PlayerPos);
		}
		break;
	}

	case ESFSoldierState::Retreat:
		bRetreatPathSet = false;
		break;

	case ESFSoldierState::Heal:
	{
		ASFSoldierCharacter* Soldier = Cast<ASFSoldierCharacter>(GetPawn());
		if (Soldier) Soldier->StartHealing();
		// Stop moving
		if (GetPawn())
		{
			GetPawn()->GetMovementComponent()->StopMovementImmediately();
		}
		break;
	}
	}
}

// ---------------------------------------------------------------------------
// Per-state updates
// ---------------------------------------------------------------------------

void ASFSoldierController::UpdatePatrol(float DeltaSeconds)
{
	bCanShoot = false;

	// If we have a path and are still following it, do nothing
	if (bWaypointInProgress && PathComp->State == SFPS_Active)
	{
		return;
	}

	// Reached waypoint or path finished/invalid – pick next destination
	bWaypointInProgress = false;

	FVector Destination;
	if (!bRandomPatrol && PatrolWaypoints.Num() > 0)
	{
		// Ordered waypoint cycle
		Destination = PatrolWaypoints[CurrentWaypointIndex];
		CurrentWaypointIndex = (CurrentWaypointIndex + 1) % PatrolWaypoints.Num();
	}
	else
	{
		Destination = GetRandomPatrolPoint();
	}

	ESFPathState PathState = PathComp->SetDestination(Destination);
	if (PathState != SFPS_Invalid)
	{
		bWaypointInProgress = true;
	}
}

void ASFSoldierController::UpdateChase(float DeltaSeconds)
{
	FVector PlayerPos;
	bool bImmediate;
	if (!GetPlayerInfo(PlayerPos, bImmediate)) return;

	// Keep shooting if we have direct LOS
	FSFTargetState TS;
	FSFTargetView  TV;
	PerceptionComp->GetCurrentTargetState(TS, TV);
	bCanShoot = TV.bClearLos;

	// Continuously update destination toward the player
	PathComp->SetDestination(PlayerPos);
}

void ASFSoldierController::UpdateAttack(float DeltaSeconds)
{
	FVector PlayerPos;
	bool bImmediate;
	if (!GetPlayerInfo(PlayerPos, bImmediate)) return;

	// Easy mode: simply move toward player and shoot when LOS available
	if (!bCoverAttack)
	{
		FSFTargetState TS;
		FSFTargetView  TV;
		PerceptionComp->GetCurrentTargetState(TS, TV);
		bCanShoot = TV.bClearLos;
		PathComp->SetDestination(PlayerPos);
		return;
	}

	// Medium / Hard: cover-based attack cycle
	// ---- Step 1: Find cover if we don't have it yet ----
	if (!bHasCover)
	{
		if (FindCoverAndAttackPositions(PlayerPos, CoverPosition, AttackPosition))
		{
			bHasCover  = true;
			CoverPhase = ESFCoverCyclePhase::MovingToCover;
			PathComp->SetDestination(CoverPosition);
		}
		else
		{
			// No cover found – fall back to simple attack
			FSFTargetState TS; FSFTargetView TV;
			PerceptionComp->GetCurrentTargetState(TS, TV);
			bCanShoot = TV.bClearLos;
			PathComp->SetDestination(PlayerPos);
		}
		return;
	}

	// ---- Step 2: Cover cycle ----
	switch (CoverPhase)
	{
	case ESFCoverCyclePhase::MovingToCover:
	{
		// Shoot if LOS available while approaching
		FSFTargetState TS; FSFTargetView TV;
		PerceptionComp->GetCurrentTargetState(TS, TV);
		bCanShoot = TV.bClearLos;

		// Near cover? Transition into the attack/hide cycle
		APawn* Pawn = GetPawn();
		if (Pawn && FVector::Dist2D(Pawn->GetActorLocation(), CoverPosition) <= 200.0f)
		{
			CoverPhase      = ESFCoverCyclePhase::Attacking;
			CoverCycleTimer = AttackPhaseDuration;
			PathComp->SetDestination(AttackPosition);
		}
		// Still approaching
		break;
	}

	case ESFCoverCyclePhase::Attacking:
	{
		bCanShoot = true;
		CoverCycleTimer -= DeltaSeconds;

		if (CoverCycleTimer <= 0.0f)
		{
			// Transition to hiding
			CoverPhase      = ESFCoverCyclePhase::Hiding;
			CoverCycleTimer = HidePhaseDuration;
			bCanShoot       = false;
			PathComp->SetDestination(CoverPosition);
		}
		break;
	}

	case ESFCoverCyclePhase::Hiding:
	{
		bCanShoot = false;
		CoverCycleTimer -= DeltaSeconds;

		if (CoverCycleTimer <= 0.0f)
		{
			// Back to attacking
			CoverPhase      = ESFCoverCyclePhase::Attacking;
			CoverCycleTimer = AttackPhaseDuration;
			bCanShoot       = true;

			// Refresh cover in case player has moved significantly
			FVector NewCover, NewAttack;
			if (FindCoverAndAttackPositions(PlayerPos, NewCover, NewAttack))
			{
				CoverPosition  = NewCover;
				AttackPosition = NewAttack;
			}
			PathComp->SetDestination(AttackPosition);
		}
		break;
	}
	}
}

void ASFSoldierController::UpdateInvestigate(float DeltaSeconds)
{
	bCanShoot = false;
	// Destination was already set in TransitionTo(Investigate).
	// CheckStateTransitions handles the exit when path finishes or target is re-acquired.
}

void ASFSoldierController::UpdateRetreat(float DeltaSeconds)
{
	bCanShoot = false;

	if (!bRetreatPathSet)
	{
		FVector PlayerPos;
		bool bImmediate;
		GetPlayerInfo(PlayerPos, bImmediate);

		RetreatDestination = FindRetreatDestination(PlayerPos);
		PathComp->SetDestination(RetreatDestination);
		bRetreatPathSet = true;
	}
}

void ASFSoldierController::UpdateHeal(float DeltaSeconds)
{
	bCanShoot = false;
	// Healing happens passively via SFSoldierCharacter::Tick.
	// CheckStateTransitions handles the exit on full health.
}

// ---------------------------------------------------------------------------
// Footstep awareness
// ---------------------------------------------------------------------------

void ASFSoldierController::UpdateFootstepAwareness(float DeltaSeconds)
{
	APawn* Pawn = GetPawn();
	if (!Pawn) return;

	USFPerceptionSystem* PerSys = USFPerceptionSystem::GetPerceptionSystem(this);
	if (!PerSys) return;

	FVector SoldierPos = Pawn->GetActorLocation();

	for (USFTargetComponent* Target : PerSys->GetAllTargetComponents())
	{
		if (!Target) continue;
		AActor* TargetActor = Target->GetOwner();
		if (!TargetActor) continue;

		float Dist = FVector::Dist(SoldierPos, TargetActor->GetActorLocation());
		if (Dist > FootstepRadius) continue;

		// Is the target moving?
		if (TargetActor->GetVelocity().SizeSquared() > 1000.0f) // ~32 cm/s threshold
		{
			PerceptionComp->AddAwareness(Target->TargetGuid, FootstepAwarenessRate * DeltaSeconds);
		}
	}
}

// ---------------------------------------------------------------------------
// Alert sharing
// ---------------------------------------------------------------------------

void ASFSoldierController::ShareAlert(int32 Count)
{
	if (Count <= 0) return;

	USFTargetComponent* Target = PerceptionComp->GetCurrentTarget();
	if (!Target) return;

	// Collect unaware soldiers sorted by distance
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;
	FVector MyPos = MyPawn->GetActorLocation();

	TArray<TPair<float, ASFSoldierController*>> Candidates;

	for (TActorIterator<ASFSoldierController> It(GetWorld()); It; ++It)
	{
		ASFSoldierController* Other = *It;
		if (Other == this || !Other->GetPawn() || !Other->PerceptionComp) continue;

		const FSFTargetView* TheirView = Other->PerceptionComp->GetTargetView(Target->TargetGuid);
		float TheirAwareness = TheirView ? TheirView->Awareness : 0.0f;

		if (TheirAwareness < AwarenessThreshold)
		{
			float Dist = FVector::Dist(Other->GetPawn()->GetActorLocation(), MyPos);
			Candidates.Emplace(Dist, Other);
		}
	}

	Candidates.Sort([](const TPair<float, ASFSoldierController*>& A,
	                   const TPair<float, ASFSoldierController*>& B) {
		return A.Key < B.Key;
	});

	int32 Shared = 0;
	for (auto& Pair : Candidates)
	{
		if (Shared >= Count) break;
		Pair.Value->ReceiveAlertMessage(Target->TargetGuid, 1.0f);
		++Shared;
	}
}

void ASFSoldierController::ReceiveAlertMessage(FGuid TargetGuid, float AwarenessValue)
{
	PerceptionComp->SetAwareness(TargetGuid, AwarenessValue);
}

// ---------------------------------------------------------------------------
// Spatial helpers
// ---------------------------------------------------------------------------

ASFGridActor* ASFSoldierController::GetGrid() const
{
	if (!CachedGrid.IsValid())
	{
		CachedGrid = Cast<ASFGridActor>(
			UGameplayStatics::GetActorOfClass(GetWorld(), ASFGridActor::StaticClass()));
	}
	return CachedGrid.Get();
}

FVector ASFSoldierController::GetRandomPatrolPoint() const
{
	ASFGridActor* Grid = GetGrid();
	if (!Grid) return GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;

	for (int32 i = 0; i < 50; ++i)
	{
		FSFCellRef Cell(FMath::RandRange(0, Grid->XCount - 1),
		              FMath::RandRange(0, Grid->YCount - 1));
		if (EnumHasAnyFlags(Grid->GetCellData(Cell), ESFCellData::CellDataTraversable))
		{
			return Grid->GetCellPosition(Cell);
		}
	}
	return GetPawn()->GetActorLocation();
}

bool ASFSoldierController::FindCoverAndAttackPositions(
	FVector PlayerPos, FVector& OutCoverPos, FVector& OutAttackPos) const
{
	ASFGridActor* Grid = GetGrid();
	APawn*        Pawn = GetPawn();
	if (!Grid || !Pawn) return false;

	FVector SoldierPos = Pawn->GetActorLocation();
	FSFCellRef SoldierCell = Grid->GetCellRef(SoldierPos);
	int32 CellRadius = FMath::CeilToInt(CoverSearchRadius / Grid->CellScale);

	FCollisionQueryParams Params(NAME_None, false, Pawn);
	FVector PlayerEye = PlayerPos + FVector(0, 0, 60.0f);

	// Collect candidate cover cells sorted by distance
	struct FCoverCandidate
	{
		FVector  CoverPos;
		FVector  AttackPos;
		float    DistToSoldier;
	};
	TArray<FCoverCandidate> Candidates;

	for (int32 dx = -CellRadius; dx <= CellRadius; ++dx)
	{
		for (int32 dy = -CellRadius; dy <= CellRadius; ++dy)
		{
			FSFCellRef TestCell(SoldierCell.X + dx, SoldierCell.Y + dy);
			if (!Grid->IsCellRefInBounds(TestCell)) continue;
			if (!EnumHasAnyFlags(Grid->GetCellData(TestCell), ESFCellData::CellDataTraversable)) continue;

			FVector TestPos = Grid->GetCellPosition(TestCell);
			float Dist = FVector::Dist2D(TestPos, SoldierPos);
			if (Dist > CoverSearchRadius) continue;

			// Check player LOS to this cell
			FHitResult Hit;
			bool bPlayerSees = !GetWorld()->LineTraceSingleByChannel(
				Hit, PlayerEye, TestPos + FVector(0, 0, 60.0f), ECC_Visibility, Params);
			if (bPlayerSees) continue; // not a cover cell

			// Find an adjacent cell from which the soldier can attack (player LOS exists)
			FVector AttackCellPos = FVector::ZeroVector;
			bool bFoundAttack = false;
			const int32 Adj[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
			for (auto& N : Adj)
			{
				FSFCellRef AdjCell(TestCell.X + N[0], TestCell.Y + N[1]);
				if (!Grid->IsCellRefInBounds(AdjCell)) continue;
				if (!EnumHasAnyFlags(Grid->GetCellData(AdjCell), ESFCellData::CellDataTraversable)) continue;

				FVector AdjPos = Grid->GetCellPosition(AdjCell);
				FHitResult AdjHit;
				bool bAdjLOS = !GetWorld()->LineTraceSingleByChannel(
					AdjHit, PlayerEye, AdjPos + FVector(0, 0, 60.0f), ECC_Visibility, Params);
				if (bAdjLOS)
				{
					AttackCellPos = AdjPos;
					bFoundAttack  = true;
					break;
				}
			}

			if (bFoundAttack)
			{
				Candidates.Add({ TestPos, AttackCellPos, Dist });
			}
		}
	}

	// Sort by distance (closest first)
	Candidates.Sort([](const FCoverCandidate& A, const FCoverCandidate& B) {
		return A.DistToSoldier < B.DistToSoldier;
	});

	// Pick the first candidate not already crowded (Hard mode spreading)
	for (const FCoverCandidate& C : Candidates)
	{
		if (bSpreadPositions && CountAIAtPosition(C.AttackPos) >= MaxAIPerAttackPosition)
		{
			continue;
		}
		OutCoverPos  = C.CoverPos;
		OutAttackPos = C.AttackPos;
		return true;
	}

	return false;
}

FVector ASFSoldierController::FindRetreatDestination(FVector PlayerPos) const
{
	ASFGridActor* Grid = GetGrid();
	APawn*        Pawn = GetPawn();
	if (!Grid || !Pawn) return Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;

	FVector SoldierPos = Pawn->GetActorLocation();
	FSFCellRef SoldierCell = Grid->GetCellRef(SoldierPos);
	// Search a wide area
	int32 CellRadius = FMath::CeilToInt(RetreatDistance * 1.5f / Grid->CellScale);

	FCollisionQueryParams Params(NAME_None, false, Pawn);
	FVector PlayerEye = PlayerPos + FVector(0, 0, 60.0f);

	FVector BestPos = SoldierPos;
	float   BestDist = 0.0f;

	for (int32 dx = -CellRadius; dx <= CellRadius; dx += 2)
	{
		for (int32 dy = -CellRadius; dy <= CellRadius; dy += 2)
		{
			FSFCellRef TestCell(SoldierCell.X + dx, SoldierCell.Y + dy);
			if (!Grid->IsCellRefInBounds(TestCell)) continue;
			if (!EnumHasAnyFlags(Grid->GetCellData(TestCell), ESFCellData::CellDataTraversable)) continue;

			FVector TestPos = Grid->GetCellPosition(TestCell);
			float DistToPlayer = FVector::Dist2D(TestPos, PlayerPos);

			if (DistToPlayer < RetreatDistance) continue;

			// Hard mode: also require cover (no player LOS)
			if (bFindCoverBeforeHeal)
			{
				FHitResult Hit;
				bool bPlayerSees = !GetWorld()->LineTraceSingleByChannel(
					Hit, PlayerEye, TestPos + FVector(0, 0, 60.0f), ECC_Visibility, Params);
				if (bPlayerSees) continue;
			}

			if (DistToPlayer > BestDist)
			{
				BestDist = DistToPlayer;
				BestPos  = TestPos;
			}
		}
	}

	return BestPos;
}

int32 ASFSoldierController::CountAIAtPosition(FVector Position) const
{
	int32 Count = 0;
	for (TActorIterator<ASFSoldierController> It(GetWorld()); It; ++It)
	{
		ASFSoldierController* Other = *It;
		if (Other == this || !Other->GetPawn()) continue;

		// Count AIs currently near the position
		if (FVector::Dist2D(Other->GetPawn()->GetActorLocation(), Position) < PositionSpreadRadius)
		{
			++Count;
			continue;
		}
		// Count AIs heading toward the position
		if (Other->PathComp && Other->PathComp->bDestinationValid &&
			FVector::Dist2D(Other->PathComp->Destination, Position) < PositionSpreadRadius)
		{
			++Count;
		}
	}
	return Count;
}

bool ASFSoldierController::GetPlayerInfo(FVector& OutPosition, bool& OutImmediate) const
{
	FSFTargetState TS;
	FSFTargetView  TV;
	if (!PerceptionComp->GetCurrentTargetState(TS, TV)) return false;
	if (TS.State == SFTS_Unknown) return false;

	OutPosition  = TS.Position;
	OutImmediate = (TS.State == SFTS_Immediate);
	return true;
}

float ASFSoldierController::GetDistanceToPlayer() const
{
	APawn* Pawn = GetPawn();
	if (!Pawn) return MAX_FLT;

	FVector PlayerPos;
	bool bImmediate;
	if (!GetPlayerInfo(PlayerPos, bImmediate)) return MAX_FLT;

	return FVector::Dist2D(Pawn->GetActorLocation(), PlayerPos);
}
