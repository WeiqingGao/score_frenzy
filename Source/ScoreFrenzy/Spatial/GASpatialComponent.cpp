#include "GASpatialComponent.h"
#include "ScoreFrenzy/Pathfinding/GAPathComponent.h"
#include "ScoreFrenzy/Grid/GAGridMap.h"
#include "Kismet/GameplayStatics.h"
#include "Math/MathFwd.h"
#include "GASpatialFunction.h"
#include "GASpatialFunction_Cover.h"
#include "ProceduralMeshComponent.h"



UGASpatialComponent::UGASpatialComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SampleDimensions = 8000.0f;		// should cover the bulk of the test map
}


AGAGridActor* UGASpatialComponent::GetGridActor() const
{
	AGAGridActor* Result = GridActorInternal.Get();
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
				GridActorInternal = Result;
			}
		}

		return Result;
	}
}

UGAPathComponent* UGASpatialComponent::GetPathComponent() const
{
	UGAPathComponent* Result = PathComponentInternal.Get();
	if (Result)
	{
		return Result;
	}
	else
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			// Note, the UGAPathComponent and the UGASpatialComponent are both on the controller
			Result = Owner->GetComponentByClass<UGAPathComponent>();
			if (Result)
			{
				PathComponentInternal = Result;
			}
		}
		return Result;
	}
}

APawn* UGASpatialComponent::GetOwnerPawn() const
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		APawn* Pawn = Cast<APawn>(Owner);
		if (Pawn)
		{
			return Pawn;
		}
		else
		{
			AController* Controller = Cast<AController>(Owner);
			if (Controller)
			{
				return Controller->GetPawn();
			}
		}
	}

	return NULL;
}


bool UGASpatialComponent::ChoosePosition(bool PathfindToPosition, bool Debug)
{
	bool Result = false;
	const APawn* OwnerPawn = GetOwnerPawn();
	if (OwnerPawn == NULL)
	{
		return false;
	}

	AGAGridActor* Grid = GetGridActor();

	if (SpatialFunctionReference.Get() == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGASpatialComponent has no SpatialFunctionReference assigned."));
		return false;
	}

	if (Grid == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGASpatialComponent::ChoosePosition can't find a GridActor."));
		return false;
	}

	UGAPathComponent* PathComponent = GetPathComponent();
	if (PathComponent == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGASpatialComponent::ChoosePosition can't find a PathComponent."));
		return false;
	}


	// Don't worry too much about the Unreal-ism below. Technically our SpatialFunctionReference is not ACTUALLY
	// a spatial function instance, rather it's a class, which happens to have a lot of data in it.
	// Happily, Unreal creates, under the hood, a default object for every class, that lets you access that data
	// as if it were a normal instance
	const UGASpatialFunction* SpatialFunction = SpatialFunctionReference->GetDefaultObject<UGASpatialFunction>();

	// The below is to create a GridMap (which you will fill in) based on a bounding box centered around the OwnerPawn

	FBox2D Box(EForceInit::ForceInit);
	FIntRect CellRect;
	// GridSpaceBoundsToRect2D expects grid space, not world space.
	// Grid space = World space - GridActor position + HalfExtents
	FVector2D PawnLocation(OwnerPawn->GetActorLocation());
	FVector2D GridActorLocation2D(Grid->GetActorLocation());
	FVector2D PawnLocationGridSpace = PawnLocation - GridActorLocation2D + Grid->HalfExtents;
	Box += PawnLocationGridSpace;
	Box = Box.ExpandBy(SampleDimensions / 2.0f);
	if (Grid->GridSpaceBoundsToRect2D(Box, CellRect))
	{
		// Super annoying, by the way, that FIntRect is not blueprint accessible, because it forces us instead
		// to make a separate bp-accessible FStruct that represents _exactly the same thing_.
		FGridBox GridBox(CellRect);

		// This is the grid map I'm going to fill with values
		FGAGridMap ScoreMap(Grid, GridBox, 0.0f);

		// Fill in this distance map using Dijkstra!
		FGAGridMap DistanceMap(Grid, GridBox, FLT_MAX);


		// ~~~ ASSIGNMENT 3 part 4-3 IMPLEMENTATION ~~~

		// (a) Run Dijkstra's to determine which cells we should even be evaluating (the GATHER phase)
		// This fills DistanceMap with path distances from the AI's current position
		FVector StartPoint = OwnerPawn->GetActorLocation();
		bool bDijkstraSuccess = PathComponent->Dijkstra(StartPoint, DistanceMap);
		
		if (!bDijkstraSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("UGASpatialComponent::ChoosePosition - Dijkstra failed."));
			return false;
		}

		// For each layer in the spatial function, evaluate and accumulate the layer in GridMap
		// Note, only evaluate accessible cells found in step 1
		for (const FFunctionLayer& Layer : SpatialFunction->Layers)
		{
			// figure out how to evaluate each layer type, and accumulate the value in the ScoreMap
			EvaluateLayer(Layer, DistanceMap, ScoreMap);
		}

		// (b) Add some hysteresis (a score bonus) to the last tick's chosen cell
		// This reduces jitter by giving a small bonus to the previously chosen position
		if (bHasLastChosenCell && Grid->IsCellRefInBounds(LastChosenCell))
		{
			float CurrentScore = 0.0f;
			if (ScoreMap.GetValue(LastChosenCell, CurrentScore))
			{
				// Add the hysteresis bias to discourage constantly switching positions
				ScoreMap.SetValue(LastChosenCell, CurrentScore + HysteresisBias);
			}
		}

		// (c) Pick the best cell in ScoreMap
		// We need to find the cell with the highest score that is also reachable
		FCellRef BestCell = FCellRef::Invalid;
		float BestScore = -FLT_MAX;
		const float INF = 1e30f;

		for (int32 Y = ScoreMap.GridBounds.MinY; Y <= ScoreMap.GridBounds.MaxY; Y++)
		{
			for (int32 X = ScoreMap.GridBounds.MinX; X <= ScoreMap.GridBounds.MaxX; X++)
			{
				FCellRef CellRef(X, Y);
				
				// Only consider traversable cells
				if (!EnumHasAllFlags(Grid->GetCellData(CellRef), ECellData::CellDataTraversable))
				{
					continue;
				}
				
				// Only consider reachable cells (those with finite distance in the distance map)
				float Distance = INF;
				if (!DistanceMap.GetValue(CellRef, Distance) || Distance >= INF)
				{
					continue;
				}
				
				// Get the score for this cell
				float Score = 0.0f;
				if (ScoreMap.GetValue(CellRef, Score))
				{
					if (Score > BestScore)
					{
						BestScore = Score;
						BestCell = CellRef;
					}
				}
			}
		}

		// Update the last chosen cell for hysteresis
		if (BestCell.IsValid())
		{
			bHasLastChosenCell = true;
			LastChosenCell = BestCell;
			Result = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UGASpatialComponent::ChoosePosition - No valid cell found."));
			return false;
		}

		if (PathfindToPosition && BestCell.IsValid())
		{
			// (d) Go there! Call the pathcomponent's BuildPathFromDistanceMap() function
			FVector EndPoint = Grid->GetCellPosition(BestCell);
			bool bPathSuccess = PathComponent->BuidPathFromDistanceMap(EndPoint, BestCell, DistanceMap);
			
			if (!bPathSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("UGASpatialComponent::ChoosePosition - BuildPathFromDistanceMap failed."));
				// Don't return false here - we still found a valid position, just couldn't path to it
			}
		}

		
		if (Debug)
		{
			// Note: this outputs (basically) the results of the position selection
			// However, you can get creative with the debugging here. For example, maybe you want
			// to be able to examine the values of a specific layer in the spatial function
			// You could create a separate debug map above (where you're doing the evaluations) and
			// cache it off for debug rendering. Ideally you'd be able to control what layer you wanted to 
			// see from blueprint

			Grid->DebugGridMap = ScoreMap;
			Grid->RefreshDebugTexture();
			Grid->DebugMeshComponent->SetVisibility(true);		//cheeky!
		}
	}

	return Result;
}


