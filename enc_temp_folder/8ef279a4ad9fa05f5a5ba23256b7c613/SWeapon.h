// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SWeapon.generated.h"

class USkeletalMeshComponent;
class UDamageType;
class UParticleSystem;
class USoundBase;

/** Contains info of a single hit scan weapon line trace */
USTRUCT()
struct FHitScanTrace
{
	GENERATED_BODY()

public:

	UPROPERTY()
		TEnumAsByte<EPhysicalSurface> SurfaceType;

	UPROPERTY()
		FVector_NetQuantize TraceTo;

};


UCLASS()
class CYBERWARFARE_API ASWeapon : public AActor
{
	GENERATED_BODY()
	
public:

	/** Sets default values for this actor's properties */
	ASWeapon();

	void StartFire();

	void StopFire();

	bool ClipIsFull();
	bool ClipIsEmpty();

	/** Reload function */
	void Reload();

protected:

	/** Begin play */
	virtual void BeginPlay() override;


	/** Locally play effects */
	void PlayFireEffects(FVector TracerEndPoint);
	void PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint);
	
	
	/** Fire functions */
	virtual void Fire();
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerFire();
	UFUNCTION()
		void OnRep_HitScanTrace();


	/** Reload functions */
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerReload();


	/** Weapon mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
		USkeletalMeshComponent* MeshComp;


	/** Weapon sockets */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
		FName MuzzleSocketName;
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
		FName TracerTargetName;


	/** Weapon special effects */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponEffects")
		UParticleSystem* MuzzleEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponEffects")
		UParticleSystem* DefaultImpactEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponEffects")
		UParticleSystem* FleshImpactEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponEffects")
		UParticleSystem* TracerEffect;
	UPROPERTY(EditDefaultsOnly, Category = "WeaponEffects")
		TSubclassOf<UCameraShake> FireCamShake;


	/** Sound effects */
	UPROPERTY(EditDefaultsOnly, Category = "WeaponSounds")
		USoundBase* SoundFire;
	UPROPERTY(EditDefaultsOnly, Category = "WeaponSounds")
		USoundBase* SoundBodyHit;
	UPROPERTY(EditDefaultsOnly, Category = "WeaponSounds")
		USoundBase* SoundSurfaceHit;


	/** Weapon stats */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
		TSubclassOf<UDamageType> DamageType;
	UPROPERTY(EditDefaultsOnly, Category = "WeaponStats")
		float BaseDamage;
	UPROPERTY(EditDefaultsOnly, Category = "WeaponStats")
		float RateOfFire;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "WeaponStats")
		int32 ClipCurrentSize;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponStats")
		int32 ClipMaxSize;


	/** Utilities for replication */
	UPROPERTY(ReplicatedUsing = OnRep_HitScanTrace)
		FHitScanTrace HitScanTrace;

	/** Times and timers for weapon fire */
	float TimeBetweenShots;
	float LastFireTime;
	FTimerHandle TimerHandle_TimeBetweenShots;
};
