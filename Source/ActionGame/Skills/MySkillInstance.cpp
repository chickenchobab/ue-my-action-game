// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/MySkillInstance.h"
#include "Skills/MySkillData.h"
#include "Characters/MyCharacter.h"
#include "Combat/MyCombatComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UMySkillInstance::Initialize(UMySkillData* InSkillData, AActor* InAvatarActor, AActor* InSourceItem)
{
	if (bInitialized || !InSkillData || !InAvatarActor || !InSourceItem)
	{
		return false;
	}

	SkillData = InSkillData;
	AvatarActor = InAvatarActor;
	SourceItem = InSourceItem;

	ExecutionGrantCount = !SkillData->bComboOnly;
	ActiveCount = 0;
	CurrentMontageIndex = 0;
	bInitialized = true;

	return true;
}

void UMySkillInstance::Deinitialize()
{
	if (!bInitialized)
	{
		return;
	}

	CancelSkill();
	ClearExecutionGrantTimers();
	StopActiveMontage();

	ExecutionGrantCount = 0;
	ActiveCount = 0;
	CurrentMontageIndex = 0;
	ActiveMontage.Reset();
	SourceItem.Reset();
	AvatarActor.Reset();
	SkillData = nullptr;
	bInitialized = false;
}

void UMySkillInstance::TryExecuteSkill(const FInputActionInstance& Instance, const FVector2D& MovementVector, AActor* LockedTarget)
{
	if (!bInitialized || !SkillData || !AvatarActor.IsValid())
	{
		return;
	}

	if (CanExecuteSkill())
	{
		UE_LOG(LogTemp, Display, TEXT("Execute skill"));
		// 콤보 montage 교체 중 이전 종료 콜백이 와도 기존 카운트 관계를 유지한다.
		const uint32 ActiveCountBeforeExecution = ActiveCount;
		++ActiveCount;
		if (!ExecuteSkill(Instance, MovementVector, LockedTarget))
		{
			ActiveCount = ActiveCountBeforeExecution;
			return;
		}

		CommitCostsAndCooldown();
	}
}

void UMySkillInstance::CancelSkill()
{
	if (!IsSkillActive())
	{
		return;
	}

	StopActiveMontage();
	ActiveCount = 1;
	OnSkillEnd(true);
}

void UMySkillInstance::HandleSkillReleased(const FInputActionInstance& Instance, const FVector2D& MovementVector)
{
}

void UMySkillInstance::EnableExecution(float Duration)
{
	if (!AvatarActor.IsValid())
	{
		return;
	}

	++ExecutionGrantCount;

	if (Duration > 0.0f)
	{
		FTimerHandle& TimerHandle = ExecutionGrantTimerHandles.AddDefaulted_GetRef();
		AvatarActor.Get()->GetWorld()->GetTimerManager().SetTimer(
			TimerHandle,
			this,
			&ThisClass::ExpireExecutionGrant,
			Duration,
			false
		);
	}
}

void UMySkillInstance::ExpireExecutionGrant()
{
	if (ExecutionGrantCount > 0)
	{
		--ExecutionGrantCount;
	}
}

bool UMySkillInstance::CanExecuteSkill()
{
	if (!bInitialized || !SkillData || !AvatarActor.IsValid())
	{
		return false;
	}

	// TODO: 현재는 PerAvatar만 지원한다
	if (IsSkillActive())
	{
		return false;
	}

	// TODO: 콤보 공격
	if (ExecutionGrantCount <= 0)
	{
		return false;
	}

	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(AvatarActor.Get()))
	{
		if (InstigatorCharacter->GetCombatComponent()->GetCurrentMana() < SkillData->Cost)
		{
			return false;
		}

		// TODO: Check avatar tags
	}

	return true;
}

void UMySkillInstance::CommitCostsAndCooldown()
{
	// TODO
}

void UMySkillInstance::OnSkillEnd(bool bCanceled)
{
	if (ActiveCount == 0)
	{
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("Skill ended"));
	ActiveCount = bCanceled ? 0 : ActiveCount - 1;

	if (ActiveCount == 0)
	{
		ActiveMontage.Reset();
	}
}

bool UMySkillInstance::PlaySkillMontage()
{
	if (!SkillData->SkillMontages.IsValidIndex(CurrentMontageIndex))
	{
		return false;
	}

	return PlaySkillMontage(SkillData->SkillMontages[CurrentMontageIndex]);
}

bool UMySkillInstance::PlaySkillMontage(UAnimMontage* SkillMontage)
{
	UAnimInstance* AnimInstance = GetAnimInstanceFromAvatarActor();
	if (!AnimInstance || !SkillMontage || AnimInstance->Montage_Play(SkillMontage) <= 0.0f)
	{
		return false;
	}

	FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(SkillMontage);
	if (!MontageInstance)
	{
		AnimInstance->Montage_Stop(0.0f, SkillMontage);
		return false;
	}

	const int32 MontageInstanceID = MontageInstance->GetInstanceID();
	FOnMontageEnded EndDelegate = FOnMontageEnded::CreateUObject(
		this,
		&ThisClass::OnSkillMontageEnded,
		MontageInstanceID
	);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, SkillMontage);

	FOnMontageBlendingOutStarted BlendOutDelegate = FOnMontageBlendingOutStarted::CreateUObject(this, &ThisClass::OnSkillMontageBlendingOutStarted);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, SkillMontage);

	ActiveMontage = SkillMontage;
	return true;
}

void UMySkillInstance::ClearExecutionGrantTimers()
{
	if (AvatarActor.IsValid())
	{
		FTimerManager& TimerManager = AvatarActor.Get()->GetWorld()->GetTimerManager();
		for (FTimerHandle& TimerHandle : ExecutionGrantTimerHandles)
		{
			TimerManager.ClearTimer(TimerHandle);
		}
	}

	ExecutionGrantTimerHandles.Reset();
}

void UMySkillInstance::StopActiveMontage()
{
	UAnimInstance* AnimInstance = GetAnimInstanceFromAvatarActor();
	UAnimMontage* Montage = ActiveMontage.Get();
	if (!AnimInstance || !Montage)
	{
		ActiveMontage.Reset();
		return;
	}

	FOnMontageEnded EmptyEndDelegate;
	AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, Montage);

	FOnMontageBlendingOutStarted EmptyBlendOutDelegate;
	AnimInstance->Montage_SetBlendingOutDelegate(EmptyBlendOutDelegate, Montage);
	AnimInstance->Montage_Stop(0.0f, Montage);
	ActiveMontage.Reset();
}

UMyCombatComponent* UMySkillInstance::GetCombatComponentFromAvatarActor() const
{
	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(AvatarActor.Get()))
	{
		return InstigatorCharacter->GetCombatComponent();
	}

	return nullptr;
}

UAnimInstance* UMySkillInstance::GetAnimInstanceFromAvatarActor() const
{
	if (AMyCharacter* InstigatorCharacter = Cast<AMyCharacter>(AvatarActor.Get()))
	{
		return InstigatorCharacter->GetMesh()->GetAnimInstance();
	}

	return nullptr;
}
