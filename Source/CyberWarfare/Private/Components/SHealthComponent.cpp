// Fill out your copyright notice in the Description page of Project Settings.

#include "../../Public/Components/SHealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "CyberWarfare.h"


// Sets default values for this component's properties
USHealthComponent::USHealthComponent()
{
	DefaultHealth = 100.f;
	DefaultShield = 100.f;
	ShieldRegenRate = 1.f;
	TimeBeforeShieldRegen = 5.f;

	TimeWithoutTakingDamage = 0.f;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SetIsReplicated(true);
}


// Called when the game starts
void USHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() == ROLE_Authority)
	{
		AActor* MyOwner = GetOwner();
		if (MyOwner)
		{
			MyOwner->OnTakeAnyDamage.AddDynamic(this, &USHealthComponent::HandleTakeAnyDamage);
		}
	}

	Health = DefaultHealth;
	Shield = DefaultShield;
}


// Tick for our shield
void USHealthComponent::TickShield(float DeltaTime)
{
	if (ShieldRegenRate > 0.f)
	{
		// Increment our time without taking any damage
		TimeWithoutTakingDamage += DeltaTime;

		// If we haven't taken damage for long enough, start regenerating shield
		if (TimeWithoutTakingDamage >= TimeBeforeShieldRegen)
		{
			// Update shield
			Shield = FMath::Clamp(Shield + ShieldRegenRate, 0.f, DefaultShield);

			// Set our time so we have to wait 0.25 second again before starting to regen shield
			TimeWithoutTakingDamage = TimeBeforeShieldRegen - 0.25f;
		}
	}
}


// Handle take damage
void USHealthComponent::HandleTakeAnyDamage(AActor * DamagedActor, float Damage, const UDamageType * DamageType, AController * InstigatedBy, AActor * DamageCauser)
{
	if (Damage <= 0.f)
	{
		return;
	}

	// Reset our time without taking damage to 0
	TimeWithoutTakingDamage = 0.f;

	// Check if we have shield
	if (Shield > 0.f)
	{
		// Take damage over shield
		Shield = FMath::Clamp(Shield - Damage, 0.f, DefaultShield);
	}
	// Else, take damage over health (note that this means if we have 10 shield and take 100 damage, we will not take damage over our health total)
	else
	{
		// Take damage over health
		Health = FMath::Clamp(Health - Damage, 0.f, DefaultHealth);
		OnHealthChanged.Broadcast(this, Health, Damage, DamageType, InstigatedBy, DamageCauser);
	}
}


// Handle replication
void USHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate health and shield
	DOREPLIFETIME(USHealthComponent, Health);
	DOREPLIFETIME(USHealthComponent, Shield);
}
