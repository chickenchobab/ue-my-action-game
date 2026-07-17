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
	AvatarActor.Get()->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);
}

bool USkillData_Attack::CanExecuteSkill()
{
	// The montage end callback(causing deactivation) may be invoked before the attack end anim notify,
	// so check activity here to prevent from executing before the montage update
	if (bInAttackComboWindow)
	{
		return IsSkillActive();
	}

	return Super::CanExecuteSkill();
}

void USkillData_Attack::ExecuteSkill()
{	
	bInAttackComboWindow = false;
	AvatarActor.Get()->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);

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

void USkillData_Attack::OnSkillEnd(bool bCanceled)
{
	if (bCanceled)
	{
		bInAttackComboWindow = false;
		AvatarActor.Get()->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);
	}

	Super::OnSkillEnd(bCanceled);
}

void USkillData_Attack::OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnSkillEnd(false);
}

void USkillData_Attack::OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted)
{
}

void USkillData_Attack::OnAttackEnd()
{
	UE_LOG(LogTemp, Display, TEXT("Attack end"));
	if (!AvatarActor.IsValid())
	{
		return;
	}

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
