// Fill out your copyright notice in the Description page of Project Settings.

#include "SWeapon.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "CyberWarfare.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "SCharacter.h"


static int32 DebugWeaponDrawing = 0;
FAutoConsoleVariableRef CVARDebugWeaponDrawing(
	TEXT("COOP.DebugWeapons"),
	DebugWeaponDrawing, 
	TEXT("Drawn debug lines for weapons"), 
	ECVF_Cheat);


// Constructor
ASWeapon::ASWeapon()
{
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MuzzleSocketName = "MuzzleSocket";
	TracerTargetName = "Target";

	BaseDamage = 20.f;
	RateOfFire = 600;
	ClipMaxSize = 30;

	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	SetReplicates(true);
}


// Begin play
void ASWeapon::BeginPlay()
{
	Super::BeginPlay();

	ClipCurrentSize = ClipMaxSize;
	TimeBetweenShots = 60 / RateOfFire;
}


// Fire function
void ASWeapon::Fire()
{
	if (ClipCurrentSize <= 0)
	{
		ASCharacter* MyOwner = Cast<ASCharacter>(GetOwner());
		if (MyOwner)
		{
			MyOwner->Reload();
		}
		return;
	}

	ClipCurrentSize--;


	// Call server fire if we are on a client
	if (Role < ROLE_Authority)
	{
		ServerFire();
	}
	
	// Get owner of weapon
	AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		FVector EyeLocation;
		FRotator EyeRotation;
		MyOwner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		FVector ShotDirection = EyeRotation.Vector();
		FVector TraceEnd = EyeLocation + (EyeRotation.Vector() * 10000);
		FVector HeightOffset(0.f, 0.f, 20.f);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(MyOwner);
		QueryParams.AddIgnoredActor(this);
		QueryParams.bTraceComplex = true;
		QueryParams.bReturnPhysicalMaterial = true;

		FVector TracerEndPoint = TraceEnd;

		EPhysicalSurface SurfaceType = SurfaceType_Default;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, MeshComp->GetSocketLocation(MuzzleSocketName) + HeightOffset, TraceEnd, COLLISION_WEAPON, QueryParams))
		{
			// Blocking hit, process damage here

			AActor* HitActor = Hit.GetActor();

			SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

			float ActualDamage = BaseDamage;
			if (SurfaceType == SURFACE_FLESHVULNERABLE)
			{
				ActualDamage *= 4.f;
			}

			UGameplayStatics::ApplyPointDamage(HitActor, ActualDamage, ShotDirection, Hit, MyOwner->GetInstigatorController(), this, DamageType);

			PlayImpactEffects(SurfaceType, Hit.ImpactPoint);

			TracerEndPoint = Hit.ImpactPoint;
		}

		if (DebugWeaponDrawing > 0)
		{
			DrawDebugLine(GetWorld(), EyeLocation, TraceEnd, FColor::White, false, 1.0f, 0, 1.0f);
		}
		
		PlayFireEffects(TracerEndPoint);

		if (Role == ROLE_Authority)
		{
			HitScanTrace.TraceTo = TracerEndPoint;
			HitScanTrace.SurfaceType = SurfaceType;
		}

		LastFireTime = GetWorld()->TimeSeconds;
	}
}


void ASWeapon::OnRep_HitScanTrace()
{
	// Play cosmetic effects
	PlayFireEffects(HitScanTrace.TraceTo);
	PlayImpactEffects(HitScanTrace.SurfaceType, HitScanTrace.TraceTo);
}


void ASWeapon::ServerFire_Implementation()
{
	Fire();
}


bool ASWeapon::ServerFire_Validate()
{
	return true;
}


void ASWeapon::StartFire()
{
	float FirstDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.f);

	GetWorldTimerManager().SetTimer(TimerHandle_TimeBetweenShots, this, &ASWeapon::Fire, TimeBetweenShots, true, FirstDelay);
}


void ASWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_TimeBetweenShots);
}


bool ASWeapon::ClipIsFull()
{
	if (ClipCurrentSize < ClipMaxSize)
	{
		return false;
	}
	return true;
}


bool ASWeapon::ClipIsEmpty()
{
	if (ClipCurrentSize <= 0)
	{
		return true;
	}
	return false;
}


void ASWeapon::Reload()
{
	// Call server reload if we are on a client
	if (Role < ROLE_Authority)
	{
		ServerReload();
	}

	// Get owner of weapon
	ASCharacter* MyOwner = Cast<ASCharacter>(GetOwner());

	if (MyOwner)
	{
		// Check how many ammos we can get to fill our clip
		int32 NewAmmos = MyOwner->RequestAmmos(ClipMaxSize - ClipCurrentSize);

		// Update our current clip
		ClipCurrentSize += NewAmmos;
	}
}


void ASWeapon::ServerReload_Implementation()
{
	Reload();
}


bool ASWeapon::ServerReload_Validate()
{
	return true;
}


// Play effects at muzzle location on fire (locally)
void ASWeapon::PlayFireEffects(FVector TracerEndPoint)
{
	if (MuzzleEffect)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
	}

	if (SoundFire)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoundFire, MeshComp->GetSocketLocation(MuzzleSocketName), 0.3);
	}

	if (TracerEffect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);

		UParticleSystemComponent* TracerComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TracerEffect, MuzzleLocation);
		if (TracerComp)
		{
			TracerComp->SetVectorParameter(TracerTargetName, TracerEndPoint);
		}
	}

	APawn* MyOwner = Cast<APawn>(GetOwner());
	if (MyOwner)
	{
		APlayerController* PC = Cast<APlayerController>(MyOwner->GetController());
		if (PC)
		{
			PC->ClientPlayCameraShake(FireCamShake);
		}
	}
}


// Play effects on impact (locally)
void ASWeapon::PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint)
{
	UParticleSystem* SelectedEffect = nullptr;
	switch (SurfaceType)
	{
	case SURFACE_FLESHDEFAULT:
		SelectedEffect = FleshImpactEffect;
		if (SoundBodyHit)
		{
			UGameplayStatics::PlaySoundAtLocation(this, SoundBodyHit, ImpactPoint, 0.5);
		}
		break;
	case SURFACE_FLESHVULNERABLE:
		SelectedEffect = FleshImpactEffect;
		if (SoundBodyHit)
		{
			UGameplayStatics::PlaySoundAtLocation(this, SoundBodyHit, ImpactPoint, 0.5);
		}
		break;
	default:
		SelectedEffect = DefaultImpactEffect;
		if (SoundSurfaceHit)
		{
			UGameplayStatics::PlaySoundAtLocation(this, SoundSurfaceHit, ImpactPoint, 0.5);
		}
		break;
	}

	if (SelectedEffect)
	{
		FVector ShotDirection = ImpactPoint - MeshComp->GetSocketLocation(MuzzleSocketName);
		ShotDirection.Normalize();

		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEffect, ImpactPoint, ShotDirection.Rotation());
	}
}


// Replicate props
void ASWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ASWeapon, ClipCurrentSize);
	DOREPLIFETIME_CONDITION(ASWeapon, HitScanTrace, COND_SkipOwner);
}

