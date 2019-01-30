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
#include "Components/StaticMeshComponent.h"

// Sets default values
ASCharacter::ASCharacter()
{
	// Init names for sockets
	WeaponAttachSocketNameTPS = "WeaponSocket";
	WeaponAttachSocketNameFPS = "WeaponSocketFPS";
	WeaponBackSocketNameTPS = "WeaponBackSocketTPS";
	WeaponBackSocketNameFPS = "WeaponBackSocketFPS";
	HeadAttachSocketName = "HeadSocket";

	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule and collision params
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);

	// Set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	FVector CharacterVelocity = GetVelocity();

	CharacterSpeed = CharacterVelocity.Size();


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

	InventorySize = 4;

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
	PlayerInputComponent->BindAction("StopMovingForward", IE_Released, this, &ASCharacter::StopRunning);

	// Bind Equipment Switch
	PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &ASCharacter::NextWeapon);
	PlayerInputComponent->BindAction("PreviousWeapon", IE_Pressed, this, &ASCharacter::PreviousWeapon);
	
}


// Called when the game starts or when spawned
void ASCharacter::BeginPlay()
{
	Super::BeginPlay();

	SpawnInventory(); 

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	NewWeapon = Inventory[0];
	CurrentInventoryIndex = 0;
	
	EquipNewWeapon();

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


void ASCharacter::SpawnInventory()
{
	
	if (Role < ROLE_Authority)

	{
		Server_SpawnInventory();
	}


	else
	{
		// Spawn a default weapon
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Inventory.Add(GetWorld()->SpawnActor<ASWeapon>(StarterWeaponClass2Test, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams));

		for (int i=1; i<=InventorySize-1; i++)
		{
			Inventory.Add(GetWorld()->SpawnActor<ASWeapon>(StarterWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams));
		}
	}
}


void ASCharacter::Server_SpawnInventory_Implementation()
{
	SpawnInventory();
}

bool ASCharacter::Server_SpawnInventory_Validate()
{
	return true;
} 

void ASCharacter::NextWeapon()
{
	CurrentInventoryIndex = (CurrentInventoryIndex + 1) % (InventorySize);
	NewWeapon = Inventory[CurrentInventoryIndex];
	EquipNewWeapon();
}

void ASCharacter::PreviousWeapon()
{
	CurrentInventoryIndex = (CurrentInventoryIndex + InventorySize - 1) % (InventorySize);
	NewWeapon = Inventory[CurrentInventoryIndex];
	EquipNewWeapon();
}

void ASCharacter::EquipNewWeapon()
{
	//Detach Current Weapon
	if (CurrentWeapon)
	{
		CurrentWeapon->SetOwner(this);
		if (IsLocallyControlled())
		{
			CurrentWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}

		else
		{
			CurrentWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}

		CurrentWeapon->SetActorHiddenInGame(true);
		CurrentWeapon->SetActorEnableCollision(false);
		CurrentWeapon->SetActorTickEnabled(true);

	}

	// Attach New Weapon
	if (NewWeapon)
	{
		NewWeapon->SetOwner(this);
		if (IsLocallyControlled())
		{
			NewWeapon->AttachToComponent(MeshCompFPS, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketNameFPS);
		}

		else
		{
			NewWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketNameTPS);
		}

		CurrentWeapon = NewWeapon;
		CurrentWeapon->SetActorHiddenInGame(false);
		CurrentWeapon->SetActorEnableCollision(true);
		CurrentWeapon->SetActorTickEnabled(false);

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
		ReloadingWeapon = CurrentWeapon;
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


void ASCharacter::Reload_AnimationFinished(ASWeapon* ReloadingWeapon)
{
	if (CurrentWeapon && !CurrentWeapon->ClipIsFull() && ReloadingWeapon==CurrentWeapon)
	{
		CurrentWeapon->Reload();
	}
	else
	{
		bIsReloading = false;
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
	bIsRunning = true;

	if (GetCharacterDirection()<45.0f && GetCharacterDirection()>-45.0f)
	{
		GetCharacterMovement()->MaxWalkSpeed = 800;
	}
}

void ASCharacter::StopRunning()
{
	bIsRunning = false;
	GetCharacterMovement()->MaxWalkSpeed = 500;
}




float ASCharacter::GetCharacterSpeed()
{
	return(CharacterSpeed);
}

void ASCharacter::Pickup(ASWeapon* Actor)
{
	
}

void ASCharacter::PrintInventory()
{
	FString sInventory = "";


}



ASWeapon* ASCharacter::GetCurrentWeapon()
{
	return (CurrentWeapon);
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