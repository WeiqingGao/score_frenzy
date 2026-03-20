#include "SFPathComponent.h"

#include "Chaos/EPA.h"
#include "GameFramework/NavMovementComponent.h"
#include "Kismet/GameplayStatics.h"

USFPathComponent::USFPathComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	State = SFPS_None;
	bDestinationValid = false;
	ArrivalDistance = 100.0f;

	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;
}

const ASFGridActor* USFPathComponent::GetGridActor() const
{
	if (GridActor.Get())
	{
		return GridActor.Get();
	}
	else
	{
		ASFGridActor* Result = NULL;
		AActor *GenericResult = UGameplayStatics::GetActorOfClass(this, ASFGridActor::StaticClass());
		if (GenericResult)
		{
			Result = Cast<ASFGridActor>(GenericResult);
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

APawn* USFPathComponent::GetOwnerPawn()
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


void USFPathComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (GetOwnerPawn() == NULL)
	{
		return;
	}

	bool Valid = false;
	if (bDestinationValid)
	{
		RefreshPath();
		Valid = true;
	}
	else if (bDistanceMapPathValid)
	{
		Valid = true;
	}
	if (Valid)
	{
		if (State == SFPS_Active)
		{
			FollowPath();
		}
	}

	// Super important! Otherwise, unbelievably, the Tick event in Blueprint won't get called

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

ESFPathState USFPathComponent::RefreshPath()
{
	AActor* Owner = GetOwnerPawn();
	if (Owner == NULL)
	{
		State = SFPS_Invalid;
		return State;
	}

	FVector StartPoint = Owner->GetActorLocation();

	check(bDestinationValid);

	float DistanceToDestination = FVector::Dist(StartPoint, Destination);

	if (DistanceToDestination <= ArrivalDistance)
	{
		// Yay! We got there!
		State = SFPS_Finished;
	}
	else
	{
		TArray<FSFPathStep> UnsmoothedSteps;
		Steps.Empty();

		// Replan the path!
		State = AStar(StartPoint, UnsmoothedSteps);

		// To debug A* without smoothing uncomment this line and then skip the call to SmoothPath below:
		Steps = UnsmoothedSteps;

		if (State == ESFPathState::SFPS_Active)
		{
			// Smooth the path!
			State = SmoothPath(StartPoint, UnsmoothedSteps, Steps);
		}
	}

	return State;
}

ESFPathState USFPathComponent::AStar(const FVector& StartPoint, TArray<FSFPathStep>& StepsOut) const
{
	// Assignment 2 Part3: replace this with an A* search!
	// HINT 1: you made need a heap structure. A TArray can be accessed as a heap -- just add/remove elements using
	// the TArray::HeapPush() and TArray::HeapPop() methods.
	// Note that whatever you push or pop needs to implement the 'less than' operator (operator<)
	// HINT 2: UE has some useful flag testing function. For example you can test for traversability by doing this:
	// ESFCellData Flags = Grid->GetCellData(CellRef);
	// bool bIsCellTraversable = EnumHasAllFlags(Flags, ESFCellData::CellDataTraversable)
	//	StepsOut.SetNum(1);
	//	StepsOut[0].Set(Destination, DestinationCell);

	// HINT 3: make sure you return the correct status, based on whether you succeeded to find a path or not.
	// See the comment in SFPathComponent above the ESFPathState enum
    StepsOut.Reset();

    const ASFGridActor* Grid = GetGridActor();
    if (!Grid)
    {
        return SFPS_Invalid;
    }

    const FSFCellRef StartCell = Grid->GetCellRef(StartPoint, /*bClamp=*/false);
    const FSFCellRef GoalCell  = DestinationCell;

    if (!StartCell.IsValid() || !GoalCell.IsValid())
    {
        return SFPS_Invalid;
    }

    if (StartCell == GoalCell)
    {
        StepsOut.SetNum(1);
        StepsOut[0].Set(Destination, GoalCell);
        return SFPS_Active;
    }

    auto IsTraversable = [&](const FSFCellRef& Cell) -> bool
    {
        if (!Grid->IsCellRefInBounds(Cell)) return false;
        return EnumHasAllFlags(Grid->GetCellData(Cell), ESFCellData::CellDataTraversable);
    };

    if (!IsTraversable(GoalCell) || !IsTraversable(StartCell))
    {
        return SFPS_Invalid;
    }

    auto Heuristic = [&](const FSFCellRef& Cell) -> float
    {
        // Euclidean in cell space
        return Cell.Distance(GoalCell);
    };

    struct FOpenNode
    {
        FSFCellRef Cell;
        float F = 0.0f;
        float G = 0.0f; // keep for stale check

        // pop the smallest F first.
        bool operator<(const FOpenNode& Other) const
        {
            return F < Other.F;
        }
    };

    TArray<FOpenNode> OpenHeap;
    OpenHeap.Reserve(1024);

    TMap<FSFCellRef, float> GScore;
    TMap<FSFCellRef, FSFCellRef> CameFrom;
    TSet<FSFCellRef> Closed;

    // init start
    {
        GScore.Add(StartCell, 0.0f);

        FOpenNode N;
        N.Cell = StartCell;
        N.G = 0.0f;
        N.F = Heuristic(StartCell);
        OpenHeap.HeapPush(N);
    }

    auto RelaxNeighbor = [&](const FSFCellRef& Current, float CurrentBestG, const FSFCellRef& Neighbor)
    {
        if (!IsTraversable(Neighbor)) return;
        if (Closed.Contains(Neighbor)) return;

        const float TentativeG = CurrentBestG + 1.0f;

        float* ExistingG = GScore.Find(Neighbor);
        if (!ExistingG || TentativeG < *ExistingG)
        {
            GScore.Add(Neighbor, TentativeG);
            CameFrom.Add(Neighbor, Current);

            FOpenNode N;
            N.Cell = Neighbor;
            N.G = TentativeG;
            N.F = TentativeG + Heuristic(Neighbor);
            OpenHeap.HeapPush(N);
        }
    };

    bool bFound = false;

    while (OpenHeap.Num() > 0)
    {
        FOpenNode CurrentNode;
        OpenHeap.HeapPop(CurrentNode);
        const FSFCellRef Current = CurrentNode.Cell;
        const float* BestG = GScore.Find(Current);
    	
        if (!BestG) continue;
        if (CurrentNode.G > *BestG) continue;
    	if (Closed.Contains(Current)) continue;
        if (Current == GoalCell)
        {
            bFound = true;
            break;
        }

        Closed.Add(Current);

        RelaxNeighbor(Current, *BestG, FSFCellRef(Current.X + 1, Current.Y));
        RelaxNeighbor(Current, *BestG, FSFCellRef(Current.X - 1, Current.Y));
        RelaxNeighbor(Current, *BestG, FSFCellRef(Current.X, Current.Y + 1));
        RelaxNeighbor(Current, *BestG, FSFCellRef(Current.X, Current.Y - 1));
    }

    if (!bFound)
    {
        return SFPS_Invalid;
    }

    // reconstruct (Goal -> ... -> Start)
    TArray<FSFCellRef> CellPath;
    {
        FSFCellRef Cur = GoalCell;
        CellPath.Add(Cur);

        while (!(Cur == StartCell))
        {
            FSFCellRef* Parent = CameFrom.Find(Cur);
            Cur = *Parent;
            CellPath.Add(Cur);
        }

        Algo::Reverse(CellPath);
    }

    // skip StartCell
    StepsOut.Reserve(CellPath.Num());
    int32 StartIndex = (CellPath.Num() >= 2 && CellPath[0] == StartCell) ? 1 : 0;

    for (int32 i = StartIndex; i < CellPath.Num(); i++)
    {
        const FSFCellRef& Cell = CellPath[i];
        FSFPathStep Step;
        Step.Set(Grid->GetCellPosition(Cell), Cell);
        StepsOut.Add(Step);
    }

    return SFPS_Active;
}

bool USFPathComponent::Dijkstra(const FVector& StartPoint, FSFGridMap& DistanceMapOut) const
{
	// Assignment 3 Part 3-1: implement Dijkstra's algorithm to fill out the distance map
	
	// gets the grid
	const ASFGridActor* Grid = GetGridActor();
	// validates the grid
	if (!Grid) 
	{
		return false;
	}
	
	// gets the start cell
	FSFCellRef StartCell = Grid->GetCellRef(StartPoint, false);
	// validates the StartCell
	if (!StartCell.IsValid() || !Grid->IsCellRefInBounds(StartCell))
	{
		return false;
	}
	
	// defines the traversable conditions: valid cell within the bounds with 'Traversable` flag
	auto IsTraversable = [&](const FSFCellRef& Cell) -> bool
	{
		if (!Cell.IsValid())
		{
			return false;
		}
		if (!Grid->IsCellRefInBounds(Cell))
		{
			return false;
		}
		// Added to check if cell is within the DistanceMap's bounds
		if (!DistanceMapOut.GridBounds.IsValidCell(Cell))
		{
			return false;
		}
		return EnumHasAllFlags(Grid->GetCellData(Cell), ESFCellData::CellDataTraversable);
	};

	// makes sure the start cell is traversable
	if (!IsTraversable(StartCell))
	{
		return false;
	}
	
	// initializes DistanceMapOut
	// d(start) = 0, d(all other) = +\infty
	const float INF = 1e30f;
	DistanceMapOut.ResetData(INF);
	DistanceMapOut.SetValue(StartCell, 0.0f);
	
	// min-heap priority queue (processing the cell with the shortest distance firstly)
	struct FQueueNode
	{
		FSFCellRef Cell;
		float Distance = 0.0f;
		
		// smaller one should be popped
		// considered the HeapPop pops the largest one, I make it inverse here.
		bool operator<(const FQueueNode& Other) const
		{
			return Distance > Other.Distance;
		}
	};
	
	// creates a heap and pushes the startcell
	TArray<FQueueNode> Heap;
	Heap.Reserve(1024);
	Heap.HeapPush(FQueueNode{ StartCell, 0.0f });
	
	// main loop: pop next, relax its neighbors
	while (Heap.Num() > 0)
	{
		FQueueNode CurrentNode;
		Heap.HeapPop(CurrentNode);
		
		const FSFCellRef U = CurrentNode.Cell;
		const float DU = CurrentNode.Distance;
		
		float RecordedDU = INF;
		if (!DistanceMapOut.GetValue(U, RecordedDU))
		{
			continue;
		}
		if (DU > RecordedDU)
		{
			continue;
		}
		
		const FSFCellRef Neighbors[4] = {
			FSFCellRef(U.X + 1, U.Y),
			FSFCellRef(U.X - 1, U.Y),
			FSFCellRef(U.X, U.Y + 1),
			FSFCellRef(U.X, U.Y - 1),  // Fixed: was (U.X - 1, U.Y + 1)
		};  
		
		for (const FSFCellRef& V : Neighbors)
		{
			if (!IsTraversable(V))
			{
				continue;
			}
			
			const float Alt = DU + 1.0f; // edge cost = 1
			
			float DV = INF;
			DistanceMapOut.GetValue(V, DV);
			
			if (Alt < DV)
			{
				DistanceMapOut.SetValue(V, Alt);
				Heap.HeapPush(FQueueNode{ V, Alt });
			}
		}
	}
	return true;
}

bool USFPathComponent::BuidPathFromDistanceMap(const FVector& EndPoint, const FSFCellRef& EndCellRef, const FSFGridMap& DistanceMap)
{
	bDistanceMapPathValid = false; 

	// Assignment 3 Part 3-2: reconstruct a path from the distance map
	// clear the old path
	Steps.Empty();
	
	// gets the grid validates it
	const ASFGridActor* Grid = GetGridActor();
	if (!Grid)
	{
		return false;
	}
	
	// sets the start cell as the pawn's position
	APawn* Pawn = GetOwnerPawn();
	if (!Pawn)
	{
		return false;
	}
	const FVector OriginPoint = Pawn->GetActorLocation();
	const FSFCellRef OriginCell = Grid->GetCellRef(OriginPoint, false);
	
	auto IsTraversable = [&](const FSFCellRef& Cell) -> bool
	{
		if (!Cell.IsValid())
		{
			return false;
		}
		if (!Grid->IsCellRefInBounds(Cell))
		{
			return false;
		}
		return EnumHasAllFlags(Grid->GetCellData(Cell), ESFCellData::CellDataTraversable);
	};
	
	if (!IsTraversable(OriginCell) || !IsTraversable(EndCellRef))
	{
		return false;
	}
	
	// checks reachability of the end point
	const float INF = 1e30f;;
	float EndDistance = INF;
	if (!DistanceMap.GetValue(EndCellRef, EndDistance) || EndDistance >= INF)
	{
		return false;
	}
	
	// reconstructs cell path by walking "downhill" in the distance field?
	TArray<FSFCellRef> CellPathReversed;
	CellPathReversed.Reserve(256);
	FSFCellRef Current = EndCellRef;
	CellPathReversed.Add(Current);
	
	const int32 MaxIterations = Grid->XCount * Grid->YCount + 10;
	int32 Iteration = 0;
	while (!(Current == OriginCell))
	{
		if (++Iteration > MaxIterations)
		{
			return false;
		}
		
		float CurrentDistance = INF;
		DistanceMap.GetValue(Current, CurrentDistance);
		
		FSFCellRef BestNext = FSFCellRef::Invalid;
		float BestDistance = CurrentDistance;
		
		const FSFCellRef Neighbors[4] = {
			FSFCellRef(Current.X + 1, Current.Y),
			FSFCellRef(Current.X - 1, Current.Y),
			FSFCellRef(Current.X, Current.Y + 1),
			FSFCellRef(Current.X, Current.Y - 1),
		};
		
		for (const FSFCellRef& N : Neighbors)
		{
			if (!IsTraversable(N)) continue;
			
			float ND = INF;
			if (!DistanceMap.GetValue(N, ND)) continue;
			
			if (ND < BestDistance)
			{
				BestNext = N;
				BestDistance = ND;
			}
		}
		
		if (!BestNext.IsValid())
		{
			return false;
		}
		
		Current = BestNext;
		CellPathReversed.Add(Current);
	}
	
	// reverses to get Origin -> .. -> End
	Algo::Reverse(CellPathReversed);
	
	// converts to unsmoothed steps
	TArray<FSFPathStep> UnsmoothedSteps;
	UnsmoothedSteps.Reserve(CellPathReversed.Num());
	
	int32 StartIndex = (CellPathReversed.Num() >0 && CellPathReversed[0] == OriginCell) ? 1 : 0;
	for (int32 i = StartIndex; i < CellPathReversed.Num(); ++i)
	{
		const FSFCellRef Cell = CellPathReversed[i];
		FSFPathStep Step;
		Step.Set(Grid->GetCellPosition(Cell), Cell);
		UnsmoothedSteps.Add(Step);
	}
	
	if (UnsmoothedSteps.Num() == 0)
	{
		bDistanceMapPathValid = true;
		State = SFPS_Finished;
		return true;
	}
	
	// Remember to smooth the path as well, using your existing smoothing code
	// Smooth the reconstructed path
	ESFPathState SmoothState = SmoothPath(OriginPoint, UnsmoothedSteps, Steps);
	if (SmoothState != SFPS_Active || Steps.Num() == 0)
	{
		return false;
	}

	bDistanceMapPathValid = true;

	if (bDistanceMapPathValid)
	{
		State = SFPS_Active;
	}

	return bDistanceMapPathValid;
}


ESFPathState USFPathComponent::SmoothPath(
	// Assignment 2 Part 4: smooth the path
	// High level description from the lecture:
	// * Trace to each subsequent step until you collide
	// * Back up one step (to the last clear one)
	// * Add that cell to your smoothed step
	// * Start again from there

    const FVector& StartPoint,
    const TArray<FSFPathStep>& UnsmoothedSteps,
    TArray<FSFPathStep>& SmoothedStepsOut) const
{
    SmoothedStepsOut.Reset();

    const ASFGridActor* Grid = GetGridActor();
    if (!Grid)
    {
        return SFPS_Invalid;
    }

    // --- helpers ---
    auto IsTraversable = [&](const FSFCellRef& Cell) -> bool
    {
        if (!Grid->IsCellRefInBounds(Cell)) return false;
        return EnumHasAllFlags(Grid->GetCellData(Cell), ESFCellData::CellDataTraversable);
    };
	
	// clearance-aware traversability (1-cell padding)
	// auto IsTraversableWithClearance = [&](const FSFCellRef& Cell) -> bool
	// {
	// 	// center cell must be traversable
	// 	if (!IsTraversable(Cell)) return false;
	//
	// 	// 1-cell clearance around it (8-neighborhood)
	// 	// (prevents smoothing from cutting corners too close to obstacles)
	// 	for (int dx = -1; dx <= 1; ++dx)
	// 	{
	// 		for (int dy = -1; dy <= 1; ++dy)
	// 		{
	// 			const FSFCellRef N(Cell.X + dx, Cell.Y + dy);
	// 			if (!IsTraversable(N))
	// 			{
	// 				return false;
	// 			}
	// 		}
	// 	}
	// 	return true;
	// };
	//
	
    // line trace: walk cells from A to B (inclusive) and ensure all traversable.
    auto LineTraceCells = [&](const FSFCellRef& A, const FSFCellRef& B) -> bool
    {
        if (!A.IsValid() || !B.IsValid()) return false;
    	if (!IsTraversable(A) || !IsTraversable(B)) return false;

        int x0 = A.X, y0 = A.Y;
        int x1 = B.X, y1 = B.Y;

        int dx = FMath::Abs(x1 - x0);
        int sx = (x0 < x1) ? 1 : -1;
        int dy = -FMath::Abs(y1 - y0);
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy;

        while (true)
        {
        	FSFCellRef C(x0, y0);
        	if (!IsTraversable(C))
        	{
        		return false;
        	}

            if (x0 == x1 && y0 == y1)
            {
                break;
            }

            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }

        return true;
    };

    // Build a cell list: [StartCell, ... path cells ... GoalCell] 
    const FSFCellRef StartCell = Grid->GetCellRef(StartPoint, /*bClamp=*/false);
    if (!StartCell.IsValid() || !IsTraversable(StartCell))
    {
        return SFPS_Invalid;
    }

    if (UnsmoothedSteps.Num() == 0)
    {
        return SFPS_Invalid;
    }

    TArray<FSFCellRef> Cells;
    Cells.Reserve(UnsmoothedSteps.Num() + 1);
    Cells.Add(StartCell);

    for (const FSFPathStep& Step : UnsmoothedSteps)
    {
        if (!Step.CellRef.IsValid())
        {
            return SFPS_Invalid;
        }
        Cells.Add(Step.CellRef);
    }

    // Smoothing: greedy "farthest visible" 
    int32 i = 0; // index of current anchor in Cells
    while (i < Cells.Num() - 1)
    {
        int32 BestJ = i + 1;

        // find farthest j we can see directly from i
        for (int32 j = i + 1; j < Cells.Num(); j++)
        {
            if (LineTraceCells(Cells[i], Cells[j]))
            {
                BestJ = j;
            }
            // else
            // {
            //     break;
            // }
        }

        {
            const FSFCellRef& Cell = Cells[BestJ];
            FVector P = Grid->GetCellPosition(Cell);

            FSFPathStep Out;
            Out.Set(P, Cell);
            SmoothedStepsOut.Add(Out);
        }

        i = BestJ;
    }

    return SFPS_Active;
}

void USFPathComponent::FollowPath()
{
	AActor* Owner = GetOwnerPawn();
	if (Owner == NULL)
	{
		return;
	}

	FVector StartPoint = Owner->GetActorLocation();

	check(State == SFPS_Active);
	check(Steps.Num() > 0);
	
	// Always follow the first step
	FVector V = Steps[0].Point - StartPoint;
	V.Normalize();

	UNavMovementComponent* MovementComponent = Owner->FindComponentByClass<UNavMovementComponent>();
	if (MovementComponent)
	{
		MovementComponent->RequestPathMove(V);
	}
}

ESFPathState USFPathComponent::SetDestination(const FVector &DestinationPoint)
{
	Destination = DestinationPoint;

	State = SFPS_Invalid;
	bDestinationValid = true;

	const ASFGridActor* Grid = GetGridActor();
	if (Grid)
	{
		FSFCellRef CellRef = Grid->GetCellRef(Destination);
		if (CellRef.IsValid())
		{
			DestinationCell = CellRef;
			bDestinationValid = true;

			RefreshPath();
		}
	}

	return State;
}