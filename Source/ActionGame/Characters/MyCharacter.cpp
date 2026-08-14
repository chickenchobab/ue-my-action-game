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
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

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

void AMyCharacter::AddStatusTag(const FGameplayTag& Tag, int32 Count)
{
	if (StatsComponent)
	{
		StatsComponent->AddStatusTag(Tag, Count);
	}
}

void AMyCharacter::RemoveStatusTag(const FGameplayTag& Tag, int32 Count)
{
	if (StatsComponent)
	{
		StatsComponent->RemoveStatusTag(Tag, Count);
	}
}

void AMyCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (StatsComponent)
	{
		StatsComponent->GetOwnedGameplayTags(TagContainer);
	}
}

bool AMyCharacter::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (StatsComponent)
	{
		return StatsComponent->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool AMyCharacter::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (StatsComponent)
	{
		return StatsComponent->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool AMyCharacter::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (StatsComponent)
	{
		return StatsComponent->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();

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
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}