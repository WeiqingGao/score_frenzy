#include "GAPerceptionSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"

UGAPerceptionSystem::UGAPerceptionSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{


}


bool UGAPerceptionSystem::RegisterPerceptionComponent(UGAPerceptionComponent* PerceptionComponent)
{
	PerceptionComponents.AddUnique(PerceptionComponent);
	return true;
}

bool UGAPerceptionSystem::UnregisterPerceptionComponent(UGAPerceptionComponent* PerceptionComponent)
{
	return PerceptionComponents.Remove(PerceptionComponent) > 0;
}


bool UGAPerceptionSystem::RegisterTargetComponent(UGATargetComponent* TargetComponent)
{
	TargetComponents.AddUnique(TargetComponent);
	return true;
}

bool UGAPerceptionSystem::UnregisterTargetComponent(UGATargetComponent* TargetComponent)
{
	return TargetComponents.Remove(TargetComponent) > 0;
}


UGAPerceptionSystem* UGAPerceptionSystem::GetPerceptionSystem(const UObject* WorldContextObject)
{
	UGAPerceptionSystem* Result = NULL;
	AGameModeBase *GameMode = UGameplayStatics::GetGameMode(WorldContextObject);
	if (GameMode)
	{
		Result = GameMode->GetComponentByClass<UGAPerceptionSystem>();
	}

	return Result;
}


bool UGAPerceptionSystem::PropagateAlertToNearest(AActor* Source, FVector PlayerPosition, int32 Count, float Radius)
{
	if (!Source || Count <= 0)
	{
		return false;
	}

	// Resolve the source pawn so we can exclude it from candidates
	APawn* SourcePawn = Cast<APawn>(Source);
	if (!SourcePawn)
	{
		AController* SourceController = Cast<AController>(Source);
		if (SourceController)
		{
			SourcePawn = SourceController->GetPawn();
		}
	}

	const float RadiusSq = Radius * Radius;
	const FVector SourceLocation = Source->GetActorLocation();

	// Collect unalerted perception components within radius, sorted by distance
	TArray<TPair<float, UGAPerceptionComponent*>> Candidates;
	for (UGAPerceptionComponent* PC : PerceptionComponents)
	{
		if (!PC || PC->bIsAlerted)
		{
			continue;
		}
		APawn* Pawn = PC->GetOwnerPawn();
		if (!Pawn || Pawn == SourcePawn)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), SourceLocation);
		if (DistSq <= RadiusSq)
		{
			Candidates.Add(TPair<float, UGAPerceptionComponent*>(DistSq, PC));
		}
	}

	Candidates.Sort([](const TPair<float, UGAPerceptionComponent*>& A,
	                   const TPair<float, UGAPerceptionComponent*>& B)
	{
		return A.Key < B.Key;
	});

	int32 Alerted = 0;
	for (auto& Pair : Candidates)
	{
		if (Alerted >= Count)
		{
			break;
		}
		Pair.Value->bIsAlerted = true;
		Pair.Value->OnAlertReceived(PlayerPosition);
		++Alerted;
	}

	// Return true if any unalerted AI still exists anywhere (not just in radius)
	for (const UGAPerceptionComponent* PC : PerceptionComponents)
	{
		if (PC && !PC->bIsAlerted)
		{
			return true;
		}
	}
	return false;
}


void UGAPerceptionSystem::RegisterAttackPosition(AActor* AI, FVector Position)
{
	if (AI)
	{
		AttackPositions.Add(TWeakObjectPtr<AActor>(AI), Position);
	}
}

void UGAPerceptionSystem::UnregisterAttackPosition(AActor* AI)
{
	if (AI)
	{
		AttackPositions.Remove(TWeakObjectPtr<AActor>(AI));
	}
}

int32 UGAPerceptionSystem::CountAIsAtPosition(FVector Position, float Radius) const
{
	int32 Count = 0;
	const float RadiusSq = Radius * Radius;
	for (const auto& Pair : AttackPositions)
	{
		if (Pair.Key.IsValid() && FVector::DistSquared(Pair.Value, Position) <= RadiusSq)
		{
			++Count;
		}
	}
	return Count;
}
