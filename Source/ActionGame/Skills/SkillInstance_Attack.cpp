// Fill out your copyright notice in the Description page of Project Settings.


#include "Skills/SkillInstance_Attack.h"
#include "Skills/SkillData_Attack.h"
#include "Characters/MyCharacter.h"
#include "Combat/MyCombatComponent.h"
#include "Items/MyWeapon.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Combat/HitReceiver.h"
#include "MotionWarpingComponent.h"

bool USkillInstance_Attack::Initialize(UMySkillData* InSkillData, AActor* InAvatarActor, AActor* InSourceItem)
{
	const USkillData_Attack* AttackData = Cast<USkillData_Attack>(InSkillData);
	if (!AttackData || !Cast<AMyWeapon>(InSourceItem) || AttackData->AttackComboWindow <= 0.0f
		|| AttackData->SkillMontages.IsEmpty() || !AttackData->LungeAttackMontage
		|| AttackData->AttackRangeOffset < 0.0f || AttackData->AttackWarpTargetName.IsNone())
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
	ClearPendingAttackEndState();
	return Super::Initialize(InSkillData, InAvatarActor, InSourceItem);
}

void USkillInstance_Attack::Deinitialize()
{
	ClearAttackWarpTarget();

	ResetComboState();
	ClearPendingAttackEndState();
	Super::Deinitialize();
}

bool USkillInstance_Attack::CanExecuteSkill()
{
	// 연타에 의해 montage를 준비하기 전에 호출되지 않도록 막는다
	if (bInAttackComboWindow)
	{
		return IsSkillActive();
	}

	return Super::CanExecuteSkill();
}

bool USkillInstance_Attack::ExecuteSkill(const FInputActionInstance& Instance, const FVector2D& MovementVector, AActor* LockedTarget)
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
	ClearAttackWarpTarget();

	const bool bHasLockedTarget = IsValid(LockedTarget);
	float CurrentTargetDistance = 0.0f;
	float CapsuleContactDistance = 0.0f;
	const bool bHasCapsuleAttackDistances = 
		bHasLockedTarget &&
		CalculateCapsuleAttackDistances(AvatarCharacter, LockedTarget, CurrentTargetDistance, CapsuleContactDistance);
	const float NormalAttackMaxDistance = CapsuleContactDistance + AttackData->AttackRangeOffset;
	const bool bShouldLunge = bHasCapsuleAttackDistances && CurrentTargetDistance > NormalAttackMaxDistance;

	if (bShouldLunge)
	{
		ResetComboState();
	}

	if (bHasLockedTarget)
	{
		FVector DesiredLookAtVector = LockedTarget->GetActorLocation() - AvatarCharacter->GetActorLocation();
		DesiredLookAtVector.Z = 0.0f;
		if (!DesiredLookAtVector.IsNearlyZero())
		{
			const FRotator DesiredRotation = FRotationMatrix::MakeFromXZ(DesiredLookAtVector, AvatarCharacter->GetActorUpVector()).Rotator();
			AvatarCharacter->SetActorRotation(DesiredRotation);
		}
	}
	else if (!MovementVector.IsNearlyZero())
	{
		AController* AvatarController = AvatarCharacter->GetController();
		if (!AvatarController)
		{
			return false;
		}

		const FRotator ControlRotation = AvatarController->GetControlRotation();
		const FRotator YawRotation(0, ControlRotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		const FVector DesiredLookAtVector = ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X;
		const FRotator DesiredRotation = FRotationMatrix::MakeFromXZ(DesiredLookAtVector, AvatarCharacter->GetActorUpVector()).Rotator();
		AvatarCharacter->SetActorRotation(DesiredRotation);
	}

	UAnimMontage* AttackMontage = bShouldLunge
		? AttackData->LungeAttackMontage
		: AttackData->SkillMontages[CurrentMontageIndex];
	if (!PlaySkillMontage(AttackMontage))
	{
		return false;
	}

	if (bHasCapsuleAttackDistances)
	{
		const float WarpStopDistance = FMath::Min(CurrentTargetDistance, CapsuleContactDistance);
		if (!SetupAttackWarpTarget(AvatarCharacter, LockedTarget, WarpStopDistance))
		{
			StopActiveMontage();
			return false;
		}
	}

	SourceWeapon->InitWeaponForAttack(this, AttackMontage);
	return true;
}

void USkillInstance_Attack::OnSkillEnd(bool bCanceled)
{
	if (bCanceled)
	{
		ResetComboState();
		ClearAttackWarpTarget();
		ClearPendingAttackEndState();
	}

	Super::OnSkillEnd(bCanceled);
}

void USkillInstance_Attack::OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 MontageInstanceID)
{
	if (PendingAttackEndMontageInstanceIDs.Contains(MontageInstanceID))
	{
		const bool bAdvancesAttackCombo =
			PendingComboPolicyMontageInstanceID == MontageInstanceID && bPendingAttackAdvancesCombo;

		if (AMyWeapon* SourceWeapon = Cast<AMyWeapon>(SourceItem.Get()))
		{
			SourceWeapon->OnAttackEnd(MontageInstanceID, bAdvancesAttackCombo);
		}

		// 무기가 이미 활성 스킬 참조를 해제한 경우에도 종료 처리를 보장한다.
		if (PendingAttackEndMontageInstanceIDs.Contains(MontageInstanceID))
		{
			OnAttackEnd(MontageInstanceID, bAdvancesAttackCombo);
		}
	}

	if (Montage == ActiveMontage.Get())
	{
		ClearAttackWarpTarget();
	}

	OnSkillEnd(false);
}

