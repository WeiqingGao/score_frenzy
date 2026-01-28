#include "GAPathComponent.h"
#include "GameFramework/NavMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UGAPathComponent::UGAPathComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	State = GAPS_None;
	bDestinationValid = false;
	ArrivalDistance = 100.0f;

	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;
}

const AGAGridActor* UGAPathComponent::GetGridActor() const
{
	if (GridActor.Get())
	{
		return GridActor.Get();
	}
	else
	{
		AGAGridActor* Result = NULL;
		AActor *GenericResult = UGameplayStatics::GetActorOfClass(this, AGAGridActor::StaticClass());
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

APawn* UGAPathComponent::GetOwnerPawn()
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


void UGAPathComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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
		if (State == GAPS_Active)
		{
			FollowPath();
		}
	}

	// Super important! Otherwise, unbelievably, the Tick event in Blueprint won't get called

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

EGAPathState UGAPathComponent::RefreshPath()
{
	AActor* Owner = GetOwnerPawn();
	if (Owner == NULL)
	{
		State = GAPS_Invalid;
		return State;
	}

	FVector StartPoint = Owner->GetActorLocation();

	check(bDestinationValid);

	float DistanceToDestination = FVector::Dist(StartPoint, Destination);

	if (DistanceToDestination <= ArrivalDistance)
	{
		// Yay! We got there!
		State = GAPS_Finished;
	}
	else
	{
		TArray<FPathStep> UnsmoothedSteps;
		Steps.Empty();

		// Replan the path!
		State = AStar(StartPoint, UnsmoothedSteps);

		// To debug A* without smoothing uncomment this line and then skip the call to SmoothPath below:
		Steps = UnsmoothedSteps;

		if (State == EGAPathState::GAPS_Active)
		{
			// Smooth the path!
			State = SmoothPath(StartPoint, UnsmoothedSteps, Steps);
		}
	}

	return State;
}

EGAPathState UGAPathComponent::AStar(const FVector& StartPoint, TArray<FPathStep>& StepsOut) const
{
	// Assignment 2 Part3: replace this with an A* search!
	// HINT 1: you made need a heap structure. A TArray can be accessed as a heap -- just add/remove elements using
	// the TArray::HeapPush() and TArray::HeapPop() methods.
	// Note that whatever you push or pop needs to implement the 'less than' operator (operator<)
	// HINT 2: UE has some useful flag testing function. For example you can test for traversability by doing this:
	// ECellData Flags = Grid->GetCellData(CellRef);
	// bool bIsCellTraversable = EnumHasAllFlags(Flags, ECellData::CellDataTraversable)
	//	StepsOut.SetNum(1);
	//	StepsOut[0].Set(Destination, DestinationCell);

	// HINT 3: make sure you return the correct status, based on whether you succeeded to find a path or not.
	// See the comment in GAPathComponent above the EGAPathState enum
    StepsOut.Reset();

    const AGAGridActor* Grid = GetGridActor();
    if (!Grid)
    {
        return GAPS_Invalid;
    }

    const FCellRef StartCell = Grid->GetCellRef(StartPoint, /*bClamp=*/false);
    const FCellRef GoalCell  = DestinationCell;

    if (!StartCell.IsValid() || !GoalCell.IsValid())
    {
        return GAPS_Invalid;
    }

    if (StartCell == GoalCell)
    {
        StepsOut.SetNum(1);
        StepsOut[0].Set(Destination, GoalCell);
        return GAPS_Active;
    }

    auto IsTraversable = [&](const FCellRef& Cell) -> bool
    {
        if (!Grid->IsCellRefInBounds(Cell)) return false;
        return EnumHasAllFlags(Grid->GetCellData(Cell), ECellData::CellDataTraversable);
    };

    if (!IsTraversable(GoalCell) || !IsTraversable(StartCell))
    {
        return GAPS_Invalid;
    }

    auto Heuristic = [&](const FCellRef& Cell) -> float
    {
        // Euclidean in cell space
        return Cell.Distance(GoalCell);
    };

    struct FOpenNode
    {
        FCellRef Cell;
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

    TMap<FCellRef, float> GScore;
    TMap<FCellRef, FCellRef> CameFrom;
    TSet<FCellRef> Closed;

    // init start
    {
        GScore.Add(StartCell, 0.0f);

        FOpenNode N;
        N.Cell = StartCell;
        N.G = 0.0f;
        N.F = Heuristic(StartCell);
        OpenHeap.HeapPush(N);
    }

    auto RelaxNeighbor = [&](const FCellRef& Current, float CurrentBestG, const FCellRef& Neighbor)
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
        const FCellRef Current = CurrentNode.Cell;
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

        RelaxNeighbor(Current, *BestG, FCellRef(Current.X + 1, Current.Y));
        RelaxNeighbor(Current, *BestG, FCellRef(Current.X - 1, Current.Y));
        RelaxNeighbor(Current, *BestG, FCellRef(Current.X, Current.Y + 1));
        RelaxNeighbor(Current, *BestG, FCellRef(Current.X, Current.Y - 1));
    }

    if (!bFound)
    {
        return GAPS_Invalid;
    }

    // reconstruct (Goal -> ... -> Start)
    TArray<FCellRef> CellPath;
    {
        FCellRef Cur = GoalCell;
        CellPath.Add(Cur);

        while (!(Cur == StartCell))
        {
            FCellRef* Parent = CameFrom.Find(Cur);
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
        const FCellRef& Cell = CellPath[i];
        FPathStep Step;
        Step.Set(Grid->GetCellPosition(Cell), Cell);
        StepsOut.Add(Step);
    }

    return GAPS_Active;
}

bool UGAPathComponent::Dijkstra(const FVector& StartPoint, FGAGridMap& DistanceMapOut) const
{
	// Assignment 3 Part 3-1: implement Dijkstra's algorithm to fill out the distance map
	return false;
}

bool UGAPathComponent::BuidPathFromDistanceMap(const FVector& StartPoint, const FCellRef& StartCellRef, const FGAGridMap& DistanceMap)
{
	bDistanceMapPathValid = false;

	// Assignment 3 Part 3-2: reconstruct a path from the distance map

	// Remember to smooth the path as well, using your existing smoothing code

	// Set this to true when you've successfully built the path
	// bDistanceMapPathValid = true;

	if (bDistanceMapPathValid)
	{
		// once you have built the path (i.e. filled in the Steps array in the GAPathComponent), set the path component's state to GAPS_Active
		// This will cause 
		State = GAPS_Active;
	}

	return bDistanceMapPathValid;
}


EGAPathState UGAPathComponent::SmoothPath(
	// Assignment 2 Part 4: smooth the path
	// High level description from the lecture:
	// * Trace to each subsequent step until you collide
	// * Back up one step (to the last clear one)
	// * Add that cell to your smoothed step
	// * Start again from there

    const FVector& StartPoint,
    const TArray<FPathStep>& UnsmoothedSteps,
    TArray<FPathStep>& SmoothedStepsOut) const
{
    SmoothedStepsOut.Reset();

    const AGAGridActor* Grid = GetGridActor();
    if (!Grid)
    {
        return GAPS_Invalid;
    }

    // --- helpers ---
    auto IsTraversable = [&](const FCellRef& Cell) -> bool
    {
        if (!Grid->IsCellRefInBounds(Cell)) return false;
        return EnumHasAllFlags(Grid->GetCellData(Cell), ECellData::CellDataTraversable);
    };
	
	// clearance-aware traversability (1-cell padding)
	auto IsTraversableWithClearance = [&](const FCellRef& Cell) -> bool
	{
		// center cell must be traversable
		if (!IsTraversable(Cell)) return false;

		// 1-cell clearance around it (8-neighborhood)
		// (prevents smoothing from cutting corners too close to obstacles)
		for (int dx = -1; dx <= 1; ++dx)
		{
			for (int dy = -1; dy <= 1; ++dy)
			{
				const FCellRef N(Cell.X + dx, Cell.Y + dy);
				if (!IsTraversable(N))
				{
					return false;
				}
			}
		}
		return true;
	};
	
    // line trace: walk cells from A to B (inclusive) and ensure all traversable.
    auto LineTraceCells = [&](const FCellRef& A, const FCellRef& B) -> bool
    {
        if (!A.IsValid() || !B.IsValid()) return false;
    	if (!IsTraversableWithClearance(A) || !IsTraversableWithClearance(B)) return false;

        int x0 = A.X, y0 = A.Y;
        int x1 = B.X, y1 = B.Y;

        int dx = FMath::Abs(x1 - x0);
        int sx = (x0 < x1) ? 1 : -1;
        int dy = -FMath::Abs(y1 - y0);
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy;

        while (true)
        {
        	FCellRef C(x0, y0);
        	if (!IsTraversableWithClearance(C))
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
    const FCellRef StartCell = Grid->GetCellRef(StartPoint, /*bClamp=*/false);
    if (!StartCell.IsValid() || !IsTraversable(StartCell))
    {
        return GAPS_Invalid;
    }

    if (UnsmoothedSteps.Num() == 0)
    {
        return GAPS_Invalid;
    }

    TArray<FCellRef> Cells;
    Cells.Reserve(UnsmoothedSteps.Num() + 1);
    Cells.Add(StartCell);

    for (const FPathStep& Step : UnsmoothedSteps)
    {
        if (!Step.CellRef.IsValid())
        {
            return GAPS_Invalid;
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
            else
            {
                break;
            }
        }

        {
            const FCellRef& Cell = Cells[BestJ];
            FVector P = Grid->GetCellPosition(Cell);

            FPathStep Out;
            Out.Set(P, Cell);
            SmoothedStepsOut.Add(Out);
        }

        i = BestJ;
    }

    return GAPS_Active;
}

void UGAPathComponent::FollowPath()
{
	AActor* Owner = GetOwnerPawn();
	if (Owner == NULL)
	{
		return;
	}

	FVector StartPoint = Owner->GetActorLocation();

	check(State == GAPS_Active);
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

EGAPathState UGAPathComponent::SetDestination(const FVector &DestinationPoint)
{
	Destination = DestinationPoint;

	State = GAPS_Invalid;
	bDestinationValid = true;

	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		FCellRef CellRef = Grid->GetCellRef(Destination);
		if (CellRef.IsValid())
		{
			DestinationCell = CellRef;
			bDestinationValid = true;

			RefreshPath();
		}
	}

	return State;
}