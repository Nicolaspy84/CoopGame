// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHealthComponent.generated.h"

/** On health changed event */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnHealthChangedSignature, USHealthComponent*, HealthComp, float, Health, float, HealthDelta, const class UDamageType*, DamageType, class AController*, InstigatedBy, AActor*, DamageCauser);


UCLASS(ClassGroup=(COOP), meta=(BlueprintSpawnableComponent))
class CYBERWARFARE_API USHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Sets default values for this component's properties */
	USHealthComponent();

	/** Called every tick by of our owner to update our shield (NOTE: this should only be called on the server) */
	void TickShield(float DeltaTime);

	/** Health change signature */
	UPROPERTY(BlueprintAssignable, Category = "Events")
		FOnHealthChangedSignature OnHealthChanged;

protected:

	/** Called when the game starts */
	virtual void BeginPlay() override;

	/** Time elapse without taking any damage */
	float TimeWithoutTakingDamage;

	/** Default health value (max health) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HealthComponent")
		float DefaultHealth;

	/** Actual health value (if it reaches 0, we die) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "HealthComponent")
		float Health;

	/** Default shield value (max shield) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HealthComponent")
		float DefaultShield;

	/** Sets the time before the shield starts regenerating */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HealthComponent")
		float TimeBeforeShieldRegen;

	/** Regenerating rate for the shield (amount of shield we get back every second after not taking damage for some time) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HealthComponent")
		float ShieldRegenRate;

	/** Actual shield value (if it reaches 0, we take damage over health) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "HealthComponent")
		float Shield;

	/** Handles damage taken */
	UFUNCTION()
		void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);
};
