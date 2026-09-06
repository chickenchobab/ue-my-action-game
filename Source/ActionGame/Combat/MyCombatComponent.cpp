// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/MyCombatComponent.h"
#include "Skills/MySkillData.h"
#include "Skills/MySkillInstance.h"
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
	if (!IsValid(Weapon) || CurrentWeapon == Weapon)
	{
		return;
	}

	UnequipCurrentWeapon();
	CurrentWeapon = Weapon;
	CurrentWeapon->Equip(Cast<ACharacter>(GetOwner())->GetMesh(), TEXT("WeaponSocket"));
	GrantSkillsFromWeapon(CurrentWeapon);
}

void UMyCombatComponent::EquipDefaultWeapon()
{
	AMyCharacter* OwnerCharacter = Cast<AMyCharacter>(GetOwner());
	check(OwnerCharacter);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMyWeapon* SpawnedWeapon = GetWorld()->SpawnActor<AMyWeapon>(DefaultWeaponClass.Get(), SpawnParams);
	if (SpawnedWeapon)
	{
		EquipWeapon(SpawnedWeapon);
	}
}

void UMyCombatComponent::UnequipCurrentWeapon()
{
	AMyWeapon* WeaponToUnequip = CurrentWeapon;
	CurrentWeapon = nullptr;

	if (!IsValid(WeaponToUnequip))
	{
		RevokeAllSkills();
		return;
	}

	WeaponToUnequip->UnEquip();
	RevokeSkillsFromWeapon(WeaponToUnequip);
}

void UMyCombatComponent::OnWeaponSkillPressed(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType, const FVector2D& MovementVector)
{
	if (CurrentWeapon && bSkillEnabled)
	{
		if (UMySkillData* SkillData = CurrentWeapon->GetSkillData(SkillType))
		{
			if (UMySkillInstance* SkillInstance = FindSkillInstance(SkillData))
			{
				// TODO: skill batching
				SkillInstance->TryExecuteSkill(ActionInstance, MovementVector);
			}
		}
	}
}

void UMyCombatComponent::OnWeaponSkillReleased(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType, const FVector2D& MovementVector)
{
	if (CurrentWeapon && bSkillEnabled)
	{
		if (UMySkillData* SkillData = CurrentWeapon->GetSkillData(SkillType))
		{
			// modified
			if (UMySkillInstance* SkillInstance = FindSkillInstance(SkillData))
			{
				if (SkillInstance->IsSkillActive())
				{
					SkillInstance->HandleSkillReleased(ActionInstance, MovementVector);
				}
			}
		}
	}
}

void UMyCombatComponent::GrantSkillsFromWeapon(AMyWeapon* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	AActor* AvatarActor = GetOwner();
	if (!IsValid(AvatarActor))
	{
		return;
	}

	for (const TPair<EWeaponSkillType, TObjectPtr<UMySkillData>>& KVP : Weapon->GetSkillSet())
	{
		UMySkillData* SkillData = KVP.Value;
		if (!SkillData || SkillInstances.Contains(SkillData))
		{
			continue;
		}

		// TODO: PerExecution 정책 구현
		if (SkillData->InstancingPolicy != ESkillInstancingPolicy::PerAvatar)
		{
			UE_LOG(LogTemp, Warning, TEXT("Unsupported skill instancing policy: %s"), *SkillData->GetName());
			continue;
		}

		const TSubclassOf<UMySkillInstance> InstanceClass = SkillData->GetInstanceClass();
		if (!InstanceClass || InstanceClass->HasAnyClassFlags(CLASS_Abstract) || !SkillData->IsInstanceClassCompatible(InstanceClass))
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid skill instance class: %s"), *SkillData->GetName());
			continue;
		}

		UMySkillInstance* SkillInstance = NewObject<UMySkillInstance>(this, InstanceClass);
		if (!SkillInstance || !SkillInstance->Initialize(SkillData, AvatarActor, Weapon))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to initialize skill instance: %s"), *SkillData->GetName());
			continue;
		}

		SkillInstances.Add(SkillData, SkillInstance);
	}
}

void UMyCombatComponent::RevokeSkillsFromWeapon(AMyWeapon* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	for (const TPair<EWeaponSkillType, TObjectPtr<UMySkillData>>& KVP : Weapon->GetSkillSet())
	{
		UMySkillData* SkillData = KVP.Value;
		if (!SkillData)
		{
			continue;
		}

		if (UMySkillInstance* SkillInstance = SkillInstances.FindRef(SkillData))
		{
			SkillInstance->Deinitialize();
			SkillInstances.Remove(SkillData);
		}
	}
}

void UMyCombatComponent::RevokeAllSkills()
{
	for (TPair<TObjectPtr<UMySkillData>, TObjectPtr<UMySkillInstance>>& KVP : SkillInstances)
	{
		if (UMySkillInstance* SkillInstance = KVP.Value)
		{
			SkillInstance->Deinitialize();
		}
	}

	SkillInstances.Empty();
}

UMySkillInstance* UMyCombatComponent::FindSkillInstance(UMySkillData* SkillData) const
{
	if (!SkillData)
	{
		return nullptr;
	}

	return SkillInstances.FindRef(SkillData);
}

// modified: 캐릭터 종료 시 무기 델리게이트와 스킬 타이머가 남지 않게 한다.
void UMyCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnequipCurrentWeapon();
	RevokeAllSkills();

	Super::EndPlay(EndPlayReason);
}
