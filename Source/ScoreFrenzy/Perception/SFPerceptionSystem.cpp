#include "SFPerceptionSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"

USFPerceptionSystem::USFPerceptionSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{


}


bool USFPerceptionSystem::RegisterPerceptionComponent(USFPerceptionComponent* PerceptionComponent)
{
	PerceptionComponents.AddUnique(PerceptionComponent);
	return true;
}

bool USFPerceptionSystem::UnregisterPerceptionComponent(USFPerceptionComponent* PerceptionComponent)
{
	return PerceptionComponents.Remove(PerceptionComponent) > 0;
}


bool USFPerceptionSystem::RegisterTargetComponent(USFTargetComponent* TargetComponent)
{
	TargetComponents.AddUnique(TargetComponent);
	return true;
}

bool USFPerceptionSystem::UnregisterTargetComponent(USFTargetComponent* TargetComponent)
{
	return TargetComponents.Remove(TargetComponent) > 0;
}


USFPerceptionSystem* USFPerceptionSystem::GetPerceptionSystem(const UObject* WorldContextObject)
{
	USFPerceptionSystem* Result = NULL;
	AGameModeBase *GameMode = UGameplayStatics::GetGameMode(WorldContextObject);
	if (GameMode)
	{
		Result = GameMode->GetComponentByClass<USFPerceptionSystem>();
	}

	return Result;
}
