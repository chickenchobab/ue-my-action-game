// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Skills/MySkillData.h"
#include "SkillData_Attack.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONGAME_API USkillData_Attack : public UMySkillData
{
	GENERATED_BODY()
	
public:
	virtual void InitWithItem(AActor* OwningItem) override;
	virtual void InitWithAvatar(AActor* NewAvatar) override;
	virtual bool CanExecuteSkill() override;
	virtual void ExecuteSkill() override;
	virtual void OnSkillEnd(bool bCanceled) override;

	virtual void OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted) override;

	virtual void OnAttackEnd();
	virtual void OnAttackHit(AActor* Instigator, TArray<AActor*>& TargetActors);

protected:

	UPROPERTY(EditDefaultsOnly)
	float AttackComboWindow = 2.0f;

	FTimerHandle ComboWindowTimerHandle;
	bool bInAttackComboWindow = false;
};