bool UGASpatialComponent::GetLastChosenWorldPosition(FVector& OutPosition) const
{
	if (!bHasLastChosenCell)
	{
		return false;
	}
	const AGAGridActor* Grid = GetGridActor();
	if (!Grid)
	{
		return false;
	}
	OutPosition = Grid->GetCellPosition(LastChosenCell);
	return true;
}


void UGASpatialComponent::EvaluateLayer(const FFunctionLayer& Layer, const FGAGridMap& DistanceMap, FGAGridMap& ScoreMap) const
{
	APawn* OwnerPawn = GetOwnerPawn();
	const AGAGridActor* Grid = GetGridActor();

	// Get the player pawn (the target for range and LOS calculations)
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	
	// Early out if we need player but don't have one
	if ((Layer.Input == SI_TargetRange || Layer.Input == SI_LOS) && PlayerPawn == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGASpatialComponent::EvaluateLayer - No player pawn found for target-based input."));
		return;
	}

	// Cache player location for efficiency
	FVector PlayerLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	
	// Get world for raycasting
	UWorld* World = GetWorld();
	
	// Constant for unreachable cells
	const float INF = 1e30f;

	for (int32 Y = ScoreMap.GridBounds.MinY; Y <= ScoreMap.GridBounds.MaxY; Y++)
	{
		for (int32 X = ScoreMap.GridBounds.MinX; X <= ScoreMap.GridBounds.MaxX; X++)
		{
			FCellRef CellRef(X, Y);

			if (EnumHasAllFlags(Grid->GetCellData(CellRef), ECellData::CellDataTraversable))
			{
				// ~~~ ASSIGNMENT 3 part 4-4 IMPLEMENTATION ~~~
				
				// Only evaluate cells that are reachable (have finite distance in the distance map)
				float PathDistance = INF;
				if (!DistanceMap.GetValue(CellRef, PathDistance) || PathDistance >= INF)
				{
					// Cell is not reachable, skip it
					continue;
				}

				// First step: determine input value based on the layer's input type
				float InputValue = 0.0f;
				
				switch (Layer.Input)
				{
					case SI_None:
					{
						// No input - constant value of 0
						InputValue = 0.0f;
						break;
					}
					
					case SI_TargetRange:
					{
						// Euclidean distance from this cell to the player (target)
						FVector CellPosition = Grid->GetCellPosition(CellRef);
						InputValue = FVector::Dist(CellPosition, PlayerLocation);
						break;
					}
					
					case SI_PathDistance:
					{
						// Path distance from the AI's current position to this cell
						// We already computed this via Dijkstra, it's stored in the DistanceMap
						// Convert from cell units to world units (multiply by CellScale)
						InputValue = PathDistance * Grid->CellScale;
						break;
					}
					
					case SI_LOS:
					{
						// Line of sight to the player
						// Returns 1.0 if we have clear LOS, 0.0 if blocked
						FVector Start = Grid->GetCellPosition(CellRef);
						FVector End = PlayerLocation;
						
						// Offset the start position up a bit so we're not tracing from the ground
						Start.Z += 50.0f;
						
						FHitResult HitResult;
						FCollisionQueryParams Params;
						Params.AddIgnoredActor(PlayerPawn);
						Params.AddIgnoredActor(OwnerPawn);
						
						bool bHitSomething = World->LineTraceSingleByChannel(
							HitResult, 
							Start, 
							End, 
							ECollisionChannel::ECC_Visibility, 
							Params
						);
						
						// If we didn't hit anything, we have clear LOS
						// InputValue = 1.0 means clear LOS, 0.0 means blocked
						InputValue = bHitSomething ? 0.0f : 1.0f;
						break;
					}

					case SI_CoverFromPlayer:
					{
						// Is this cell in cover FROM the player?
						// Trace from the player toward the cell. If something blocks it, the cell is in cover (score 1.0).
						// This is the inverse of SI_LOS: 1.0 = player cannot see this cell.
						FVector Start = PlayerLocation;
						Start.Z += 50.0f;	// raise to approximate eye level so trace hits walls
						FVector End = Grid->GetCellPosition(CellRef);
						End.Z += 50.0f;

						// Apply distance constraints only when MaxCoverDistance is explicitly set (> 0)
						const UGASpatialFunction* SpatialFunction = SpatialFunctionReference ? SpatialFunctionReference->GetDefaultObject<UGASpatialFunction>() : nullptr;
						const UGASpatialFunction_Cover* CoverFunction = Cast<UGASpatialFunction_Cover>(SpatialFunction);
						if (CoverFunction && CoverFunction->MaxCoverDistance > 0.0f)
						{
							float DistToPlayer = FVector::Dist(End, PlayerLocation);
							if (DistToPlayer < CoverFunction->MinCoverDistance || DistToPlayer > CoverFunction->MaxCoverDistance)
							{
								InputValue = 0.0f;
								break;
							}
						}

						FHitResult HitResult;
						FCollisionQueryParams Params;
						Params.AddIgnoredActor(PlayerPawn);
						Params.AddIgnoredActor(OwnerPawn);

						bool bHitSomething = World->LineTraceSingleByChannel(
							HitResult,
							Start,
							End,
							ECollisionChannel::ECC_WorldStatic,
							Params
						);

						// Blocked by geometry = good cover = 1.0
						InputValue = bHitSomething ? 1.0f : 0.0f;
						break;
					}

					default:
					{
						InputValue = 0.0f;
						break;
					}
				}

				// Next: run the input through the response curve
				float ModifiedValue = 0.0f;
				const FRichCurve* Curve = Layer.ResponseCurve.GetRichCurveConst();
				if (Curve)
				{
					ModifiedValue = Curve->Eval(InputValue, 0.0f);
				}
				else
				{
					// No curve defined, just use the input directly
					ModifiedValue = InputValue;
				}

				// Finally: combine with the current score using the specified operator
				float CurrentScore = 0.0f;
				ScoreMap.GetValue(CellRef, CurrentScore);
				
				float CombinedValue = CurrentScore;
				
				switch (Layer.Op)
				{
					case SO_None:
					{
						// No operation - don't modify the score
						CombinedValue = CurrentScore;
						break;
					}
					
					case SO_Add:
					{
						// Add this layer's value to the accumulated score
						CombinedValue = CurrentScore + ModifiedValue;
						break;
					}
					
					case SO_Multiply:
					{
						// Multiply this layer's value into the accumulated score
						CombinedValue = CurrentScore * ModifiedValue;
						break;
					}
					
					default:
					{
						CombinedValue = CurrentScore;
						break;
					}
				}

				ScoreMap.SetValue(CellRef, CombinedValue);
			}
		}
	}
}