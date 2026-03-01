#include "GAPerceptionComponent.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GAPerceptionSystem.h"
#include "Components/CapsuleComponent.h"
#include "Concepts/Iterable.h"
#include "GameFramework/Character.h"

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
		if (TargetComponent->IsKnown())
		{
			return PerceptionSystem->TargetComponents[0];
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
		TArray<TObjectPtr<UGATargetComponent>>& TargetComponents = PerceptionSystem->GetAllTargetComponents();
		for (UGATargetComponent* TargetComponent : TargetComponents)
		{
			UpdateTargetView(TargetComponent);
		}
	}
}

void UGAPerceptionComponent::UpdateTargetView(UGATargetComponent* TargetComponent)
{
	// REMEMBER: the UGAPerceptionComponent is going to be attached to the controller, not the pawn. So we call this special accessor to 
	// get the pawn that our controller is controlling
	APawn* OwnerPawn = GetOwnerPawn();
	if (OwnerPawn == NULL)
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
		const FVector TargetLocation = Target->GetActorLocation();
		// 1-2 gets the AI's current location
		const FVector SelfLocation = Target->GetActorLocation();
		// 1-3 calculates the actual distance
		const float Distance = FVector::Distance(SelfLocation, TargetLocation);
		// 1-4 determines if within vision distance
		bool bWithinVisionDistance = Distance <= VisionParameters.VisionDistance;
		
		// 2. tests if within the vision field
		bool bWithinVisionField = false;
		if (bWithinVisionDistance)
		{
			// 2-1 gets the AI's current forward direction
			const FVector ForwardVector = OwnerPawn->GetActorForwardVector();
			// 2-2 gets the vector from AI to the target
			const FVector AIToTargetVector = TargetLocation - SelfLocation;
			// 2-3 calculates the cosine value of the angle between these two vectors
			// 2-3-1 gets the unit vectors of these two vectors
			const FVector AIToTargetVectorUnit = AIToTargetVector.GetSafeNormal();
			float CosineTargetAndForward = FVector::DotProduct(ForwardVector, AIToTargetVectorUnit);
			// 2-4 calculates the cosine value of half of the VisionAngle
			float CosineHalfVisionAngle = FMath::Cos(FMath::DegreesToRadians(VisionParameters.VisionAngle * 0.5f));
			// 2-5 determines if within vision field
			bWithinVisionField = CosineTargetAndForward >= CosineHalfVisionAngle;
		}
		
		// 3. LOS test
		bool bClearLOSNow = false;
		if (bWithinVisionDistance && bWithinVisionField)
		{
			// 3-1 gets the starting point of raycast
			FVector RaycastStartLocation;
			FRotator ViewRotation;
			OwnerPawn->GetActorEyesViewPoint(RaycastStartLocation, ViewRotation);
			
			// 3-2 gets the end point of raycast
			// 3-2-1 gets the target's character
			ACharacter* TargetCharacter = Cast<ACharacter>(Target);
			FVector RaycastEndLocation;
			if (TargetCharacter)
			{
				// 3-2-2 makes the top of target's head as the end point,
				//       since on this map, if the target is visible, the top of the target's head will also be visible
				UCapsuleComponent* Capsule = TargetCharacter->GetCapsuleComponent();
				// the middle position of the capsule 
				FVector TargetCenter = Capsule->GetComponentLocation();
				float TargetHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

				FVector TargetTop = TargetCenter + FVector(0,0,TargetHalfHeight);
				RaycastEndLocation = TargetTop;
			}

			// 3-3 single line trace (raycast) by channel
			FHitResult HitResult;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_LOS));
			// ignore itself
			Params.AddIgnoredActor(OwnerPawn);
			Params.bTraceComplex = true;
			const ECollisionChannel Channel = ECC_Visibility;

			const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, RaycastStartLocation, RaycastEndLocation, Channel, Params);
			
			if (HitResult.GetActor() == Target)
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
		
		// 4-1 sets the rise rate and fall rate
		const float RiseRate = 0.8f;
		const float FallRate = 0.4f;
		
		// 4-2 clamps the awareness meter
		if (bClearLOSNow)
		{
			TargetView->Awareness = FMath::Clamp(TargetView->Awareness + RiseRate * GetWorld()->GetDeltaSeconds(), 0.f, 1.f);
		}
		else
		{
			TargetView->Awareness = FMath::Clamp(TargetView->Awareness - FallRate * GetWorld()->GetDeltaSeconds(), 0.f, 1.f);
		}
	}
}


const FTargetView* UGAPerceptionComponent::GetTargetView(FGuid TargetGuid) const
{
	return TargetMap.Find(TargetGuid);
}
