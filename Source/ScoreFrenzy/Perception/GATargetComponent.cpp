#include "GATargetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ScoreFrenzy/Grid/GAGridActor.h"
#include "GAPerceptionSystem.h"
#include "ProceduralMeshComponent.h"



UGATargetComponent::UGATargetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;

	SetTickGroup(ETickingGroup::TG_PostUpdateWork);

	// Generate a new guid
	TargetGuid = FGuid::NewGuid();
}


AGAGridActor* UGATargetComponent::GetGridActor() const
{
	AGAGridActor* Result = GridActor.Get();
	if (Result)
	{
		return Result;
	}
	else
	{
		AActor* GenericResult = UGameplayStatics::GetActorOfClass(this, AGAGridActor::StaticClass());
		if (GenericResult)
		{
			Result = Cast<AGAGridActor>(GenericResult);
			if (Result)
			{
				// Cache the result
				// Note, GridActor is marked as mutable in the header, which is why this is allowed in a const method
				GridActor = Result;
			}
		}

		return Result;
	}
}


void UGATargetComponent::OnRegister()
{
	Super::OnRegister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->RegisterTargetComponent(this);
	}

	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		OccupancyMap = FGAGridMap(Grid, 0.0f);
	}
}

void UGATargetComponent::OnUnregister()
{
	Super::OnUnregister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->UnregisterTargetComponent(this);
	}
}



void UGATargetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool isImmediate = false;

	// update my perception state FSM
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGAPerceptionComponent>> &PerceptionComponents = PerceptionSystem->GetAllPerceptionComponents();
		for (UGAPerceptionComponent* PerceptionComponent : PerceptionComponents)
		{
			const FTargetView* TargetView = PerceptionComponent->GetTargetView(TargetGuid);
			if (TargetView && (TargetView->Awareness >= 1.0f))
			{
				isImmediate = true;
				break;
			}
		}
	}

	if (isImmediate)
	{
		AActor* Owner = GetOwner();
		LastKnownState.State = GATS_Immediate;

		// REFRESH MY STATE
		LastKnownState.Set(Owner->GetActorLocation(), Owner->GetVelocity());

		// Tell the omap to clear out and put all the probability in the observed location
		OccupancyMapSetPosition(LastKnownState.Position);
	}
	else if (IsKnown())
	{
		LastKnownState.State = GATS_Hidden;
	}

	if (LastKnownState.State == GATS_Hidden)
	{
		OccupancyMapUpdate();
	}

	// As long as I'm known, whether I'm immediate or not, diffuse the probability in the omap

	if (IsKnown())
	{
		OccupancyMapDiffuse();
	}

	if (bDebugOccupancyMap)
	{
		AGAGridActor* Grid = GetGridActor();
		Grid->DebugGridMap = OccupancyMap;
		GridActor->RefreshDebugTexture();
		GridActor->DebugMeshComponent->SetVisibility(true);
	}
}


void UGATargetComponent::OccupancyMapSetPosition(const FVector& Position)
{
	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		OccupancyMap = FGAGridMap(Grid, 0.0f);

		FCellRef TargetPositionInCellRef = Grid->GetCellRef(Position, true);
		if (TargetPositionInCellRef.IsValid() && Grid->IsCellRefInBounds(TargetPositionInCellRef))
		{
			OccupancyMap.SetValue(TargetPositionInCellRef, 1.0f);
		}
	}
}


