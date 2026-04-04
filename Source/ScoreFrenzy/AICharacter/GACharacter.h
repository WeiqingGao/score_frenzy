// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "../AILogic/Patrol/ScoutPatrolTypes.h"
#include "Logging/LogMacros.h"
#include "GACharacter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateAICharacter, Log, All);

UENUM(BlueprintType)
enum class EDifficultyMode : uint8
{
	Easy    UMETA(DisplayName = "Easy"),
	Medium  UMETA(DisplayName = "Medium"),
	Hard    UMETA(DisplayName = "Hard"),
};

UENUM(BlueprintType)
enum class EHealingState : uint8
{
	Normal      UMETA(DisplayName = "Normal"),      // Normal combat / patrol mode
	Retreating  UMETA(DisplayName = "Retreating"),  // Moving away from player before healing
	Healing     UMETA(DisplayName = "Healing"),     // Regenerating health out of combat
};

// The base class for our AI characters in CS 4150/5150
// Note the use of the UClass specifiers:
//    BlueprintType - you can use this class as the type for a variable in Blueprint
//    Blueprintable - you can create subclasses of this class in Blueprint
// For the full list of UClass specifiers, see https://docs.unrealengine.com/5.3/en-US/class-specifiers/

UCLASS(BlueprintType, Blueprintable, config=Game)
class AGACharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGACharacter();

	// Initial movement frequency and amplitude
	// Note the use of the UProperty specifiers:
	//    EditAnywhere - In any AGACharacter subclasses, you can override this value from the editor
	//    BlueprintReadWrite - You can read and write the value of the member variable from Blueprint
	//    ClampMin/ ClampMax - Specifies the min and max allowable values for this property (integers and floats only)
	// For the full list of UProperty specifiers, see https://docs.unrealengine.com/5.3/en-US/unreal-engine-uproperties/#propertyspecifiers

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveFrequency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = -1.0, ClampMax = 1.0f))
	float MoveAmplitude;

	// --- Health & Healing System ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0f))
	float MaxHealth;

	UPROPERTY(BlueprintReadWrite)
	float Health;

	// Health fraction (0–1) at or below which the AI considers retreating to heal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float LowHealthThreshold;

	// Health points restored per second while in the Healing state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0f))
	float HealRate;

	// True while the AI is actively fighting; suppresses healing even in Healing state
	UPROPERTY(BlueprintReadWrite)
	bool bInCombat;

	UPROPERTY(BlueprintReadOnly)
	EHealingState HealingState;

	// Enter Healing state. Also clears bInCombat.
	UFUNCTION(BlueprintCallable)
	void EnterHealingState();

	// Return to Normal state. Called automatically on full health, or externally on death.
	UFUNCTION(BlueprintCallable)
	void ExitHealingState();

	// External setter for bInCombat (called from BT services).
	UFUNCTION(BlueprintCallable)
	void SetInCombat(bool bCombat);

	// Returns true when Health / MaxHealth <= LowHealthThreshold.
	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsLowHealth() const;
	
	// Scout Patrol Branch
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol")
	TArray<FScoutPatrolPoint> PatrolPoints;

	UPROPERTY(BlueprintReadWrite, Category="Patrol")
	int32 CurrentPatrolIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category="Patrol")
	bool bPatrolInitialized = false;

	// Group Behavior - Share Player's Position
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group")
	int32 SquadID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group")
	bool bIsLeader = false;
	
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

};

