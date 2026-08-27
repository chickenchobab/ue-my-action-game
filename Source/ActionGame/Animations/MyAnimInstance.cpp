// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/MyAnimInstance.h"
#include "Characters/MyCharacter.h"
#include "Characters/MyCharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "AnimationStateMachineLibrary.h"
#include "AnimExecutionContextLibrary.h"
#include "Animation/CachedAnimDataLibrary.h"
#include "Kismet/KismetMathLibrary.h"

UMyAnimInstance::UMyAnimInstance()
{
}

void UMyAnimInstance::NativeInitializeAnimation()
{
	if (AMyCharacter* OwningCharacter = Cast<AMyCharacter>(GetOwningActor()))
	{
		Character = OwningCharacter;
		MovementComponent = Cast<UMyCharacterMovementComponent>(OwningCharacter->GetCharacterMovement());
	}

	bIsFirstUpdate = true;

	LocomotionStateData.StateMachineName = TEXT("MainStates");
	LocomotionStateData.StateName = TEXT("Locomotion");
	WalkStateData.StateMachineName = TEXT("Locomotion");
	WalkStateData.StateName = TEXT("Walk");
	RunStateData.StateMachineName = TEXT("Locomotion");
	RunStateData.StateName = TEXT("Run");
	PivotStateData.StateMachineName = TEXT("Locomotion");
	PivotStateData.StateName = TEXT("Pivot");
}

void UMyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (!IsValid(Character))
	{
		return;
	}

	WorldVelocity = MovementComponent->Velocity;
	MaxSpeed = MovementComponent->GetMaxSpeed();

	WorldAcceleration = MovementComponent->GetCurrentAcceleration();
	GroundFriction = MovementComponent->GroundFriction;

	Location = Character->GetActorLocation();
	WorldRotation = Character->GetActorRotation();
	Forward = Character->GetActorForwardVector();

	InputVector = MovementComponent->GetLastInputVector();
	InputVector = UKismetMathLibrary::ClampVectorSize(InputVector, 0.f, 1.f);

	bIsFalling = MovementComponent->IsFalling();
	bIsRunning = Character->IsSprintActive();
	bHasRootMotion = Character->HasAnyRootMotion();
}

void UMyAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	UpdateLocationData(DeltaSeconds);
	UpdateRotationData(DeltaSeconds);
	UpdateVelocityData(DeltaSeconds);
	UpdateAccelerationData(DeltaSeconds);

	LocomotionStateLastUpdate = LocomotionState;
	DetermineLocomotionState();
	if (LocomotionState == ELocomotionState::Walk || LocomotionState == ELocomotionState::Run)
	{
		if (LocomotionState != LocomotionStateLastUpdate)
		{
			SetupMoveState();
		}
		UpdateLocomotionValues();
	}

	bIsFirstUpdate = false;
}

void UMyAnimInstance::NativePostEvaluateAnimation()
{
	if (IsValid(Character))
	{
		UpdateCharacterRotation();
	}
}

void UMyAnimInstance::UpdateLocationData(float DeltaSeconds)
{
	if (!bIsFirstUpdate)
	{
		DisplacementSinceLastUpdate = (Location - LocationLastUpdate).Size();
		DisplacementSpeed = (DeltaSeconds != 0.0f ? DisplacementSinceLastUpdate / DeltaSeconds : 0.0f);
	}

	LocationLastUpdate = Location;
}

void UMyAnimInstance::UpdateRotationData(float DeltaSeconds)
{
	if (!bIsFirstUpdate)
	{
		YawDelta = WorldRotation.Yaw - RotationLastUpdate.Yaw;
	}

	RotationLastUpdate = WorldRotation;
}

void UMyAnimInstance::UpdateVelocityData(float DeltaSeconds)
{
	bWasMovingLastUpdate = Speed > 0.0f;
	Speed = WorldVelocity.Size();
	bHasVelocity = !WorldVelocity.IsZero();
}

