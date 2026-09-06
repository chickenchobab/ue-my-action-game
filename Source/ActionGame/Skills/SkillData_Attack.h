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

	UPROPERTY(EditDefaultsOnly)
	float AttackComboWindow = 2.0f;

	virtual bool IsInstanceClassCompatible(TSubclassOf<UMySkillInstance> InstanceClass) const override;

protected:

	virtual TSubclassOf<UMySkillInstance> GetDefaultInstanceClass() const override;
};
