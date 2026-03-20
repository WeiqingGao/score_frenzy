#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ScoreFrenzy/Grid/SFGridActor.h"
#include "Curves/CurveFloat.h"
#include "SFSpatialFunction.generated.h"


UENUM(BlueprintType)
enum ESFSpatialInput
{
	SI_None				UMETA(DisplayName = "None"),
	SI_TargetRange		UMETA(DisplayName = "Target Range"),
	SI_PathDistance		UMETA(DisplayName = "PathDistance"),
	SI_LOS				UMETA(DisplayName = "Line Of Sight")
	// Add others if you want!
};

UENUM(BlueprintType)
enum ESFSpatialOp
{
	SO_None				UMETA(DisplayName = "None"),
	SO_Add				UMETA(DisplayName = "Add"),			// add this layer to the accumulated buffer
	SO_Multiply			UMETA(DisplayName = "Multiply")		// multiply this layer into the accumulated buffer
	// Add others if you want!
};


// A single layer in our spatial function
// In our simple model, we keep a single buffer (a GridMap) that accumulates the values from each 
// subsequent using an ESFSpatialOp -- at first this is just addition or multiplication. So you can
// express spatial functions of the form 
// Output = (((((I0 + I1) * I2) + I3) * I4) * I5)

USTRUCT(BlueprintType)
struct FSFFunctionLayer
{
	GENERATED_USTRUCT_BODY()

	FSFFunctionLayer() : Input(SI_None), Op(SO_None) {}

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TEnumAsByte<ESFSpatialInput> Input;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FRuntimeFloatCurve ResponseCurve;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TEnumAsByte<ESFSpatialOp> Op;

};


// A spatial function is a description of how to combine various inputs (line of sight, distance, path-distance, etc.) 
// in order to rank an individual location where an AI might want to stand

UCLASS(BlueprintType, Blueprintable)
class USFSpatialFunction: public UObject
{
	GENERATED_UCLASS_BODY()

	// Our list of layers
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FSFFunctionLayer> Layers;
};