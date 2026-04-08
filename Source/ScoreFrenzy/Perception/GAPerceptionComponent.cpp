#include "GAPerceptionComponent.h" 
#include "Kismet/GameplayStatics.h" 
#include "GAPerceptionSystem.h" 
#include "ScoreFrenzy/Grid/GAGridActor.h" 
#include "GameFramework/Character.h" 
#include "Components/CapsuleComponent.h"


UGAPerceptionComponent::UGAPerceptionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;
}


void UGAPerceptionComponent::OnRegister()
{
	Super::OnRegister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->RegisterPerceptionComponent(this);
	}
}

void UGAPerceptionComponent::OnUnregister()
{
	Super::OnUnregister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->UnregisterPerceptionComponent(this);
	}
}


APawn* UGAPerceptionComponent::GetOwnerPawn() const
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



// Returns the Target this AI is attending to right now.

UGATargetComponent* UGAPerceptionComponent::GetCurrentTarget() const
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);

	if (PerceptionSystem && PerceptionSystem->TargetComponents.Num() > 0)
	{
		UGATargetComponent* TargetComponent = PerceptionSystem->TargetComponents[0];
		/*if (TargetComponent->IsKnown())
		{
			return PerceptionSystem->TargetComponents[0];
		}*/
		const FTargetView* TargetView = TargetMap.Find(TargetComponent->TargetGuid);
		if (TargetView && TargetView->bClearLos && TargetView->Awareness >= 1.0f)
		{
			return TargetComponent;
		}

	}

	return NULL;
}

bool UGAPerceptionComponent::HasTarget() const
{
	return GetCurrentTarget() != NULL;
}


bool UGAPerceptionComponent::GetCurrentTargetState(FTargetState& TargetStateOut, FTargetView& TargetViewOut) const
{
	UGATargetComponent* Target = GetCurrentTarget();
	if (Target)
	{
		const FTargetView* TargetView = TargetMap.Find(Target->TargetGuid);
		if (TargetView)
		{
			TargetStateOut = Target->LastKnownState;
			TargetViewOut = *TargetView;
			return true;
		}

	}
	return false;
}


void UGAPerceptionComponent::GetAllTargetStates(bool OnlyKnown, TArray<FTargetState>& TargetStatesOut, TArray<FTargetView>& TargetViewsOut) const
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGATargetComponent>>& TargetComponents = PerceptionSystem->GetAllTargetComponents();
		for (UGATargetComponent* TargetComponent : TargetComponents)
		{
			const FTargetView* TargetView = TargetMap.Find(TargetComponent->TargetGuid);
			if (TargetView)
			{
				if (!OnlyKnown || TargetComponent->IsKnown())
				{
					TargetStatesOut.Add(TargetComponent->LastKnownState);
					TargetViewsOut.Add(*TargetView);
				}
			}
		}
	}
}


void UGAPerceptionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateAllTargetViews();
}


void UGAPerceptionComponent::UpdateAllTargetViews()
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGATargetComponent>> TargetComponents = PerceptionSystem->GetAllTargetComponents();
		for (UGATargetComponent* TargetComponent : TargetComponents)
		{
			if (!TargetComponent || !IsValid(TargetComponent)) continue;
			UpdateTargetView(TargetComponent);
		}
	}
}

