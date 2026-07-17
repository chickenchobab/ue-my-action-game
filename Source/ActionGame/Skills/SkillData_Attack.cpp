// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/SkillData_Attack.h"
#include "Combat/MyCombatComponent.h"
#include "Items/MyWeapon.h"

void USkillData_Attack::InitWithItem(AActor* OwningItem)
{
	if (AMyWeapon* Weapon = Cast<AMyWeapon>(OwningItem))
	{
		Weapon->OnAttackHit.AddUObject(this, &ThisClass::OnAttackHit);
	}
}

void USkillData_Attack::InitWithAvatar(AActor* NewAvatar)
{
	Super::InitWithAvatar(NewAvatar);

	bInAttackComboWindow = false;
	ComboWindowTimerHandle.Invalidate();
}

bool USkillData_Attack::CanExecuteSkill()
{
	if (bInAttackComboWindow)
	{
		return true;
	}

	return Super::CanExecuteSkill();
}

void USkillData_Attack::ExecuteSkill()
{	
	bInAttackComboWindow = false;

	PlaySkillMontage();
	if (CurrentMontageIndex >= SkillMontages.Num())
	{
		return;
	}

	UMyCombatComponent* CombatComponent = GetCombatComponentFromAvatarActor();
	if (AMyWeapon* Weapon = CombatComponent->GetCurrentWeapon())
	{
		Weapon->InitWeaponForAttack(this, SkillMontages[CurrentMontageIndex]);
	}
}

void USkillData_Attack::OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnSkillEnd();
}

void USkillData_Attack::OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted)
{
}

void USkillData_Attack::OnAttackEnd()
{
	if (!AvatarActor.IsValid())
	{
		return;
	}

	ComboWindowTimerHandle.Invalidate();

	if (CurrentMontageIndex == SkillMontages.Num() - 1)
	{
		CurrentMontageIndex = 0;
	}
	else
	{
		CurrentMontageIndex = CurrentMontageIndex + 1;
		bInAttackComboWindow = true;

		AvatarActor.Get()->GetWorld()->GetTimerManager().SetTimer(
			ComboWindowTimerHandle,
			[this]() {
				CurrentMontageIndex = 0;
				bInAttackComboWindow = false;
			},
			AttackComboWindow,
			false
		);
	}
}

void USkillData_Attack::OnAttackHit(AActor* Instigator, TArray<AActor*>& TargetActors)
{
	UE_LOG(LogTemp, Display, TEXT("Attack Hit"));
	for (auto TargetActor : TargetActors)
	{
		if (TargetActor)
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *TargetActor->GetName());
		}
	}
}
