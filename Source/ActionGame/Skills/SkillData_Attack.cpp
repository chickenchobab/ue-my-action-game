// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/SkillData_Attack.h"
#include "Combat/MyCombatComponent.h"
#include "Items/MyWeapon.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void USkillData_Attack::InitWithItem(AActor* OwningItem)
{
	if (AMyWeapon* Weapon = Cast<AMyWeapon>(OwningItem))
	{
		Weapon->OnAttackHit.AddUObject(this, &ThisClass::OnAttackHit);
	}
}

void USkillData_Attack::InitWithAvatar(AActor* NewAvatar)
{
	Super::InitWithAvatar(NewAvatar);

	bInAttackComboWindow = false;
	NewAvatar->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);
}

bool USkillData_Attack::CanExecuteSkill()
{
	// The montage end callback(causing deactivation) may be invoked before the attack end anim notify,
	// so check activity here to prevent from executing before the montage update
	if (bInAttackComboWindow)
	{
		return IsSkillActive();
	}

	return Super::CanExecuteSkill();
}

void USkillData_Attack::ExecuteSkill(const FInputActionInstance& Instance, const FVector2D& MovementVector)
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor.Get());
	if (!AvatarCharacter)
	{
		return;
	}

	bInAttackComboWindow = false;
	AvatarCharacter->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);

	CachedRotationRate = AvatarCharacter->GetCharacterMovement()->RotationRate;
	if (!MovementVector.IsNearlyZero())
	{
		AvatarCharacter->GetCharacterMovement()->RotationRate = FRotator(-1.0f, -1.0f, -1.0f);

		const FRotator ControlRotation = AvatarCharacter->GetController()->GetControlRotation();
		const FRotator YawRotation(0, ControlRotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		const FVector DesiredLookAtVector = ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X;
		const FRotator DesiredRotation = FRotationMatrix::MakeFromXZ(DesiredLookAtVector, AvatarCharacter->GetActorUpVector()).Rotator();
		AvatarCharacter->SetActorRotation(DesiredRotation);
	}

	PlaySkillMontage();
	if (CurrentMontageIndex >= SkillMontages.Num())
	{
		return;
	}

	UMyCombatComponent* CombatComponent = GetCombatComponentFromAvatarActor();
	if (AMyWeapon* Weapon = CombatComponent->GetCurrentWeapon())
	{
		Weapon->InitWeaponForAttack(this, SkillMontages[CurrentMontageIndex]);
	}
}

void USkillData_Attack::OnSkillEnd(bool bCanceled)
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor.Get());
	if (!AvatarCharacter)
	{
		return;
	}

	if (bCanceled)
	{
		bInAttackComboWindow = false;
		AvatarCharacter->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);
	}

	Super::OnSkillEnd(bCanceled);

	if (ActiveCount == 0)
	{
		AvatarCharacter->GetCharacterMovement()->RotationRate = CachedRotationRate;
	}
}

void USkillData_Attack::OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnSkillEnd(false);
}

void USkillData_Attack::OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted)
{
}

void USkillData_Attack::OnAttackEnd()
{
	UE_LOG(LogTemp, Display, TEXT("Attack end"));
	if (!AvatarActor.IsValid())
	{
		return;
	}

	if (CurrentMontageIndex == SkillMontages.Num() - 1)
	{
		CurrentMontageIndex = 0;
	}
	else
	{
		CurrentMontageIndex = CurrentMontageIndex + 1;
		bInAttackComboWindow = true;
		AvatarActor.Get()->GetWorld()->GetTimerManager().SetTimer(
			ComboWindowTimerHandle,
			[this]() {
				CurrentMontageIndex = 0;
				bInAttackComboWindow = false;
			},
			AttackComboWindow,
			false
		);
	}
}

void USkillData_Attack::OnAttackHit(AActor* Instigator, TArray<AActor*>& TargetActors)
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
