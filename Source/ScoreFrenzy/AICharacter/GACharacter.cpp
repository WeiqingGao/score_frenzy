// Copyright Epic Games, Inc. All Rights Reserved.

#include "GACharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY(LogTemplateAICharacter);

//////////////////////////////////////////////////////////////////////////
// AGACharacter

AGACharacter::AGACharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Configure character rotation
	// Should the character rotate towards the direction of movement?
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	// ... or should it take rotation from its controller?
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 200.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	MoveFrequency = 1.5f;
	MoveAmplitude = 1.0f;

	MaxHealth = 100.0f;
	Health = 100.0f;
	LowHealthThreshold = 0.3f;
	HealRate = 10.0f;
	bInCombat = false;
	HealingState = EHealingState::Normal;
}

void AGACharacter::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
}

void AGACharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Regenerate health only when in Healing state and not actively in combat
	if (HealingState == EHealingState::Healing && !bInCombat)
	{
		Health = FMath::Min(Health + HealRate * DeltaSeconds, MaxHealth);
		if (Health >= MaxHealth)
		{
			ExitHealingState();
		}
	}
}

void AGACharacter::EnterHealingState()
{
	HealingState = EHealingState::Healing;
	bInCombat = false;
}

void AGACharacter::ExitHealingState()
{
	HealingState = EHealingState::Normal;
}

void AGACharacter::SetInCombat(bool bCombat)
{
	bInCombat = bCombat;
}

bool AGACharacter::IsLowHealth() const
{
	return (MaxHealth > 0.0f) && ((Health / MaxHealth) <= LowHealthThreshold);
}