#include "SFSoldierCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

ASFSoldierCharacter::ASFSoldierCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentHealth = MaxHealth;
}

void ASFSoldierCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
}

void ASFSoldierCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsHealing && !IsDead())
	{
		CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + HealRate * DeltaSeconds);
	}
}

float ASFSoldierCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (IsDead()) return 0.0f;

	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	if (IsDead())
	{
		bIsHealing = false;
		SetActorEnableCollision(false);
		GetCharacterMovement()->DisableMovement();
		OnDeath.Broadcast();
	}

	return DamageAmount;
}

bool ASFSoldierCharacter::IsLowHealth() const
{
	return MaxHealth > 0.0f && (CurrentHealth / MaxHealth) <= LowHealthThreshold;
}

bool ASFSoldierCharacter::IsDead() const
{
	return CurrentHealth <= 0.0f;
}

float ASFSoldierCharacter::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

void ASFSoldierCharacter::StartHealing()
{
	bIsHealing = true;
}

void ASFSoldierCharacter::StopHealing()
{
	bIsHealing = false;
}
