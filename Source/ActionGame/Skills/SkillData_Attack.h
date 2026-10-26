// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Skills/MySkillData.h"
#include "SkillData_Attack.generated.h"

class UAnimMontage;

/**
 *
 */
UCLASS()
class ACTIONGAME_API USkillData_Attack : public UMySkillData
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly)
	float AttackComboWindow = 2.0f;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UAnimMontage> LungeAttackMontage;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.0"))
	float AttackRangeOffset = 40.0f; // 무기 길이와 대응한다

	UPROPERTY(EditDefaultsOnly)
	FName AttackWarpTargetName = TEXT("AttackTarget");

	virtual bool IsInstanceClassCompatible(TSubclassOf<UMySkillInstance> InstanceClass) const override;

protected:

	virtual TSubclassOf<UMySkillInstance> GetDefaultInstanceClass() const override;
};
