#include "GATargetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ScoreFrenzy/Grid/GAGridActor.h"
#include "GAPerceptionSystem.h"
#include "ProceduralMeshComponent.h"

UGATargetComponent::UGATargetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;

	SetTickGroup(ETickingGroup::TG_PostUpdateWork);

	// Generate a new guid
	TargetGuid = FGuid::NewGuid();
	
	bDebugOccupancyMap = true;
}


AGAGridActor* UGATargetComponent::GetGridActor() const
{
	AGAGridActor* Result = GridActor.Get();
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
				GridActor = Result;
			}
		}

		return Result;
	}
}


void UGATargetComponent::OnRegister()
{
	Super::OnRegister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->RegisterTargetComponent(this);
	}

	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		OccupancyMap = FGAGridMap(Grid, 0.0f);
	}
}

void UGATargetComponent::OnUnregister()
{
	Super::OnUnregister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->UnregisterTargetComponent(this);
	}
}



void UGATargetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool isImmediate = false;

	// update my perception state FSM
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGAPerceptionComponent>> PerceptionComponents = PerceptionSystem->GetAllPerceptionComponents();
		for (UGAPerceptionComponent* PerceptionComponent : PerceptionComponents)
		{
			if (!PerceptionComponent || !IsValid(PerceptionComponent)) { continue; }
			const FTargetView* TargetView = PerceptionComponent->GetTargetView(TargetGuid);
			if (TargetView && (TargetView->Awareness >= 1.0f))
			{
				isImmediate = true;
				break;
			}
		}
	}

	if (isImmediate)
	{
		AActor* Owner = GetOwner();
		LastKnownState.State = GATS_Immediate;

		// REFRESH MY STATE
		LastKnownState.Set(Owner->GetActorLocation(), Owner->GetVelocity());

		// Tell the omap to clear out and put all the probability in the observed location
		OccupancyMapSetPosition(LastKnownState.Position);
	}
	else if (IsKnown())
	{
		LastKnownState.State = GATS_Hidden;
	}

	if (LastKnownState.State == GATS_Hidden)
	{
		OccupancyMapUpdate();
	}

	// As long as I'm known, whether I'm immediate or not, diffuse the probability in the omap

	if (IsKnown())
	{
		OccupancyMapDiffuse();
	}

	if (bDebugOccupancyMap)
	{
		AGAGridActor* Grid = GetGridActor();
		if (Grid)
		{
			Grid->DebugGridMap = OccupancyMap;
			GridActor->RefreshDebugTexture();
			GridActor->DebugMeshComponent->SetVisibility(true);
		}
	}
}


void UGATargetComponent::OccupancyMapSetPosition(const FVector& Position)
{
	// TODO PART 4

	// We've been observed to be in a given position
	// Clear out all probability in the omap, and set the appropriate cell to P = 1.0

	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		// $\forall n \neq n^*, p(n) \leftarrow 0$ 
		OccupancyMap = FGAGridMap(Grid, 0.0f);

		FCellRef TargetPositionInCellRef = Grid->GetCellRef(Position, true);
	
		// $p(n^*) \leftarrow 1$ 
		if (TargetPositionInCellRef.IsValid() && Grid->IsCellRefInBounds(TargetPositionInCellRef))
		{
			OccupancyMap.SetValue(TargetPositionInCellRef, 1.0f);
		}
	}
}


