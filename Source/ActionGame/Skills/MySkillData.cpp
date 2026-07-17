// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/MySkillData.h"
#include "Characters/MyCharacter.h"
#include "Combat/MyCombatComponent.h"

void UMySkillData::InitWithAvatar(AActor* NewAvatarActor)
{
	AvatarActor = NewAvatarActor;

	ExecutionGrantCount = !bComboOnly;
	ActiveCount = 0;
	CurrentMontageIndex = 0;
}

void UMySkillData::TryExecuteSkill()
{
	check(AvatarActor.IsValid());

	if (CanExecuteSkill())
	{
		UE_LOG(LogTemp, Display, TEXT("Execute skill"));
		++ActiveCount;
		CommitCostsAndCooldown();
		ExecuteSkill();
	}
}

void UMySkillData::CancelSkill()
{
	OnSkillEnd(true);
}

void UMySkillData::HandleSkillReleased()
{
}

void UMySkillData::EnableExecution(float Duration)
{
	if (!AvatarActor.IsValid())
	{
		return;
	}

	++ExecutionGrantCount;

	if (Duration > 0.0f)
	{
		FTimerHandle TimerHandle;

		AvatarActor.Get()->GetWorld()->GetTimerManager().SetTimer(
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

bool UMySkillData::CanExecuteSkill()
{
	if (InstancingPolicy != ESkillInstancingPolicy::PerExecution && IsSkillActive())
	{
		return false;
	}
	
	if (ExecutionGrantCount <= 0)
	{
		return false;
	}

	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(AvatarActor))
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

void UMySkillData::OnSkillEnd(bool bCanceled)
{
	UE_LOG(LogTemp, Display, TEXT("Skill ended"));
	--ActiveCount;
}

void UMySkillData::PlaySkillMontage()
{
	if (CurrentMontageIndex >= SkillMontages.Num())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstanceFromAvatarActor();
	AnimInstance->Montage_Play(SkillMontages[CurrentMontageIndex]);

	FOnMontageEnded EndDelegate = FOnMontageEnded::CreateUObject(this, &ThisClass::OnSkillMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, SkillMontages[CurrentMontageIndex]);

	FOnMontageBlendingOutStarted BlendOutDelegate = FOnMontageBlendingOutStarted::CreateUObject(this, &ThisClass::OnSkillMontageBlendingOutStarted);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, SkillMontages[CurrentMontageIndex]);
}

UMyCombatComponent* UMySkillData::GetCombatComponentFromAvatarActor() const
{
	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(AvatarActor))
	{
		return InstigatorCharacter->GetCombatComponent();
	}

	return nullptr;
}

UAnimInstance* UMySkillData::GetAnimInstanceFromAvatarActor() const
{
	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(AvatarActor))
	{
		return InstigatorCharacter->GetMesh()->GetAnimInstance();
	}
	
	return nullptr;
}
