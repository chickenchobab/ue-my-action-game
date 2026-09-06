// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/SkillInstance_Attack.h"
#include "Skills/SkillData_Attack.h"
#include "Combat/MyCombatComponent.h"
#include "Items/MyWeapon.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

bool USkillInstance_Attack::Initialize(UMySkillData* InSkillData, AActor* InAvatarActor, AActor* InSourceItem)
{
	const USkillData_Attack* AttackData = Cast<USkillData_Attack>(InSkillData);
	if (!AttackData || !Cast<AMyWeapon>(InSourceItem) || AttackData->AttackComboWindow <= 0.0f || AttackData->SkillMontages.IsEmpty())
	{
		return false;
	}

	for (const TObjectPtr<UAnimMontage>& SkillMontage : AttackData->SkillMontages)
	{
		if (!SkillMontage)
		{
			return false;
		}
	}

	bInAttackComboWindow = false;
	return Super::Initialize(InSkillData, InAvatarActor, InSourceItem);
}

void USkillInstance_Attack::Deinitialize()
{
	if (AvatarActor.IsValid())
	{
		AvatarActor.Get()->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);

		if (ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor.Get()))
		{
			AvatarCharacter->GetCharacterMovement()->RotationRate.Yaw = 0.0f;
		}
	}

	bInAttackComboWindow = false;
	Super::Deinitialize();
}

bool USkillInstance_Attack::CanExecuteSkill()
{
	// 스킬을 비활성화하는 SkillMontageEnded 콜백이 다음 콤보를 준비하는 AttackEnd보다 먼저 호출된다
	// 연타에 의해 montage를 준비하기 전에 호출되지 않도록 막는다
	if (bInAttackComboWindow)
	{
		return IsSkillActive();
	}

	return Super::CanExecuteSkill();
}

bool USkillInstance_Attack::ExecuteSkill(const FInputActionInstance& Instance, const FVector2D& MovementVector)
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor.Get());
	const USkillData_Attack* AttackData = Cast<USkillData_Attack>(SkillData);
	AMyWeapon* SourceWeapon = Cast<AMyWeapon>(SourceItem.Get());
	UMyCombatComponent* CombatComponent = GetCombatComponentFromAvatarActor();
	if (!AvatarCharacter || !AttackData || !SourceWeapon || !CombatComponent || CombatComponent->GetCurrentWeapon() != SourceWeapon)
	{
		return false;
	}

	bInAttackComboWindow = false;
	AvatarCharacter->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);

	if (!MovementVector.IsNearlyZero())
	{
		AController* AvatarController = AvatarCharacter->GetController();
		if (!AvatarController)
		{
			return false;
		}

		AvatarCharacter->GetCharacterMovement()->RotationRate.Yaw = -1.f;

		const FRotator ControlRotation = AvatarController->GetControlRotation();
		const FRotator YawRotation(0, ControlRotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		const FVector DesiredLookAtVector = ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X;
		const FRotator DesiredRotation = FRotationMatrix::MakeFromXZ(DesiredLookAtVector, AvatarCharacter->GetActorUpVector()).Rotator();
		AvatarCharacter->SetActorRotation(DesiredRotation);
	}

	if (!PlaySkillMontage())
	{
		AvatarCharacter->GetCharacterMovement()->RotationRate.Yaw = 0.0f;
		return false;
	}

	SourceWeapon->InitWeaponForAttack(this, AttackData->SkillMontages[CurrentMontageIndex]);
	return true;
}

void USkillInstance_Attack::OnSkillEnd(bool bCanceled)
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor.Get());

	if (bCanceled)
	{
		bInAttackComboWindow = false;
		if (AvatarCharacter)
		{
			AvatarCharacter->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);
		}
	}

	Super::OnSkillEnd(bCanceled);

	if (AvatarCharacter && ActiveCount == 0)
	{
		AvatarCharacter->GetCharacterMovement()->RotationRate.Yaw = 0.f;
	}
}

void USkillInstance_Attack::OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnSkillEnd(false);
}

void USkillInstance_Attack::OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted)
{
}

void USkillInstance_Attack::OnAttackEnd()
{
	UE_LOG(LogTemp, Display, TEXT("Attack end"));
	const USkillData_Attack* AttackData = Cast<USkillData_Attack>(SkillData);
	if (!AvatarActor.IsValid() || !AttackData || !AttackData->SkillMontages.IsValidIndex(CurrentMontageIndex))
	{
		return;
	}

	if (CurrentMontageIndex == AttackData->SkillMontages.Num() - 1)
	{
		CurrentMontageIndex = 0;
	}
	else
	{
		CurrentMontageIndex = CurrentMontageIndex + 1;
		bInAttackComboWindow = true;

		AvatarActor.Get()->GetWorld()->GetTimerManager().SetTimer(
			ComboWindowTimerHandle,
			this,
			&ThisClass::OnComboWindowExpired,
			AttackData->AttackComboWindow,
			false
		);
	}
}

void USkillInstance_Attack::OnComboWindowExpired()
{
	CurrentMontageIndex = 0;
	bInAttackComboWindow = false;
}

void USkillInstance_Attack::OnAttackHit(AActor* Instigator, TArray<AActor*>& TargetActors)
{
	UE_LOG(LogTemp, Display, TEXT("Attack Hit"));
	for (auto TargetActor : TargetActors)
	{
		if (TargetActor)
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *TargetActor->GetName());
		}
	}
}
