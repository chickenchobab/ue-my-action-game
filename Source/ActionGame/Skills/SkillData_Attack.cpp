// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/SkillData_Attack.h"
#include "Skills/SkillInstance_Attack.h"

TSubclassOf<UMySkillInstance> USkillData_Attack::GetDefaultInstanceClass() const
{
	return USkillInstance_Attack::StaticClass();
}

bool USkillData_Attack::IsInstanceClassCompatible(TSubclassOf<UMySkillInstance> InstanceClass) const
{
	return InstanceClass && InstanceClass->IsChildOf(USkillInstance_Attack::StaticClass());
}
