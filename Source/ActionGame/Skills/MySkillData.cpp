// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/MySkillData.h"
#include "Characters/MyCharacter.h"
#include "Combat/MyCombatComponent.h"

void UMySkillData::InitWithAvatar(AActor* NewAvatarActor)
{
	AvatarActor = NewAvatarActor;

	ExecutionGrantCount = !bComboOnly;
	bIsActive = false;
}

void UMySkillData::TryExecuteSkill(AActor* Instigator)
{
	check(Instigator);

	if (CanExecuteSkill(Instigator))
	{
		UE_LOG(LogTemp, Display, TEXT("Execute skill"));

		bIsActive = true;
		CommitCostsAndCooldown();
		ExecuteSkill(Instigator);

		FTimerHandle TimerHandle;
		Instigator->GetWorld()->GetTimerManager().SetTimer(
			TimerHandle,
			this,
			&ThisClass::OnSkillEnd,
			3.0f,
			false
		);
	}
}

void UMySkillData::HandleSkillReleased()
{
	UE_LOG(LogTemp, Display, TEXT("Skill released"));
}

void UMySkillData::EnableExecution(float Duration)
{
	++ExecutionGrantCount;

	if (Duration > 0.0f)
	{
		FTimerHandle TimerHandle;

		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle,
			[this]()
			{
				if (ExecutionGrantCount > 0)
				{
					--ExecutionGrantCount;
				}
			},
			Duration,
			false
		);
	}
}

bool UMySkillData::CanExecuteSkill(AActor* Instigator)
{
	if (InstancingPolicy != ESkillInstancingPolicy::PerExecution && bIsActive)
	{
		return false;
	}
	
	if (ExecutionGrantCount <= 0)
	{
		return false;
	}

	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(Instigator))
	{
		if (InstigatorCharacter->GetCombatComponent()->GetCurrentMana() < Cost)
		{
			return false;
		}

		// TODO: Check avatar tags
	}

	return true;
}

void UMySkillData::CommitCostsAndCooldown()
{
	// TODO
}

void UMySkillData::OnSkillEnd()
{
	UE_LOG(LogTemp, Display, TEXT("Skill ended"));
	bIsActive = false;
}

UMyCombatComponent* UMySkillData::GetCombatComponentFromInstigator(AActor* Instigator) const
{
	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(Instigator))
	{
		return InstigatorCharacter->GetCombatComponent();
	}

	return nullptr;
}

UAnimInstance* UMySkillData::GetAnimInstanceFromInstigator(AActor* Instigator) const
{
	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(Instigator))
	{
		return InstigatorCharacter->GetMesh()->GetAnimInstance();
	}
	
	return nullptr;
}
