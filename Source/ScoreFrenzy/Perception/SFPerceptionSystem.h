#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFPerceptionComponent.h"
#include "SFTargetComponent.h"
#include "SFPerceptionSystem.generated.h"



UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class USFPerceptionSystem : public UActorComponent
{
	GENERATED_UCLASS_BODY()


	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<USFPerceptionComponent>> PerceptionComponents;

	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<USFTargetComponent>> TargetComponents;


	bool RegisterPerceptionComponent(USFPerceptionComponent* PerceptionComponent);
	bool UnregisterPerceptionComponent(USFPerceptionComponent* PerceptionComponent);

	bool RegisterTargetComponent(USFTargetComponent* TargetComponent);
	bool UnregisterTargetComponent(USFTargetComponent* TargetComponent);

	TArray<TObjectPtr<USFTargetComponent>>& GetAllTargetComponents() { return TargetComponents; }
	TArray<TObjectPtr<USFPerceptionComponent>>& GetAllPerceptionComponents() { return PerceptionComponents; }

	static USFPerceptionSystem* GetPerceptionSystem(const UObject* WorldContextObject);

};