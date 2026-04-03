#pragma once

#include "CoreMinimal.h"
#include "ScoutPatrolTypes.generated.h"

USTRUCT(BlueprintType)
struct FScoutPatrolPoint
{
	GENERATED_BODY()

	// patrol position
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location;

	// facing direction after finishing scanning
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator ExitFacing;

	// pausing seconds for each direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PauseForward = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PauseRight = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PauseBackward = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PauseLeft = 0.5f;
};