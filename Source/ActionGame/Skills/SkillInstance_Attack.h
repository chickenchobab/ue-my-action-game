// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Skills/MySkillInstance.h"
#include "SkillInstance_Attack.generated.h"

struct FInputActionInstance;

/**
 *
 */
UCLASS()
class ACTIONGAME_API USkillInstance_Attack : public UMySkillInstance
{
	GENERATED_BODY()

public:
	virtual bool Initialize(UMySkillData* InSkillData, AActor* InAvatarActor, AActor* InSourceItem) override;
	virtual void Deinitialize() override;
	virtual bool CanExecuteSkill() override;
	virtual bool ExecuteSkill(const FInputActionInstance& Instance, const FVector2D& MovementVector) override;
	virtual void OnSkillEnd(bool bCanceled) override;

	virtual void OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted) override;

	virtual void OnAttackEnd();
	virtual void OnAttackHit(AActor* Instigator, TArray<AActor*>& TargetActors);

protected:
	void OnComboWindowExpired();

	FTimerHandle ComboWindowTimerHandle;
	bool bInAttackComboWindow = false;
};
