#pragma once

#include "CoreMinimal.h"
#include "ScoreFrenzy/AICharacter/SFCharacter.h"
#include "SFSoldierCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSFSoldierDeath);

/**
 * Base character class for Soldier AI. Adds health, damage, and healing on top of GACharacter.
 * The state machine that drives behavior lives in ASFSoldierController.
 */
UCLASS(BlueprintType, Blueprintable, config = Game)
class SCOREFRENZY_API ASFSoldierCharacter : public ASFCharacter
{
	GENERATED_BODY()

public:
	ASFSoldierCharacter();

	// --- Health ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.0f;

	// HP per second while healing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HealRate = 10.0f;

	// Fraction of MaxHealth below which the soldier considers itself "low health"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float LowHealthThreshold = 0.3f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnSFSoldierDeath OnDeath;

	UFUNCTION(BlueprintCallable, Category = "Health")
	bool IsLowHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Health")
	bool IsDead() const;

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealthPercent() const;

	// Call from the controller to start/stop passive healing each Tick
	UFUNCTION(BlueprintCallable, Category = "Health")
	void StartHealing();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void StopHealing();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	bool IsHealing() const { return bIsHealing; }

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	bool bIsHealing = false;
};
