// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SWeapon.h"
#include "SProjectileWeapon.generated.h"

/**
 * Grenade launcher inherited from classic weapon
 */
UCLASS()
class CYBERWARFARE_API ASProjectileWeapon : public ASWeapon
{
	GENERATED_BODY()

protected:

	virtual void Fire() override;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile weapon")
	TSubclassOf<AActor> ProjectileClass;

	
};
