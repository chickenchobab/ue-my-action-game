// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/MyCharacter.h"
#include "Characters/MyCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Player/MyPlayerState.h"
#include "Combat/MyCombatComponent.h"
#include "Attributes/MyHealthComponent.h"
#include "Attributes/MyStatsComponent.h"
#include "GameMode/MyGameMode.h"
#include "Engine/AssetManager.h"
#include "Items/MyWeapon.h"
#include "MotionWarpingComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

AMyCharacter::AMyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

	CombatComponent = CreateDefaultSubobject<UMyCombatComponent>(TEXT("CombatComponent"));
	HealthComponent = CreateDefaultSubobject<UMyHealthComponent>(TEXT("HealthComponent"));
	StatsComponent = CreateDefaultSubobject<UMyStatsComponent>(TEXT("StatsComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->MaxWalkSpeed = 180.f;

	GetCharacterMovement()->BrakingFrictionFactor = 1.0f;
	GetCharacterMovement()->BrakingFriction = 6.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1400.0f;

	GetCharacterMovement()->GroundFriction = 8.0f;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 0.f);
	GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = false;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(65.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;

	// Configure character movement
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
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

void AMyCharacter::ReceiveHit(const FHitReceiveContext& HitContext)
{
	FVector DirectionToHitSource = -HitContext.ImpactDirection.GetSafeNormal2D();
	if (DirectionToHitSource.IsNearlyZero() && IsValid(HitContext.Instigator))
	{
		DirectionToHitSource = (HitContext.Instigator->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	}

	if (DirectionToHitSource.IsNearlyZero())
	{
		return;
	}

	const FVector CharacterForward = GetActorForwardVector().GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(CharacterForward, DirectionToHitSource);

	// modified: 좌우 전용 에셋이 없으므로 피격 원점이 캐릭터의 전방 반구에 있는지만 판정한다.
	UAnimMontage* HitReactionMontage = ForwardDot >= 0.0f ? HitReactionFrontMontage.Get() : HitReactionBackMontage.Get();

	// modified: 기존 네 방향 판정 로직을 보존한다.
	// const FVector CharacterRight = GetActorRightVector().GetSafeNormal2D();
	// const float RightDot = FVector::DotProduct(CharacterRight, DirectionToHitSource);
	//
	// UAnimMontage* HitReactionMontage = nullptr;
	// if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	// {
	// 	HitReactionMontage = ForwardDot >= 0.0f ? HitReactionFrontMontage.Get() : HitReactionBackMontage.Get();
	// }
	// else
	// {
	// 	HitReactionMontage = RightDot >= 0.0f ? HitReactionRightMontage.Get() : HitReactionLeftMontage.Get();
	// }

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && HitReactionMontage)
	{
		AnimInstance->Montage_Play(HitReactionMontage);
	}
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