void UGATargetComponent::OccupancyMapUpdate()
{
	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		FGAGridMap VisibilityMap(Grid, 0.0f);

		// TODO PART 4

		// STEP 1: Build a visibility map, based on the perception components of the AIs in the world
		// The visibility map is a simple map where each cell is either 0 (not currently visible to ANY perceiver) or 1 (currently visible to one or more perceivers).
		// 

		UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
		if (PerceptionSystem)
		{
			TArray<TObjectPtr<UGAPerceptionComponent>> PerceptionComponents = PerceptionSystem->GetAllPerceptionComponents();
			for (UGAPerceptionComponent* PerceptionComponent : PerceptionComponents)
			{
				if (!PerceptionComponent || !IsValid(PerceptionComponent)) { continue; }
				// Find visible cells for this perceiver.
				// Reminder: Use the PerceptionComponent.VisionParameters when determining whether a cell is visible or not (in addition to a line trace).
				// Suggestion: you might find it useful to add a UGAPerceptionComponent::TestVisibility method to the perception component.
				
				// selects candidate grid range by vision distance
				// gets the location and orientation of this perceiver

				// 1. gets the vision's start point
				APawn* Pawn = PerceptionComponent->GetOwnerPawn();
				if (!Pawn || !IsValid(Pawn))
				{
					continue;
				}
				FVector RaycastStartLocation;
				FRotator ViewRotation;
				Pawn->GetActorEyesViewPoint(RaycastStartLocation, ViewRotation);
				// 2. gets this perceiver's vision distance
				const float VisionDistance = PerceptionComponent->VisionParameters.VisionDistance;
				// 3. since traversing the cells should be done in grid space, the vision's center and vision distance 
				//    are converted to cell reference
				const FCellRef Center = Grid->GetCellRef(RaycastStartLocation, /*bClamp=*/true);
				const int32 Radius = FMath::CeilToInt(VisionDistance / Grid->CellScale);
				
				// 4. draws a circle with the vision distance as the radius and the vision's start point as the center
				//    and traverses all the grids of the circumscribed square region of this circle
				for (int32 y = Center.Y - Radius; y <= (Center.Y + Radius); ++y)
				{
					// const int32 dy = y - Center.Y;

					for (int32 x = Center.X - Radius; x <= (Center.X + Radius); ++x)
					{
						// const int32 dx = x - Center.X;
						
						const FCellRef Cell(x,y);
						// excludes cells outside the bound
						if (!Grid->IsCellRefInBounds(Cell)) continue;
						
						// 4-1. tests if within vision distance
						//      the scope is narrowed down to the inscribed circle region of the square area
						// 4-1-1 the distance comparison would be more accurate in the world space since it is continuous
						const FVector thisCellLocation = Grid->GetCellPosition(Cell);
						float Distance = FVector::Distance(thisCellLocation, RaycastStartLocation);
						// 4-1-2 when the cell's location is converted to world space, it is the grid center's coordinate,
						//       thus half the grid's side length is subtracted
						if ((Distance - Grid->CellScale / 2) > VisionDistance) continue;
						
						// 4-2. tests if within the vision field
						if (!IsValid(Pawn)) { break; }
						// 4-2-1 gets the perceiver's current forward direction
						FVector ForwardVector = Pawn->GetActorForwardVector();
						// 4-2-2 gets the vector from AI to the target
						FVector PerceiverToThisCellVector = thisCellLocation - RaycastStartLocation;
						// 4-2-3 the goal is to make the vision angle judgement in the horizontal plane, therefore the 
						//       vector in the Z direction is removed and then standardized
						ForwardVector.Z = 0.0f;
						ForwardVector.Normalize();
						PerceiverToThisCellVector.Z = 0.0f;
						PerceiverToThisCellVector.Normalize();
						// 4-2-4 calculates the cosine value of the angle between these two vectors
						// 4-2-4-1 gets the unit vectors of these two vectors
						const FVector PerceiverToTargetVectorUnit = PerceiverToThisCellVector.GetSafeNormal();
						float CosineTargetAndForward = FVector::DotProduct(ForwardVector, PerceiverToTargetVectorUnit);
						// 4-2-5 calculates the cosine value of half of the VisionAngle
						float CosineHalfVisionAngle = FMath::Cos(FMath::DegreesToRadians(PerceptionComponent->VisionParameters.PeripheralVisionAngle * 0.5f));
						// 4-2-6 determines if within vision field
						if (CosineTargetAndForward < CosineHalfVisionAngle) continue;
						
						// 4-3 LOS
						if (!PerceptionComponent->TestVisibility(Cell)) continue;
						
						VisibilityMap.SetValue(Cell, 1.0f);
					}
				}
			}
		}
		
		// STEP 2: Clear out the probability in the visible cells
		float CulledProbability = 0.0f;
		int32 MinX = OccupancyMap.GridBounds.MinX;
		int32 MaxX = OccupancyMap.GridBounds.MaxX;
		int32 MinY = OccupancyMap.GridBounds.MinY;
		int32 MaxY = OccupancyMap.GridBounds.MaxY;
		
		for (int32 y = MinY; y <= MaxY; ++y)
		{
			for (int32 x = MinX; x <= MaxX; ++x)
			{
				const FCellRef Cell(x,y);
				float CurrentValueInVisibilityMap;
				VisibilityMap.GetValue(Cell, CurrentValueInVisibilityMap);
				
				float CurrentProbabilityInOccupancyMap;
				OccupancyMap.GetValue(Cell,CurrentProbabilityInOccupancyMap);
				
				if (CurrentValueInVisibilityMap == 1.0f)
				{
					CulledProbability += CurrentProbabilityInOccupancyMap;
					OccupancyMap.SetValue(Cell, 0.0f);
				} 
			}
			
			
		}
		// STEP 3: Renormalize the OMap, so that it's still a valid probability distribution
		for (int32 y = MinY; y <= MaxY; ++y)
		{
			for (int32 x = MinX; x <= MaxX; ++x)
			{
				const FCellRef Cell(x,y);
				
				float CurrentValueInVisibilityMap;
				VisibilityMap.GetValue(Cell, CurrentValueInVisibilityMap);
				if (CurrentValueInVisibilityMap == 0.0f)
				{
					const float RemainingProbability = 1.0f - CulledProbability;
					if (RemainingProbability > KINDA_SMALL_NUMBER)
					{
						float CurrentProbabilityInOccupancyMap;
						OccupancyMap.GetValue(Cell, CurrentProbabilityInOccupancyMap);
						OccupancyMap.SetValue(Cell, CurrentProbabilityInOccupancyMap / RemainingProbability);
					}
				}
			}
		}

		// STEP 4: Extract the highest-likelihood cell on the omap and refresh the LastKnownState.
		float MaxProbability = 0.0f;
		FCellRef MaxProbabilityPosition;
		for (int32 y = MinY; y <= MaxY; ++y)
		{
			for (int32 x = MinX; x <= MaxX; ++x)
			{
				const FCellRef Cell(x,y);
				// excludes cells outside the bound
				if (!Grid->IsCellRefInBounds(Cell)) continue;
				// excludes cells not traversable
				if (!EnumHasAnyFlags(Grid->GetCellData(Cell), ECellData::CellDataTraversable)) continue;
				float P;
				OccupancyMap.GetValue(Cell, P);
				if (P > MaxProbability)
				{
					MaxProbability = P;
					MaxProbabilityPosition = Cell;
				}
			}
		}
		
		if (MaxProbabilityPosition.IsValid())
		{
			FVector MaxProbabilityPositionInWorldSpace = Grid->GetCellPosition(MaxProbabilityPosition);
			LastKnownState.Set(MaxProbabilityPositionInWorldSpace, LastKnownState.Velocity);
		}
	}

}


