// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/MyCombatComponent.h"
#include "Skills/MySkillData.h"
#include "Characters/MyCharacter.h"
#include "Items/MyWeapon.h"
#include "EnhancedPlayerInput.h"

// Sets default values for this component's properties
UMyCombatComponent::UMyCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	bSkillEnabled = true;
}

void UMyCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CurrentMana += ManaRegenRate * DeltaTime;
	CurrentMana = FMath::Clamp(CurrentMana, 0.0f, MaxMana);
}

void UMyCombatComponent::InitAttributes(UCurveTable* CurveTable, float Level)
{
	if (const FRealCurve* Curve = CurveTable->FindCurve(FName("MaxMana"), TEXT("")))
	{
		MaxMana = Curve->Eval(Level);
	}

	if (const FRealCurve* Curve = CurveTable->FindCurve(FName("ManaRegenRate"), TEXT("")))
	{
		ManaRegenRate = Curve->Eval(Level);
	}

	CurrentMana = MaxMana;
}

void UMyCombatComponent::EquipWeapon(AMyWeapon* Weapon)
{
	AMyCharacter* OwnerCharacter = Cast<AMyCharacter>(GetOwner());

	Weapon->Equip(OwnerCharacter->GetMesh(), TEXT("WeaponSocket"));
}

void UMyCombatComponent::EquipDefaultWeapon()
{
	AMyCharacter* OwnerCharacter = Cast<AMyCharacter>(GetOwner());
	check(OwnerCharacter);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	CurrentWeapon = GetWorld()->SpawnActor<AMyWeapon>(DefaultWeaponClass.Get(), SpawnParams);
	if (CurrentWeapon)
	{
		CurrentWeapon->Equip(OwnerCharacter->GetMesh(), TEXT("WeaponSocket"));
	}
}

void UMyCombatComponent::OnWeaponSkillPressed(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType, const FVector2D& MovementVector)
{
	if (CurrentWeapon && bSkillEnabled)
	{
		if (UMySkillData* SkillData = CurrentWeapon->GetSkillData(SkillType))
		{
			// TODO: skill batching
			SkillData->TryExecuteSkill(ActionInstance, MovementVector);
		}
	}
}

void UMyCombatComponent::OnWeaponSkillReleased(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType, const FVector2D& MovementVector)
{
	if (CurrentWeapon && bSkillEnabled)
	{
		if (UMySkillData* SkillData = CurrentWeapon->GetSkillData(SkillType))
		{
			if (SkillData->IsSkillActive())
			{
				SkillData->HandleSkillReleased(ActionInstance, MovementVector);
			}
		}
	}
}