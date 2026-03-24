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
	APawn* Pawn = GetOwnerPawn();
	if (Pawn == NULL)
	{
		return;
	}

	FTargetView *TargetView = TargetMap.Find(TargetComponent->TargetGuid);
	if (TargetView == NULL)
	{
		FTargetView NewTargetView;
		FGuid TargetGuid = TargetComponent->TargetGuid;
		TargetView = &TargetMap.Add(TargetGuid, NewTargetView);
	}

	if (TargetView)
	{
		AActor* Target = TargetComponent->GetOwner();
		const FVector TargetLocation = Target->GetActorLocation();
		const FVector SelfLocation = Pawn->GetActorLocation();
		const float Distance = FVector::Distance(SelfLocation, TargetLocation);
		bool bWithinVisionDistance = Distance <= VisionParameters.VisionDistance;

		enum class EVisionZone { Front, Peripheral, None };
		EVisionZone VisionZone = EVisionZone::None;

		if (bWithinVisionDistance)
		{
			const FVector ForwardVector = Pawn->GetActorForwardVector();
			const FVector AIToTargetVectorUnit = (TargetLocation - SelfLocation).GetSafeNormal();
			float CosineTargetAndForward = FVector::DotProduct(ForwardVector, AIToTargetVectorUnit);
			float CosineHalfFrontVisionAngle = FMath::Cos(FMath::DegreesToRadians(VisionParameters.FrontVisionAngle * 0.5f));
			float CosineHalfPeripheralVisionAngle = FMath::Cos(FMath::DegreesToRadians(VisionParameters.PeripheralVisionAngle * 0.5f));

			if (CosineTargetAndForward >= CosineHalfFrontVisionAngle)
			{
				VisionZone = EVisionZone::Front;
			}
			else if (CosineTargetAndForward >= CosineHalfPeripheralVisionAngle)
			{
				VisionZone = EVisionZone::Peripheral;
			}
		}

		bool bClearLOSNow = false;
		if (VisionZone != EVisionZone::None)
		{
			FVector RaycastStartLocation;
			FRotator ViewRotation;
			Pawn->GetActorEyesViewPoint(RaycastStartLocation, ViewRotation);

			ACharacter* TargetCharacter = Cast<ACharacter>(Target);
			FVector RaycastEndLocation = TargetLocation;
			if (TargetCharacter)
			{
				UCapsuleComponent* Capsule = TargetCharacter->GetCapsuleComponent();
				FVector TargetTop = Capsule->GetComponentLocation() + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
				RaycastEndLocation = TargetTop;
			}

			FHitResult HitResult;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_LOS));
			Params.AddIgnoredActor(Pawn);
			Params.bTraceComplex = true;
			bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, RaycastStartLocation, RaycastEndLocation, ECC_Visibility, Params);
			bClearLOSNow = (!bHit || HitResult.GetActor() == Target);
		}

		TargetView->bClearLos = bClearLOSNow;

		float RiseRate = 0.0f;
		const float FallRate = 0.5f;
		if (bClearLOSNow)
		{
			RiseRate = (VisionZone == EVisionZone::Front) ? 1.6f : 1.0f;
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

bool UGAPerceptionComponent::TestVisibility(const FCellRef& Cell) const
{
	const AGAGridActor* Grid = Cast<AGAGridActor>(UGameplayStatics::GetActorOfClass(GetWorld(), AGAGridActor::StaticClass()));
	if (!Grid) return false;

	APawn* Pawn = GetOwnerPawn();
	if (!Pawn) return false;

	FVector RaycastStartLocation;
	FRotator ViewRotation;
	Pawn->GetActorEyesViewPoint(RaycastStartLocation, ViewRotation);

	FVector CellLocation = Grid->GetCellPosition(Cell);

	FHitResult HitResult;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_LOS));
	Params.AddIgnoredActor(Pawn);
	Params.bTraceComplex = true;

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, RaycastStartLocation, CellLocation, ECC_Visibility, Params);
	return !bHit;
}
