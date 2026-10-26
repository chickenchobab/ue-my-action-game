// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Skills/MySkillInstance.h"
#include "SkillInstance_Attack.generated.h"

struct FInputActionInstance;
class ACharacter;

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
	virtual bool ExecuteSkill(const FInputActionInstance& Instance, const FVector2D& MovementVector, AActor* LockedTarget) override;
	virtual void OnSkillEnd(bool bCanceled) override;

	virtual void OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 MontageInstanceID) override;
	virtual void OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted) override;

	void RegisterPendingAttackEnd(int32 MontageInstanceID, bool bAdvancesAttackCombo);
	virtual void OnAttackEnd(int32 MontageInstanceID, bool bAdvancesAttackCombo);
	virtual void OnAttackHit(AActor* Instigator, AActor* TargetActor, const FVector& ImpactPoint);

protected:
	bool CalculateCapsuleAttackDistances(const ACharacter* AvatarCharacter, const AActor* TargetActor,
		float& OutCurrentDistance, float& OutCapsuleContactDistance) const;
	bool SetupAttackWarpTarget(ACharacter* AvatarCharacter, AActor* TargetActor, float WarpStopDistance);
	void ClearAttackWarpTarget();
	void ClearPendingAttackEndState();
	void ResetComboState();
	void OnComboWindowExpired();

	FTimerHandle ComboWindowTimerHandle;
	TSet<int32> PendingAttackEndMontageInstanceIDs;
	int32 PendingComboPolicyMontageInstanceID = INDEX_NONE;
	bool bPendingAttackAdvancesCombo = false;
	bool bInAttackComboWindow = false;
};
