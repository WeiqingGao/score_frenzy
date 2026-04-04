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
	FVector2D PawnLocation(OwnerPawn->GetActorLocation());
	Box += PawnLocation;
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


		// (a) Run Dijkstra from the AI's current position to populate the distance map.
		//     Cells that are unreachable remain at FLT_MAX.
		// PathComponent->Dijkstra(OwnerPawn->GetActorLocation(), DistanceMap);

		// Evaluate and accumulate each layer of the spatial function
		for (const FFunctionLayer& Layer : SpatialFunction->Layers)
		{
			EvaluateLayer(Layer, DistanceMap, ScoreMap);
		}

		// (c) Pick the highest-scoring reachable cell
		Result = true;
		// FCellRef BestCell = FCellRef::Invalid;
		// float BestScore = -FLT_MAX;
		// for (int32 Y = ScoreMap.GridBounds.MinY; Y < ScoreMap.GridBounds.MaxY; Y++)
		// {
		// 	for (int32 X = ScoreMap.GridBounds.MinX; X < ScoreMap.GridBounds.MaxX; X++)
		// 	{
		// 		FCellRef CellRef(X, Y);
		// 		if (!EnumHasAllFlags(Grid->GetCellData(CellRef), ECellData::CellDataTraversable))
		// 		{
		// 			continue;
		// 		}
		// 		float Score = 0.0f;
		// 		if (ScoreMap.GetValue(CellRef, Score) && Score > BestScore)
		// 		{
		// 			BestScore = Score;
		// 			BestCell = CellRef;
		// 		}
		// 	}
		// }
		//
		// Result = BestCell.IsValid();
		//
		// if (Result)
		// {
		// 	ChosenPosition = Grid->GetCellPosition(BestCell);
		//
		// 	if (PathfindToPosition)
		// 	{
		// 		// (d) Build and begin following a path to the chosen cell
		// 		PathComponent->BuidPathFromDistanceMap(ChosenPosition, BestCell, DistanceMap);
		// 	}
		// }
		if (PathfindToPosition)
		{
			// (d) Go there! You should call your pathcomponent's UGAPathComponent::BuildPathFromDistanceMap() function
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


void UGASpatialComponent::EvaluateLayer(const FFunctionLayer& Layer, const FGAGridMap& DistanceMap, FGAGridMap& ScoreMap) const
{
	APawn* OwnerPawn = GetOwnerPawn();
	const AGAGridActor* Grid = GetGridActor();
	
	for (int32 Y = ScoreMap.GridBounds.MinY; Y < ScoreMap.GridBounds.MaxY; Y++)
	{
		for (int32 X = ScoreMap.GridBounds.MinX; X < ScoreMap.GridBounds.MaxX; X++)
		{
			FCellRef CellRef(X, Y);

			if (EnumHasAllFlags(Grid->GetCellData(CellRef), ECellData::CellDataTraversable))
			{
				// Assignment 3 part 4-4: evaluate me!

				// First step is determine input value. Remember there are three possible inputs to handle:
				// 	SI_None				UMETA(DisplayName = "None"),
				//	SI_TargetRange		UMETA(DisplayName = "Target Range"),
				//	SI_PathDistance		UMETA(DisplayName = "PathDistance"),
				//	SI_LOS				UMETA(DisplayName = "Line Of Sight")

				// Next, run it through the response curve using something like this
				// float Value = 4.5f;
				// float ModifiedValue = Layer.ResponseCurve.GetRichCurveConst()->Eval(Value, 0.0f);

				// Then add it's influence to the grid map, combining with the current value using one of the two operators
				//	SO_None				UMETA(DisplayName = "None"),
				//	SO_Add				UMETA(DisplayName = "Add"),			// add this layer to the accumulated buffer
				//	SO_Multiply			UMETA(DisplayName = "Multiply")		// multiply this layer into the accumulated buffer

				//ScoreMap.SetValue(CellRef, CombinedValue);


				// HERE ARE SOME ADDITIONAL HINTS

				// Here's how to get the player's pawn
				// APawn *PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

				// Here's how to cast a ray

				// UWorld* World = GetWorld();
				// FHitResult HitResult;
				// FCollisionQueryParams Params;
				// FVector Start = Grid->GetCellPosition(CellRef);		// need a ray start
				// FVector End = PlayerPawn->GetActorLocation();		// need a ray end
				// Start.Z += 50.0f;									// offset by 50uus so 
				// Add any actors that should be ignored by the raycast by calling
				// Params.AddIgnoredActor(PlayerPawn);			// Probably want to ignore the player pawn
				// Params.AddIgnoredActor(OwnerPawn);			// Probably want to ignore the AI themself
				// bool bHitSomething = World->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, Params);
				// If bHitSomething is false, then we have a clear LOS
			}
		}
	}
}
	// UWorld* World = GetWorld();

	// if (!Grid || !World)
	// {
		// return;
	// }

	// If this is a cover function, retrieve cover-specific distance / LOS constraints
// 	const UGASpatialFunction_Cover* CoverFunc = nullptr;
// 	if (SpatialFunctionReference.Get())
// 	{
// 		CoverFunc = Cast<UGASpatialFunction_Cover>(
// 			SpatialFunctionReference->GetDefaultObject<UGASpatialFunction>()
// 		);
// 	}
//
// 	for (int32 Y = ScoreMap.GridBounds.MinY; Y < ScoreMap.GridBounds.MaxY; Y++)
// 	{
// 		for (int32 X = ScoreMap.GridBounds.MinX; X < ScoreMap.GridBounds.MaxX; X++)
// 		{
// 			FCellRef CellRef(X, Y);
//
// 			if (!EnumHasAllFlags(Grid->GetCellData(CellRef), ECellData::CellDataTraversable))
// 			{
// 				continue;
// 			}
//
// 			float RawValue = 0.0f;
// 			bool bSkipCell = false;
//
// 			switch (Layer.Input)
// 			{
// 			case SI_None:
// 				RawValue = 1.0f;
// 				break;
//
// 			case SI_TargetRange:
// 			{
// 				APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
// 				if (!PlayerPawn) { bSkipCell = true; break; }
// 				FVector CellPos = Grid->GetCellPosition(CellRef);
// 				RawValue = FVector::Dist(CellPos, PlayerPawn->GetActorLocation());
// 				break;
// 			}
//
// 			case SI_PathDistance:
// 			{
// 				float DistValue = 0.0f;
// 				if (!DistanceMap.GetValue(CellRef, DistValue) || DistValue >= FLT_MAX)
// 				{
// 					bSkipCell = true;
// 					break;
// 				}
// 				RawValue = DistValue;
// 				break;
// 			}
//
// 			case SI_LOS:
// 			{
// 				APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
// 				if (!PlayerPawn) { bSkipCell = true; break; }
//
// 				FVector CellPos = Grid->GetCellPosition(CellRef);
// 				FVector PlayerPos = PlayerPawn->GetActorLocation();
// 				CellPos.Z += 50.0f;
//
// 				FHitResult HitResult;
// 				FCollisionQueryParams Params;
// 				Params.AddIgnoredActor(PlayerPawn);
// 				if (OwnerPawn) Params.AddIgnoredActor(OwnerPawn);
//
// 				bool bHit = World->LineTraceSingleByChannel(HitResult, CellPos, PlayerPos, ECC_Visibility, Params);
// 				RawValue = bHit ? 0.0f : 1.0f;   // 1 = clear LOS to player (good for attacking)
// 				break;
// 			}
//
// 			case SI_CoverFromPlayer:
// 			{
// 				APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
// 				if (!PlayerPawn) { bSkipCell = true; break; }
//
// 				FVector CellPos = Grid->GetCellPosition(CellRef);
// 				FVector PlayerPos = PlayerPawn->GetActorLocation();
// 				CellPos.Z += 50.0f;
//
// 				// Apply distance constraints from GASpatialFunction_Cover if available
// 				if (CoverFunc)
// 				{
// 					const float Dist = FVector::Dist(CellPos, PlayerPos);
// 					if (Dist < CoverFunc->MinCoverDistance || Dist > CoverFunc->MaxCoverDistance)
// 					{
// 						bSkipCell = true;
// 						break;
// 					}
// 				}
//
// 				FHitResult HitResult;
// 				FCollisionQueryParams Params;
// 				Params.AddIgnoredActor(PlayerPawn);
// 				if (OwnerPawn) Params.AddIgnoredActor(OwnerPawn);
//
// 				bool bHit = World->LineTraceSingleByChannel(HitResult, CellPos, PlayerPos, ECC_Visibility, Params);
//
// 				// If peek-and-shoot mode is required but this cell is fully blocked, skip it
// 				if (CoverFunc && CoverFunc->bRequireLOSToPlayer && bHit)
// 				{
// 					bSkipCell = true;
// 					break;
// 				}
//
// 				RawValue = bHit ? 1.0f : 0.0f;   // 1 = player cannot see this cell (good cover)
// 				break;
// 			}
//
// 			default:
// 				bSkipCell = true;
// 				break;
// 			}
//
// 			if (bSkipCell)
// 			{
// 				continue;
// 			}
//
// 			// Apply response curve
// 			const float ModifiedValue = Layer.ResponseCurve.GetRichCurveConst()->Eval(RawValue, 0.0f);
//
// 			// Accumulate into ScoreMap using the configured operator
// 			float CurrentValue = 0.0f;
// 			ScoreMap.GetValue(CellRef, CurrentValue);
//
// 			float NewValue = CurrentValue;
// 			switch (Layer.Op)
// 			{
// 			case SO_Add:
// 				NewValue = CurrentValue + ModifiedValue;
// 				break;
// 			case SO_Multiply:
// 				NewValue = CurrentValue * ModifiedValue;
// 				break;
// 			default:
// 				NewValue = ModifiedValue;
// 				break;
// 			}
//
// 			ScoreMap.SetValue(CellRef, NewValue);
// 		}
// 	}
// }