void UGAPerceptionComponent::UpdateTargetView(UGATargetComponent* TargetComponent)
{
	if (!TargetComponent || !IsValid(TargetComponent)) return;

	// REMEMBER: the UGAPerceptionComponent is going to be attached to the controller, not the pawn. So we call this special accessor to
	// get the pawn that our controller is controlling
	APawn* Pawn = GetOwnerPawn();
	if (Pawn == NULL)
	{
		return;
	}

	FTargetView *TargetView = TargetMap.Find(TargetComponent->TargetGuid);
	if (TargetView == NULL)		// If we don't already have a target data for the given target component, add it
	{
		FTargetView NewTargetView;
		FGuid TargetGuid = TargetComponent->TargetGuid;
		TargetView = &TargetMap.Add(TargetGuid, NewTargetView);
	}

	if (TargetView)
	{
		// TODO PART 3
		// 
		// - Update TargetView->bClearLOS
		//		Use this.VisionParameters to determine whether the target is within the vision cone or not 
		//		(and ideally do so before you cast a ray towards it)
		// - Update TargetView->Awareness
		//		On ticks when the AI has a clear LOS, the Awareness should grow
		//		On ticks when the AI does not have a clear LOS, the Awareness should decay
		//
		// Awareness should be clamped to the range [0, 1]
		// You can add parameters to the UGAPerceptionComponent to control the speed at which awareness rises and falls

		// YOUR CODE HERE
		// 1. tests if within vision distance
		// 1-1 gets the target's current location
		AActor* Target = TargetComponent->GetOwner();
		if (!Target || !IsValid(Target)) return;
		const FVector TargetLocation = Target->GetActorLocation();
		// 1-2 gets the AI's current location
		const FVector SelfLocation = Pawn->GetActorLocation();
		// 1-3 calculates the actual distance
		const float Distance = FVector::Distance(SelfLocation, TargetLocation);
		// 1-4 determines if within vision distance
		bool bWithinVisionDistance = Distance <= VisionParameters.VisionDistance;
		
		// improvement part
		// 2. determines which zone the target is in
		// creates an Enumeration represents the possible zones the target may be in
		enum class EVisionZone {Front, Peripheral, Rear, None};
		EVisionZone VisionZone = EVisionZone::None;

		if (bWithinVisionDistance)
		{
			// 2-1 gets the AI's current forward direction
			const FVector ForwardVector = Pawn->GetActorForwardVector();
			// 2-2 gets the vector from AI to the target
			const FVector AIToTargetVector = TargetLocation - SelfLocation;
			// 2-3 calculates the cosine value of the angle between these two vectors
			// 2-3-1 gets the unit vectors of these two vectors
			const FVector AIToTargetVectorUnit = AIToTargetVector.GetSafeNormal();
			float CosineTargetAndForward = FVector::DotProduct(ForwardVector, AIToTargetVectorUnit);
			// 2-4 calculates the cosine value of half of the FrontVisionAngle and PeripheralVisionAngle
			float CosineHalfFrontVisionAngle = FMath::Cos(FMath::DegreesToRadians(VisionParameters.FrontVisionAngle * 0.5f));
			float CosineHalfPeripheralVisionAngle = FMath::Cos(FMath::DegreesToRadians(VisionParameters.PeripheralVisionAngle * 0.5f));
			// 2-5 determines which zone the target is in based on cosine value comparison
			if (CosineTargetAndForward >= CosineHalfFrontVisionAngle)
			{
				VisionZone = EVisionZone::Front;
			}
			else if (CosineTargetAndForward >= CosineHalfPeripheralVisionAngle)
			{
				VisionZone = EVisionZone::Peripheral;
			}
			else
			{
				VisionZone = EVisionZone::None;
			}
		}
		
		// 3. LOS test
		bool bClearLOSNow = false;
		if (VisionZone != EVisionZone::None)
		{
			// 3-1 gets the starting point of raycast
			FVector RaycastStartLocation;
			FRotator ViewRotation;
			Pawn->GetActorEyesViewPoint(RaycastStartLocation, ViewRotation);
			
			// 3-2 gets the end point of raycast
			// 3-2-1 gets the target's character
			ACharacter* TargetCharacter = Cast<ACharacter>(Target);
			FVector RaycastEndLocation = TargetLocation;
			if (TargetCharacter)
			{
				// 3-2-2 makes the top of target's head as the end point,
				//       since on this map, if the target is visible, the top of the target's head will also be visible
				UCapsuleComponent* Capsule = TargetCharacter->GetCapsuleComponent();
				if (Capsule)
				{
					// the middle position of the capsule
					FVector TargetCenter = Capsule->GetComponentLocation();
					float TargetHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

					FVector TargetTop = TargetCenter + FVector(0,0,TargetHalfHeight);
					RaycastEndLocation = TargetTop;
				}
			}

			// 3-3 single line trace (raycast) by channel
			UWorld* World = GetWorld();
			if (!World) return;
			FHitResult HitResult;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_LOS));
			// ignore itself
			Params.AddIgnoredActor(Pawn);
			Params.bTraceComplex = true;
			const ECollisionChannel Channel = ECC_Visibility;

			const bool bHit = World->LineTraceSingleByChannel(HitResult, RaycastStartLocation, RaycastEndLocation, Channel, Params);
			
			if (!bHit || HitResult.GetActor() == Target)
			{
				bClearLOSNow = true;
			}
			else
			{
				bClearLOSNow = false;
			}
		}
		
		// 4.updates Awareness
		TargetView->bClearLos = bClearLOSNow;

		UWorld* CurrentWorld = GetWorld();
		if (!CurrentWorld) return;

		// 4-1 sets the rise rate and fall rate
		float RiseRate = 0.0f;
		float FallRate = 0.5f;

		// 4-2 sets the rise rate based on vision zone, then clamps the awareness meter
		if (bClearLOSNow)
		{
			switch (VisionZone)
			{
				case EVisionZone::Front:
					RiseRate = 1.6f;
					break;
				case EVisionZone::Peripheral:
					RiseRate = 1.0f;
					break;
				case EVisionZone::Rear:
					RiseRate = 0.1f;
					break;
				default:
					break;
			}
			TargetView->Awareness = FMath::Clamp(TargetView->Awareness + RiseRate * CurrentWorld->GetDeltaSeconds(), 0.f, 1.f);
		}
		else
		{
			TargetView->Awareness = FMath::Clamp(TargetView->Awareness - FallRate * CurrentWorld->GetDeltaSeconds(), 0.f, 1.f);
		}

	}
}


const FTargetView* UGAPerceptionComponent::GetTargetView(FGuid TargetGuid) const
{
	return TargetMap.Find(TargetGuid);
}

bool UGAPerceptionComponent::TestVisibility(const FCellRef& Cell) const
{
	// // 1. tests if within vision distance
	// // 1-1 transfer this cell's ref to world space since it is continuous, and thus more accurate
	UWorld* World = GetWorld();
	if (!World) return false;
	const AGAGridActor* Grid = Cast<AGAGridActor>(UGameplayStatics::GetActorOfClass(World, AGAGridActor::StaticClass()));
	if (!Grid) return false;
	const FVector thisLocation = Grid->GetCellPosition(Cell);
	// // 1-2 gets the AI's current location
	APawn* Pawn = GetOwnerPawn();
	if (!Pawn || !IsValid(Pawn)) return false;
	
	// 3. LOS test
	// 3-1 gets the starting point of raycast
	FVector RaycastStartLocation;
	FRotator ViewRotation;
	Pawn->GetActorEyesViewPoint(RaycastStartLocation, ViewRotation);
	
	// 3-3 single line trace (raycast) by channel
	FHitResult HitResult;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_LOS));
	// ignore itself
	Params.AddIgnoredActor(Pawn);
	Params.bTraceComplex = true;
	const ECollisionChannel Channel = ECC_Visibility;

	const bool bHit = World->LineTraceSingleByChannel(HitResult, RaycastStartLocation, thisLocation, Channel, Params);

	return !bHit;
}
