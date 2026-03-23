#pragma once

#include "CoreMinimal.h"
#include "GASpatialFunction.h"
#include "GASpatialFunction_Cover.generated.h"


// A spatial function preset for finding cover positions relative to the player.
//
// Usage:
//   1. Create a Blueprint subclass of GASpatialFunction_Cover.
//   2. Add one or more FFunctionLayer entries with Input = SI_CoverFromPlayer.
//   3. Assign the Blueprint class to GASpatialComponent::SpatialFunctionReference.
//
// SI_CoverFromPlayer scores a cell 1.0 when the player's LOS to that cell is blocked
// (good cover) and 0.0 when it is clear.  Set the response curve accordingly.
//
// MinCoverDistance / MaxCoverDistance let you restrict the search to a sensible engagement
// range.  bRequireLOSToPlayer enables peek-and-shoot cover (the cell must still have a
// clear sightline to the player so the AI can fire while in cover).

UCLASS(BlueprintType, Blueprintable)
class UGASpatialFunction_Cover : public UGASpatialFunction
{
	GENERATED_UCLASS_BODY()

	// Cells closer than this distance to the player are excluded from cover candidates.
	// Prevents the AI from hiding directly beside the player.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (ClampMin = 0.0f))
	float MinCoverDistance;

	// Cells farther than this distance from the player are excluded from cover candidates.
	// Keeps the AI within a relevant engagement range.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (ClampMin = 0.0f))
	float MaxCoverDistance;

	// If true, a cover candidate must still have a clear LOS to the player so the AI can
	// shoot from cover.  If false, the AI will fully hide behind the obstacle.
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bRequireLOSToPlayer;
};