void UGATargetComponent::OccupancyMapDiffuse()
{
	// TODO PART 4
	// Diffuse the probability in the OMAP
	const AGAGridActor* Grid = GetGridActor();
	if (!Grid) return;
	FGAGridMap Buffer = FGAGridMap(Grid, 0.0f);
	
	int32 MinX = OccupancyMap.GridBounds.MinX;
	int32 MaxX = OccupancyMap.GridBounds.MaxX;
	int32 MinY = OccupancyMap.GridBounds.MinY;
	int32 MaxY = OccupancyMap.GridBounds.MaxY;
	
	const float Alpha = 0.01f;
		
	for (int32 y = MinY; y <= MaxY; ++y)
	{
		for (int32 x = MinX; x <= MaxX; ++x)
		{
			const FCellRef Cell(x,y);
			if (!Grid->IsCellRefInBounds(Cell)) continue;
			
			float CellProbability;
			OccupancyMap.GetValue(Cell, CellProbability);
			if (CellProbability == 0.0f) continue;
			
			float LeftProbability = CellProbability;
			
			for (int32 CurrY = y - 1; CurrY <= y + 1; ++CurrY)
			{
				for (int32 CurrX = x - 1; CurrX <= x + 1; ++CurrX)
				{
					if (CurrX == x && CurrY == y) continue;
					
					FCellRef Neighbor(CurrX, CurrY);
					if (!Grid->IsCellRefInBounds(Neighbor)) continue;
					if (!EnumHasAnyFlags(Grid->GetCellData(Neighbor), ECellData::CellDataTraversable)) continue;
					
					float ProbabilityDiffusion;
					if ((CurrY - y + CurrX - x) % 2 == 0)
					{
						ProbabilityDiffusion = Alpha * LeftProbability / FMath::Sqrt(2.0f);
					}
					else
					{
						ProbabilityDiffusion = Alpha * LeftProbability;
					}
					float PreviousProbabilityOfThisNeighborCell;
					Buffer.GetValue(Neighbor, PreviousProbabilityOfThisNeighborCell);
					PreviousProbabilityOfThisNeighborCell += ProbabilityDiffusion;
					Buffer.SetValue(Neighbor, PreviousProbabilityOfThisNeighborCell);
					LeftProbability -= ProbabilityDiffusion;
				}
				
			}
			float PreviousProbabilityOfThisCell;
			Buffer.GetValue(Cell, PreviousProbabilityOfThisCell);
			PreviousProbabilityOfThisCell += LeftProbability; 
			Buffer.SetValue(Cell, PreviousProbabilityOfThisCell);
		}
	}
	
	OccupancyMap = Buffer;
}
