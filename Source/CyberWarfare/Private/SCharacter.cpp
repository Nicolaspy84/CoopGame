// Fill out your copyright notice in the Description page of Project Settings.

#include "SCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "SWeapon.h"
#include "Components/CapsuleComponent.h"
#include "CyberWarfare.h"
#include "../public/Components/SHealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
ASCharacter::ASCharacter()
{
	// Init names for sockets
	WeaponAttachSocketName = "WeaponSocketFPS";
	HeadAttachSocketName = "HeadSocket";

	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule and collision params
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);

	// Set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Attach camera to our TPS mesh
	// Create a CameraComponent	
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->bUsePawnControlRotation = true;
	CameraComp->SetupAttachment(GetMesh(), HeadAttachSocketName);

	// Attach FPSMesh to camera
	MeshCompFPS = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshCompFPS"));
	MeshCompFPS->SetupAttachment(CameraComp);

	// Enable the possibility to crouch
	GetMovementComponent()->GetNavAgentPropertiesRef().bCanCrouch = true;

	// Init health component
	HealthComp = CreateDefaultSubobject<USHealthComponent>(TEXT("HealthComp"));

	// Set our bag
	AmmoCount = 300;
}


// Called to bind functionality to input
void ASCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Call the overwritten function
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &ASCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ASCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ASCharacter::LookUpAtRate);

	// Bind crouch
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ASCharacter::BeginCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &ASCharacter::EndCrouch);

	// Bind jump
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);

	// Bind zoom (aim down)
	PlayerInputComponent->BindAction("Zoom", IE_Pressed, this, &ASCharacter::BeginZoom);
	PlayerInputComponent->BindAction("Zoom", IE_Released, this, &ASCharacter::EndZoom);

	// Bind primary fire
	PlayerInputComponent->BindAction("PrimaryFire", IE_Pressed, this, &ASCharacter::StartFire);
	PlayerInputComponent->BindAction("PrimaryFire", IE_Released, this, &ASCharacter::StopFire);

	// Bind reload
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ASCharacter::Reload);

	// Bind running
	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &ASCharacter::StartRunning);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &ASCharacter::StopRunning);
}


// Called when the game starts or when spawned
void ASCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Role == ROLE_Authority)
	{
		// Spawn a default weapon
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CurrentWeapon = GetWorld()->SpawnActor<ASWeapon>(StarterWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (CurrentWeapon)
		{
			CurrentWeapon->SetOwner(this);
			CurrentWeapon->AttachToComponent(MeshCompFPS, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketName);
		}
	}

	HealthComp->OnHealthChanged.AddDynamic(this, &ASCharacter::OnHealthChanged);
}


// Called every frame
void ASCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Role == ROLE_Authority)
	{
		HealthComp->TickShield(DeltaTime);
		SetLookRotation(GetControlRotation());
	}
}


// Called whenever our life total is affected
void ASCharacter::OnHealthChanged(USHealthComponent * HealthComponent, float Health, float HealthDelta, const UDamageType * DamageType, AController * InstigatedBy, AActor * DamageCauser)
{
	if (Health <= 0.f && !bDied)
	{
		// Die
		bDied = true;

		StopFire();

		GetMovementComponent()->StopMovementImmediately();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		DetachFromControllerPendingDestroy();

		CurrentWeapon->SetLifeSpan(10.f);
		SetLifeSpan(10.f);
	}
}


int32 ASCharacter::RequestAmmos(int32 Request)
{
	if (Request > 0)
	{
		if (AmmoCount >= Request)
		{
			AmmoCount -= Request;
			return Request;
		}
		int32 ReturnAmount = AmmoCount;
		AmmoCount = 0;
		return ReturnAmount;
	}
	return 0;
}


FVector ASCharacter::GetPawnViewLocation() const
{
	if (CameraComp)
	{
		return CameraComp->GetComponentLocation();
	}

	return Super::GetPawnViewLocation();
}


void ASCharacter::BeginCrouch()
{
	Crouch();
}


void ASCharacter::EndCrouch()
{
	UnCrouch();
}


void ASCharacter::BeginZoom()
{
	bWantsToZoom = true;
}


void ASCharacter::EndZoom()
{
	bWantsToZoom = false;
}


void ASCharacter::Reload()
{
	// Call server fire if we are on a client
	if (Role < ROLE_Authority)
	{
		ServerReload();
	}

	// If we have a weapon attached and its clip isn't full, we can start our reload animation
	if (CurrentWeapon && !CurrentWeapon->ClipIsFull())
	{
		if (bIsFiring)
		{
			StopFire();
		}
		bIsReloading = true;
	}
}


void ASCharacter::ServerReload_Implementation()
{
	Reload();
}


bool ASCharacter::ServerReload_Validate()
{
	return true;
}


void ASCharacter::Reloaded()
{
	if (CurrentWeapon && !CurrentWeapon->ClipIsFull())
	{
		CurrentWeapon->Reload();
	}
}


void ASCharacter::StartFire()
{
	if (CurrentWeapon && !bIsReloading)
	{
		bIsFiring = true;
		CurrentWeapon->StartFire();
	}
}


void ASCharacter::StopFire()
{
	if (CurrentWeapon && bIsFiring)
	{
		bIsFiring = false;
		CurrentWeapon->StopFire();
	}
}


void ASCharacter::StartRunning()
{
	GetCharacterMovement()->MaxWalkSpeed = 800;
}

void ASCharacter::StopRunning()
{
	GetCharacterMovement()->MaxWalkSpeed = 500;
}

void ASCharacter::SetLookRotation_Implementation(FRotator Rotation)
{
	if (!IsLocallyControlled())
	{
		LookRotation = Rotation;
	}
}


void ASCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// Add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}


void ASCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// Add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}


void ASCharacter::TurnAtRate(float Rate)
{
	// Calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}


void ASCharacter::LookUpAtRate(float Rate)
{
	// Calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}


// Called to replicate objects/variables
void ASCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASCharacter, CurrentWeapon);
	DOREPLIFETIME(ASCharacter, bDied);
	DOREPLIFETIME(ASCharacter, bWantsToZoom);
	DOREPLIFETIME(ASCharacter, bIsFiring);
	DOREPLIFETIME(ASCharacter, bIsReloading);
}