void UMyAnimInstance::UpdateAccelerationData(float DeltaSeconds)
{
	bHasAcceleration = !WorldAcceleration.IsZero();

	const FVector AccelerationNormal2D = WorldAcceleration.GetSafeNormal2D();
	VelocityAccelDot = WorldVelocity.GetSafeNormal2D() | AccelerationNormal2D;
	ForwardAccelDot = Forward | AccelerationNormal2D;

	const bool bHasOppositeAccelLastUpdate = bHasOppositeAccel;
	bHasOppositeAccel = VelocityAccelDot < -0.5f/*|| ForwardAccelDot < -0.5f*/;
	bAccelDotReversed = !bHasOppositeAccelLastUpdate && bHasOppositeAccel;

	VelocityAccelDotLastUpdate = VelocityAccelDot;
}

void UMyAnimInstance::DetermineLocomotionState()
{
	if (bIsFalling)
	{
		LocomotionState = ELocomotionState::Idle;
		return;
	}

	if (LocomotionState == ELocomotionState::Pivot)
	{
		if (!ShouldExitPivot())
		{
			return;
		}
	}
	else if (VelocityAccelDot < -0.5f)
	{
		LocomotionState = (CanEnterPivot() ? ELocomotionState::Pivot : ELocomotionState::Idle);
		return;
	}

	if (IsMovementWithinThresholds(1.f, 500.f, 0.5))
	{
		LocomotionState = ELocomotionState::Run;
	}
	else if (IsMovementWithinThresholds(1.f, 0.f, 0.01f))
	{
		LocomotionState = ELocomotionState::Walk;
	}
	else
	{
		LocomotionState = ELocomotionState::Idle;
	}
}

bool UMyAnimInstance::CanEnterPivot() const
{
	if (LocomotionState != ELocomotionState::Walk && LocomotionState != ELocomotionState::Run)
	{
		UE_LOG(LogTemp, Display, TEXT("Pivot failed(%d) : Should be moving"), GFrameNumber);
		return false;
	}

	if (bHasRootMotion)
	{
		return false;
	}

	// 여기에서 걸러지면 idle(stop) 상태로 이동한다
	if (!bHasAcceleration)
	{
		UE_LOG(LogTemp, Display, TEXT("Pivot failed(%d) : There is no acceleration"), GFrameNumber);
		return false;
	}

	return true;
}

bool UMyAnimInstance::ShouldExitPivot() const
{
	if (!bHasAcceleration)
	{
		return true;
	}

	if (!bPivotExitNotified)
	{
		return false;
	}

	return !bHasOppositeAccel; // Repivot 가능성이 없다
}

void UMyAnimInstance::SetupPivotValues()
{
	const FRotator InputRotation = InputVector.IsNearlyZero() ? WorldRotation : InputVector.Rotation();

	PivotAngle = (InputRotation - WorldRotation).GetNormalized().Yaw;
	PrimaryTargetRotation = SecondaryTargetRotation = InputRotation;

	bPivotExitNotified = false;
}

void UMyAnimInstance::SetupMoveState()
{
	float& StartAngle = LocomotionState == ELocomotionState::Walk ? WalkStartAngle : RunStartAngle;

	const float InputRotationYaw = InputVector.IsNearlyZero() ? WorldRotation.Yaw : InputVector.Rotation().Yaw;
	const FRotator InputRotation = FRotator(0.f, InputRotationYaw, 0.f);

	if (LocomotionStateLastUpdate == ELocomotionState::Pivot)
	{
		bNeedsStartState = false; // Anim layer의 cycle state에서 토글할 것이다
		StartAngle = 0.0f;
		return;
	}
	bNeedsStartState = true;

	PrimaryTargetRotation = SecondaryTargetRotation = InputRotation;
	StartAngle = (InputRotation - WorldRotation).GetNormalized().Yaw;
}

float UMyAnimInstance::ComputeLocomotionPlayRate(float CharacterSpeed) const
{
	return UKismetMathLibrary::SafeDivide(CharacterSpeed, FMath::Clamp(GetCurveValue(TEXT("MoveData_Speed")), 50.f, 1000.f));
}

void UMyAnimInstance::UpdateLocomotionValues()
{
	PlayRate = ComputeLocomotionPlayRate(Speed);
}

