#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ScoreFrenzy/Grid/SFGridActor.h"
#include "SFPathComponent.generated.h"



USTRUCT(BlueprintType)
struct FSFPathStep
{
	GENERATED_USTRUCT_BODY()

	FSFPathStep() : Point(FVector::ZeroVector), CellRef(FSFCellRef::Invalid) {}

	void Set(const FVector& PointIn, const FSFCellRef& CellRefIn)
	{
		Point = PointIn;
		CellRef = CellRefIn;
	}

	UPROPERTY(BlueprintReadWrite)
	FVector Point;

	UPROPERTY(BlueprintReadWrite)
	FSFCellRef CellRef;
};

// Note the UMeta -- DisplayName is just a nice way to show the name in the editor
// This enumeration indicates the general status of the path in the path component
UENUM(BlueprintType)
enum ESFPathState
{
	SFPS_None			UMETA(DisplayName = "None"),				// no path has been requested
	SFPS_Active			UMETA(DisplayName = "Active"),				// we are actively following a path
	SFPS_Finished		UMETA(DisplayName = "Finished"),			// we've successfully moved to the end of the path. Yay!
	SFPS_Invalid		UMETA(DisplayName = "Invalid"),				// pathfinding failed -- we couldn't find a way to the destination
};


// Our custom path following component, which will rely on the data
// contained in the GridActor
// Note the meta-specific "BlueprintSpawnableComponnet". This will allow us
// to attach this component to any actor type in Blueprint. Otherwise it would
// only be attachable in code.

UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class USFPathComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	// Note just a cached pointer
	UPROPERTY()
	mutable TSoftObjectPtr<ASFGridActor> GridActor;

	UFUNCTION(BlueprintCallable)
	const ASFGridActor *GetGridActor() const;

	// It is super easy to forget: this component will usually be attached to the CONTROLLER, not the pawn it's controlling
	// A lot of times we want access to the pawn (e.g. when sending signals to its movement component).
	UFUNCTION(BlueprintCallable, BlueprintPure)
	APawn *GetOwnerPawn();


	// State Update ------------------------

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	ESFPathState RefreshPath();

	ESFPathState AStar(const FVector& StartPoint, TArray<FSFPathStep>& StepsOut) const;

	// Fill the DistanceMap param with distance data based on the starting point and cell ref passed in
	bool Dijkstra(const FVector& StartPoint, FSFGridMap& DistanceMapOut) const;

	// Fill in the Steps array with steps that get me from the origin of the distance map (i.e. the cell that has distance 0) to the end point and end cell ref.
	bool BuidPathFromDistanceMap(const FVector& EndPoint, const FSFCellRef& EndCellRef, const FSFGridMap& DistanceMap);

	ESFPathState SmoothPath(const FVector& StartPoint, const TArray<FSFPathStep>& UnsmoothedSteps, TArray<FSFPathStep>& SmoothedStepsOut) const;

	void FollowPath();

	// Parameters ------------------------

	// When I'm within this distance of my destination, my path is considered finished.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ArrivalDistance;

	// Destination ------------------------

	UFUNCTION(BlueprintCallable)
	ESFPathState SetDestination(const FVector &DestinationPoint);

	UPROPERTY(BlueprintReadOnly)
	bool bDestinationValid;

	UPROPERTY(BlueprintReadOnly)
	bool bDistanceMapPathValid;

	UPROPERTY(BlueprintReadOnly)
	FVector Destination;

	UPROPERTY(BlueprintReadOnly)
	FSFCellRef DestinationCell;

	// State ------------------------

	UPROPERTY(BlueprintReadOnly)
	TEnumAsByte<ESFPathState> State;

	UPROPERTY(BlueprintReadWrite)
	TArray<FSFPathStep> Steps;

};