#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAPerceptionComponent.h"
#include "GATargetComponent.h"
#include "GAPerceptionSystem.generated.h"



UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class UGAPerceptionSystem : public UActorComponent
{
	GENERATED_UCLASS_BODY()


	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<UGAPerceptionComponent>> PerceptionComponents;

	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<UGATargetComponent>> TargetComponents;


	bool RegisterPerceptionComponent(UGAPerceptionComponent* PerceptionComponent);
	bool UnregisterPerceptionComponent(UGAPerceptionComponent* PerceptionComponent);

	bool RegisterTargetComponent(UGATargetComponent* TargetComponent);
	bool UnregisterTargetComponent(UGATargetComponent* TargetComponent);

	TArray<TObjectPtr<UGATargetComponent>>& GetAllTargetComponents() { return TargetComponents; }
	TArray<TObjectPtr<UGAPerceptionComponent>>& GetAllPerceptionComponents() { return PerceptionComponents; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Perception", meta=(WorldContext="WorldContextObject"))
	static UGAPerceptionSystem* GetPerceptionSystem(const UObject* WorldContextObject);

	// --- Alert Propagation ---

	// Inform the nearest Count unalerted AIs within Radius of Source about the player's position.
	// Calls OnAlertReceived on each selected PerceptionComponent.
	// Returns true if at least one unalerted AI still exists (used by the Hard-mode timer to know
	// whether to keep broadcasting).
	UFUNCTION(BlueprintCallable)
	bool PropagateAlertToNearest(AActor* Source, FVector PlayerPosition, int32 Count, float Radius);

	// --- Attack Position Tracking (Hard Mode) ---

	// Register that AI is moving toward / standing at a specific world attack position.
	// Re-registering the same AI simply updates its position.
	UFUNCTION(BlueprintCallable)
	void RegisterAttackPosition(AActor* AI, FVector Position);

	// Remove an AI's registered attack position (on position change, retreat, or death).
	UFUNCTION(BlueprintCallable)
	void UnregisterAttackPosition(AActor* AI);

	// Returns how many AIs are currently registered within Radius of the given world position.
	UFUNCTION(BlueprintCallable)
	int32 CountAIsAtPosition(FVector Position, float Radius) const;

private:
	// Maps each attacking AI to its current target world position.
	// TWeakObjectPtr ensures destroyed actors are handled safely.
	TMap<TWeakObjectPtr<AActor>, FVector> AttackPositions;

};