void UMyAnimInstance::UpdateCharacterRotation()
{
	if (!UCachedAnimDataLibrary::StateMachine_IsStateRelevant(this, LocomotionStateData) || bHasRootMotion)
	{
		PrimaryTargetRotation = SecondaryTargetRotation = WorldRotation;
		return;
	}

	if (LocomotionState != ELocomotionState::Walk && LocomotionState != ELocomotionState::Run
		&& LocomotionState != ELocomotionState::Pivot)
	{
		return;
	}

	FName RotationCurveName;
	const FCachedAnimStateData* StateData = nullptr;

	if (LocomotionState == ELocomotionState::Pivot)
	{
		RotationCurveName = TEXT("MoveData_PivotRotationDelta");
		StateData = &PivotStateData;
	}
	else if (LocomotionState == ELocomotionState::Walk)
	{
		RotationCurveName = TEXT("MoveData_WalkRotationDelta");
		StateData = &WalkStateData;
	}
	else
	{
		RotationCurveName = TEXT("MoveData_RunRotationDelta");
		StateData = &RunStateData;
	}

	if (LocomotionState != ELocomotionState::Pivot && !InputVector.IsNearlyZero())
	{
		PrimaryTargetRotation = FMath::RInterpConstantTo(PrimaryTargetRotation, InputVector.Rotation(), GetDeltaSeconds(), PrimaryRotationInterpSpeed);
	}
	SecondaryTargetRotation = FMath::RInterpTo(SecondaryTargetRotation, PrimaryTargetRotation, GetDeltaSeconds(), SecondaryRotationInterpSpeed);

	const float StateWeight = UCachedAnimDataLibrary::StateMachine_GetGlobalWeight(this, *StateData);
	float CurveRotationYaw = UKismetMathLibrary::SafeDivide(GetCurveValue(RotationCurveName), StateWeight);
	FRotator NewRotation = UKismetMathLibrary::MakeRotator(SecondaryTargetRotation.Roll, SecondaryTargetRotation.Pitch, SecondaryTargetRotation.Yaw + CurveRotationYaw);
	Character->SetActorRotation(NewRotation);
}

void UMyAnimInstance::SetupIdleState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
}

void UMyAnimInstance::UpdateIdleState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
}

void UMyAnimInstance::SetupMoveState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
}

void UMyAnimInstance::UpdateWalkState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	WalkStateWeight = UAnimExecutionContextLibrary::GetCurrentWeight(Context);
}

void UMyAnimInstance::UpdateRunState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	RunStateWeight = UAnimExecutionContextLibrary::GetCurrentWeight(Context);
}

void UMyAnimInstance::SetupPivotState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	// Repivot 시 play rate의 일관성을 위해 캐싱한다
	PivotInitialSpeed = MaxSpeed;
}

void UMyAnimInstance::UpdatePivotState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
}

bool UMyAnimInstance::IsMovementWithinThresholds(float MinCurrentSpeed, float MinMaxSpeed, float MinInputAcceleration) const
{
	return MinCurrentSpeed <= Speed && MinMaxSpeed <= MaxSpeed && MinInputAcceleration <= InputVector.Length();
}

ECardinalDirection UMyAnimInstance::SelectCardinalDirectionFromAngle(float Angle, float DeadZone, ECardinalDirection CurrentDirection, bool bUseCurrentDirection)
{
	float AbsAngle = FMath::Abs(Angle);

	float FwdDeadZone = DeadZone;
	float BwdDeadZone = DeadZone;
	// It should be harder to leave Fwd or Bwd.
	if (bUseCurrentDirection)
	{
		if (CurrentDirection == ECardinalDirection::Forward)
		{
			FwdDeadZone *= 2.0f;
		}
		else if (CurrentDirection == ECardinalDirection::Backward)
		{
			BwdDeadZone *= 2.0f;
		}
	}

	if (AbsAngle <= 45.0f + FwdDeadZone)
	{
		return ECardinalDirection::Forward;
	}
	if (AbsAngle >= 135.0f - BwdDeadZone)
	{
		return ECardinalDirection::Backward;
	}
	if (Angle > 0.0f)
	{
		return ECardinalDirection::Right;
	}
	return ECardinalDirection::Left;
}
