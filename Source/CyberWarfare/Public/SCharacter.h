// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class ASWeapon;
class USHealthComponent;
class USkeletalMeshComponent;

UCLASS()
class CYBERWARFARE_API ASCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASCharacter();

	/** Called every frame */
	virtual void Tick(float DeltaTime) override;

	/** Called to bind functionality to input */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Returns the view location of the pawn (the "center" of the eyes) */
	virtual FVector GetPawnViewLocation() const override;

	/** Request a certain amount of ammo from player, and update our ammo count (will return Request if we have enough ammo, or less if we don't have enough ammo) */
	int32 RequestAmmos(int32 Request);

	/** Handles reloading */
	void Reload();

	/** This is called once the reloading animation has ended */
	UFUNCTION(BlueprintCallable)
		void Reloaded();

	/** Handles starting fire (useful for auto weapons) */
	void StartFire();

	/** Handles ending fire (useful for auto weapons) */
	void StopFire();

protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
    
	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles strafing movement, left and right */
	void MoveRight(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handles crouching */
	void BeginCrouch();

	/** Handles uncrouching */
	void EndCrouch();

	/** Handles aiming */
	void BeginZoom();

	/** Handles unaiming */
	void EndZoom();

	/** Will setup our LookAtRotation var if we are not the owner of the pawn */
	UFUNCTION(NetMulticast, Reliable)
		void SetLookRotation(FRotator Rotation);

	/** Called on health changed */
	UFUNCTION()
		void OnHealthChanged(USHealthComponent* HealthComponent, float Health, float HealthDelta, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	/** Server reload for server */
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerReload();

	/** Rates for looking around */
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
		float BaseTurnRate;
	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
		float BaseLookUpRate;
    

	/** Camera component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
		UCameraComponent* CameraComp;

	/** Health component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
		USHealthComponent* HealthComp;


	/** Variables for animations */
	/** Is set to true when the player asks to zoom in */
	UPROPERTY(Replicated, BlueprintReadOnly)
		bool bWantsToZoom;
	/** Is set to true when the player is firing */
	UPROPERTY(Replicated, BlueprintReadOnly)
		bool bIsFiring;
	/** Is set to true when we are reloading */
	UPROPERTY(Replicated, BlueprintReadWrite)
		bool bIsReloading;
	/** Replicated controller rotation for other clients to know where this character is looking */
	UPROPERTY(BlueprintReadWrite)
		FRotator LookRotation;
	/** Called on character's death */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player")
		bool bDied;


	/** Items that can be hold by the player */
	/** Holds the current weapon of the player */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
		ASWeapon* CurrentWeapon;
	/** Holds the weapon class of the player */
	UPROPERTY(EditDefaultsOnly, Category = "Player")
		TSubclassOf<ASWeapon> StarterWeaponClass;
	/** Holds total ammo count for the player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PlayerBag")
		int32 AmmoCount;


	/** Socket names for the player */
	/** Holds the socket name for where to attach the weapon */
	UPROPERTY(VisibleDefaultsOnly, Category = "Weapon")
		FName WeaponAttachSocketName;
	/** Holds the socket name for where to attach the camera */
	UPROPERTY(VisibleDefaultsOnly, Category = "Player")
		FName HeadAttachSocketName;
};
