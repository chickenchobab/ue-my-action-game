// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/MyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/MyPlayerState.h"
#include "Combat/MyCombatComponent.h"
#include "Attributes/MyHealthComponent.h"
#include "Attributes/MyStatsComponent.h"
#include "GameMode/MyGameMode.h"
#include "Engine/AssetManager.h"
#include "Items/MyWeapon.h"

AMyCharacter::AMyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatComponent = CreateDefaultSubobject<UMyCombatComponent>(TEXT("CombatComponent"));
	HealthComponent = CreateDefaultSubobject<UMyHealthComponent>(TEXT("HealthComponent"));
	StatsComponent = CreateDefaultSubobject<UMyStatsComponent>(TEXT("StatsComponent"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Configure character movement
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AMyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Initialize character stats and equip the weapon.

	if (UCurveTable* CurveTable = AttributeCurveTable.LoadSynchronous())
	{
		LoadAttributes(HealthComponent.Get(), CurveTable, 0.0f);
		LoadAttributes(CombatComponent.Get(), CurveTable, 0.0f);
		LoadAttributes(StatsComponent.Get(), CurveTable, 0.0f);
	}

	if (AMyGameMode* GM = Cast<AMyGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if (GM->ShouldAutoEquipWeapon())
		{
			const TSoftClassPtr<AMyWeapon>& WeaponClass = GetCombatComponent()->GetDefaultWeaponClass();
			if (!WeaponClass.IsNull())
			{
				UAssetManager::GetStreamableManager().RequestAsyncLoad(
					WeaponClass.ToSoftObjectPath(),
					FStreamableDelegate::CreateUObject(GetCombatComponent(), &UMyCombatComponent::EquipDefaultWeapon)
				);
			}
		}
	}
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

