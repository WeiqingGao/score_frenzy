#pragma once

#include "CoreMinimal.h"
#include "LeaderCombatStates.generated.h"

UENUM(BlueprintType)
enum class ELeaderCombatStates : uint8
{
	Idle        UMETA(DisplayName="Idle"),
	FindCover   UMETA(DisplayName="FindCover"),
	MoveToCover UMETA(DisplayName="MoveToCover"),
	Hide        UMETA(DisplayName="Hide"),
	PeekShoot   UMETA(DisplayName="PeekShoot")
};