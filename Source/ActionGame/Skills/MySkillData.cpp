// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/MySkillData.h"
#include "Skills/MySkillInstance.h"

TSubclassOf<UMySkillInstance> UMySkillData::GetInstanceClass() const
{
	return InstanceClassOverride ? InstanceClassOverride : GetDefaultInstanceClass();
}

bool UMySkillData::IsInstanceClassCompatible(TSubclassOf<UMySkillInstance> InstanceClass) const
{
	return InstanceClass && InstanceClass->IsChildOf(UMySkillInstance::StaticClass());
}

TSubclassOf<UMySkillInstance> UMySkillData::GetDefaultInstanceClass() const
{
	return UMySkillInstance::StaticClass();
}