void UGATargetComponent::OccupancyMapUpdate()
{
	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		FGAGridMap VisibilityMap(Grid, 0.0f);

		// STEP 1: Build visibility map
		UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
		if (PerceptionSystem)
		{
			TArray<TObjectPtr<UGAPerceptionComponent>>& PerceptionComponents = PerceptionSystem->GetAllPerceptionComponents();
			for (UGAPerceptionComponent* PerceptionComponent : PerceptionComponents)
			{
				APawn* Pawn = PerceptionComponent->GetOwnerPawn();
				if (!Pawn) continue;

				FVector RaycastStartLocation;
				FRotator ViewRotation;
				Pawn->GetActorEyesViewPoint(RaycastStartLocation, ViewRotation);

				const float VisionDistance = PerceptionComponent->VisionParameters.VisionDistance;
				const FCellRef Center = Grid->GetCellRef(RaycastStartLocation, /*bClamp=*/true);
				const int32 Radius = FMath::CeilToInt(VisionDistance / Grid->CellScale);

				for (int32 y = Center.Y - Radius; y <= (Center.Y + Radius); ++y)
				{
					for (int32 x = Center.X - Radius; x <= (Center.X + Radius); ++x)
					{
						const FCellRef Cell(x, y);
						if (!Grid->IsCellRefInBounds(Cell)) continue;

						const FVector CellLocation = Grid->GetCellPosition(Cell);
						float Distance = FVector::Distance(CellLocation, RaycastStartLocation);
						if ((Distance - Grid->CellScale / 2) > VisionDistance) continue;

						FVector ForwardVector = Pawn->GetActorForwardVector();
						FVector PerceiverToCell = CellLocation - RaycastStartLocation;
						ForwardVector.Z = 0.0f; ForwardVector.Normalize();
						PerceiverToCell.Z = 0.0f; PerceiverToCell.Normalize();
						float CosineAngle = FVector::DotProduct(ForwardVector, PerceiverToCell.GetSafeNormal());
						float CosineHalfAngle = FMath::Cos(FMath::DegreesToRadians(PerceptionComponent->VisionParameters.PeripheralVisionAngle * 0.5f));
						if (CosineAngle < CosineHalfAngle) continue;

						if (!PerceptionComponent->TestVisibility(Cell)) continue;

						VisibilityMap.SetValue(Cell, 1.0f);
					}
				}
			}
		}

		// STEP 2: Clear out probability in visible cells
		float CulledProbability = 0.0f;
		int32 MinX = OccupancyMap.GridBounds.MinX;
		int32 MaxX = OccupancyMap.GridBounds.MaxX;
		int32 MinY = OccupancyMap.GridBounds.MinY;
		int32 MaxY = OccupancyMap.GridBounds.MaxY;

		for (int32 y = MinY; y <= MaxY; ++y)
		{
			for (int32 x = MinX; x <= MaxX; ++x)
			{
				const FCellRef Cell(x, y);
				float Vis = 0.0f;
				VisibilityMap.GetValue(Cell, Vis);
				if (Vis == 1.0f)
				{
					float P = 0.0f;
					OccupancyMap.GetValue(Cell, P);
					CulledProbability += P;
					OccupancyMap.SetValue(Cell, 0.0f);
				}
			}
		}

		// STEP 3: Renormalize
		if (CulledProbability < 1.0f)
		{
			for (int32 y = MinY; y <= MaxY; ++y)
			{
				for (int32 x = MinX; x <= MaxX; ++x)
				{
					const FCellRef Cell(x, y);
					float Vis = 0.0f;
					VisibilityMap.GetValue(Cell, Vis);
					if (Vis == 0.0f)
					{
						float P = 0.0f;
						OccupancyMap.GetValue(Cell, P);
						OccupancyMap.SetValue(Cell, P / (1.0f - CulledProbability));
					}
				}
			}
		}

		// STEP 4: Extract highest-likelihood cell and refresh LastKnownState
		float MaxProbability = 0.0f;
		FCellRef MaxProbabilityCell;
		for (int32 y = MinY; y <= MaxY; ++y)
		{
			for (int32 x = MinX; x <= MaxX; ++x)
			{
				const FCellRef Cell(x, y);
				if (!Grid->IsCellRefInBounds(Cell)) continue;
				if (!EnumHasAnyFlags(Grid->GetCellData(Cell), ECellData::CellDataTraversable)) continue;
				float P = 0.0f;
				OccupancyMap.GetValue(Cell, P);
				if (P > MaxProbability)
				{
					MaxProbability = P;
					MaxProbabilityCell = Cell;
				}
			}
		}

		FVector BestPos = Grid->GetCellPosition(MaxProbabilityCell);
		LastKnownState.Set(BestPos, LastKnownState.Velocity);
	}
}


void UGATargetComponent::OccupancyMapDiffuse()
{
	const AGAGridActor* Grid = GetGridActor();
	if (!Grid) return;

	FGAGridMap Buffer = FGAGridMap(Grid, 0.0f);

	int32 MinX = OccupancyMap.GridBounds.MinX;
	int32 MaxX = OccupancyMap.GridBounds.MaxX;
	int32 MinY = OccupancyMap.GridBounds.MinY;
	int32 MaxY = OccupancyMap.GridBounds.MaxY;

	const float Alpha = 0.01f;

	for (int32 y = MinY; y <= MaxY; ++y)
	{
		for (int32 x = MinX; x <= MaxX; ++x)
		{
			const FCellRef Cell(x, y);
			if (!Grid->IsCellRefInBounds(Cell)) continue;

			float CellProbability = 0.0f;
			OccupancyMap.GetValue(Cell, CellProbability);
			if (CellProbability == 0.0f) continue;

			float LeftProbability = CellProbability;

			for (int32 CurrY = y - 1; CurrY <= y + 1; ++CurrY)
			{
				for (int32 CurrX = x - 1; CurrX <= x + 1; ++CurrX)
				{
					if (CurrX == x && CurrY == y) continue;

					FCellRef Neighbor(CurrX, CurrY);
					if (!Grid->IsCellRefInBounds(Neighbor)) continue;
					if (!EnumHasAnyFlags(Grid->GetCellData(Neighbor), ECellData::CellDataTraversable)) continue;

					float ProbabilityDiffusion;
					if ((CurrY - y + CurrX - x) % 2 == 0)
					{
						ProbabilityDiffusion = Alpha * LeftProbability / FMath::Sqrt(2.0f);
					}
					else
					{
						ProbabilityDiffusion = Alpha * LeftProbability;
					}
					float NeighborP = 0.0f;
					Buffer.GetValue(Neighbor, NeighborP);
					Buffer.SetValue(Neighbor, NeighborP + ProbabilityDiffusion);
					LeftProbability -= ProbabilityDiffusion;
				}
			}

			float CellP = 0.0f;
			Buffer.GetValue(Cell, CellP);
			Buffer.SetValue(Cell, CellP + LeftProbability);
		}
	}

	OccupancyMap = Buffer;
}