void USkillInstance_Attack::OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted)
{
}

void USkillInstance_Attack::RegisterPendingAttackEnd(int32 MontageInstanceID, bool bAdvancesAttackCombo)
{
	if (MontageInstanceID == INDEX_NONE)
	{
		return;
	}

	PendingAttackEndMontageInstanceIDs.Add(MontageInstanceID);
	PendingComboPolicyMontageInstanceID = MontageInstanceID;
	bPendingAttackAdvancesCombo = bAdvancesAttackCombo;
}

void USkillInstance_Attack::OnAttackEnd(int32 MontageInstanceID, bool bAdvancesAttackCombo)
{
	if (PendingAttackEndMontageInstanceIDs.Remove(MontageInstanceID) == 0)
	{
		return;
	}

	if (PendingComboPolicyMontageInstanceID == MontageInstanceID)
	{
		PendingComboPolicyMontageInstanceID = INDEX_NONE;
		bPendingAttackAdvancesCombo = false;
	}

	UE_LOG(LogTemp, Display, TEXT("Attack end"));
	if (!bAdvancesAttackCombo)
	{
		return;
	}

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
	ResetComboState();
}

bool USkillInstance_Attack::CalculateCapsuleAttackDistances(const ACharacter* AvatarCharacter, const AActor* TargetActor,
	float& OutCurrentDistance, float& OutCapsuleContactDistance) const
{
	const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	if (!AvatarCharacter || !TargetCharacter || !AvatarCharacter->GetCapsuleComponent() || !TargetCharacter->GetCapsuleComponent())
	{
		return false;
	}

	const UCapsuleComponent* AvatarCapsule = AvatarCharacter->GetCapsuleComponent();
	const UCapsuleComponent* TargetCapsule = TargetCharacter->GetCapsuleComponent();
	OutCurrentDistance = FVector::Dist2D(AvatarCapsule->GetComponentLocation(), TargetCapsule->GetComponentLocation());
	OutCapsuleContactDistance = AvatarCapsule->GetScaledCapsuleRadius() + TargetCapsule->GetScaledCapsuleRadius();
	return true;
}

bool USkillInstance_Attack::SetupAttackWarpTarget(ACharacter* AvatarCharacter, AActor* TargetActor, float WarpStopDistance)
{
	const USkillData_Attack* AttackData = Cast<USkillData_Attack>(SkillData);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(AvatarCharacter);
	UMotionWarpingComponent* WarpingComponent = MyCharacter ? MyCharacter->GetMotionWarpingComponent() : nullptr;
	if (!AttackData || !IsValid(TargetActor) || !WarpingComponent)
	{
		return false;
	}

	const FVector AvatarLocation = AvatarCharacter->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	FVector DirectionToTarget = TargetLocation - AvatarLocation;
	DirectionToTarget.Z = 0.0f;

	FRotator WarpRotation = AvatarCharacter->GetActorRotation();
	if (DirectionToTarget.Normalize())
	{
		WarpRotation = DirectionToTarget.Rotation();
	}

	const FVector WarpLocation = TargetLocation - DirectionToTarget * FMath::Max(0.0f, WarpStopDistance);
	WarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
		AttackData->AttackWarpTargetName,
		WarpLocation,
		WarpRotation
	);
	return true;
}

void USkillInstance_Attack::ClearAttackWarpTarget()
{
	const USkillData_Attack* AttackData = Cast<USkillData_Attack>(SkillData);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(AvatarActor.Get());
	UMotionWarpingComponent* WarpingComponent = MyCharacter ? MyCharacter->GetMotionWarpingComponent() : nullptr;
	if (WarpingComponent && AttackData)
	{
		WarpingComponent->RemoveWarpTarget(AttackData->AttackWarpTargetName);
	}
}

void USkillInstance_Attack::ClearPendingAttackEndState()
{
	PendingAttackEndMontageInstanceIDs.Reset();
	PendingComboPolicyMontageInstanceID = INDEX_NONE;
	bPendingAttackAdvancesCombo = false;
}

void USkillInstance_Attack::ResetComboState()
{
	if (AvatarActor.IsValid())
	{
		AvatarActor.Get()->GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimerHandle);
	}

	CurrentMontageIndex = 0;
	bInAttackComboWindow = false;
}

void USkillInstance_Attack::OnAttackHit(AActor* Instigator, AActor* TargetActor, const FVector& ImpactPoint)
{
	if (!IsValid(Instigator) || !IsValid(TargetActor))
	{
		return;
	}

	IHitReceiver* HitReceiver = Cast<IHitReceiver>(TargetActor);
	if (!HitReceiver)
	{
		return;
	}

	FVector ImpactDirection = (ImpactPoint - Instigator->GetActorLocation()).GetSafeNormal();
	if (ImpactDirection.IsNearlyZero())
	{
		ImpactDirection = (TargetActor->GetActorLocation() - Instigator->GetActorLocation()).GetSafeNormal();
	}

	FHitReceiveContext HitContext;
	HitContext.Instigator = Instigator;
	HitContext.DamageCauser = Cast<AActor>(SourceItem.Get());
	HitContext.ImpactPoint = ImpactPoint;
	HitContext.ImpactDirection = ImpactDirection;

	HitReceiver->ReceiveHit(HitContext);
}
