// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/MyAnimInstance.h"
#include "Characters/MyCharacter.h"
#include "Characters/MyPlayerCharacter.h"
#include "Characters/MyCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "KismetAnimationLibrary.h"
#include "AnimationStateMachineLibrary.h"
#include "AnimExecutionContextLibrary.h"
#include "Animation/CachedAnimDataLibrary.h"
#include "Kismet/KismetMathLibrary.h"

// jump 상태 중 fall loop 상태로 전이되기 위한 GroundDistance 차이
constexpr float GroundDistanceErrorTolerance = 30.0f;

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
	LocomotionStateData.StateName = TEXT("OnGround");
	// Locomotion state 은 OnGround state를 구현한다
	WalkStateData.StateMachineName = TEXT("Locomotion");
	WalkStateData.StateName = TEXT("Walk");
	RunStateData.StateMachineName = TEXT("Locomotion");
	RunStateData.StateName = TEXT("Run");
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

	if (const AMyPlayerCharacter* PlayerCharacter = Cast<AMyPlayerCharacter>(Character))
	{
		TargetLockContext = PlayerCharacter->GetTargetLockContext();
	}
	else
	{
		TargetLockContext = FTargetLockContext();
	}

	InputVector = MovementComponent->GetLastInputVector();
	InputVector = UKismetMathLibrary::ClampVectorSize(InputVector, 0.f, 1.f);

	bIsOnGround = MovementComponent->IsMovingOnGround();
	MovementMode = MovementComponent->MovementMode;
	CustomMovementMode = static_cast<EMyCustomMovementMode>(MovementComponent->CustomMovementMode);
	MaxJumpHeight = MovementComponent->GetMaxJumpHeightWithJumpTime();
	if (MovementMode == MOVE_Falling)
	{
		const float GroundRangeBase = FMath::Max(MaxJumpHeight, InitialJumpMaxHeight);
		const float FloorTraceDistance = GroundRangeBase + GroundDistanceErrorTolerance + 1.0f;

		FFindFloorResult FloorResult;
		MovementComponent->ComputeFloorDist(
			Location,
			FloorTraceDistance,
			FloorTraceDistance,
			FloorResult,
			Character->GetCapsuleComponent()->GetScaledCapsuleRadius());

		GroundDistance = FloorResult.GetDistanceToFloor();
	}

	bIsRunning = Character->IsSprintActive();
	// HasAnyRootMotion()은 Root Motion Source만 검사하므로 몽타주 루트 모션도 별도로 포함한다.
	bHasRootMotion = Character->IsPlayingRootMotion() || Character->HasAnyRootMotion();
}

void UMyAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	CurrentPivotStateWeight = 0.0f;
	TargetLockTurnInPlaceStateWeight = 0.0f;

	if (bWasTargetLocked != TargetLockContext.bIsTargetLocked)
	{
		bNeedsStartState = !TargetLockContext.bIsTargetLocked;
	}

	if (bWasTargetLocked && !TargetLockContext.bIsTargetLocked)
	{
		PrimaryTargetRotation = SecondaryTargetRotation = WorldRotation;
	}
	bWasTargetLocked = TargetLockContext.bIsTargetLocked;

	UpdateLocationData(DeltaSeconds);
	UpdateRotationData(DeltaSeconds);
	UpdateVelocityData(DeltaSeconds);
	UpdateAccelerationData(DeltaSeconds);
	UpdateJumpFallData(DeltaSeconds);

	LocomotionStateLastUpdate = LocomotionState;
	DetermineLocomotionState();

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

		VerticalDisplacementSinceLastUpdate = Location.Z - LocationLastUpdate.Z;
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
	Speed = WorldVelocity.Size();
	bHasVelocity = !WorldVelocity.IsZero();
	LocalVelocity = WorldRotation.UnrotateVector(WorldVelocity);
}

void UMyAnimInstance::UpdateAccelerationData(float DeltaSeconds)
{
	bHasAcceleration = !WorldAcceleration.IsZero();

	const FVector AccelerationNormal2D = WorldAcceleration.GetSafeNormal2D();
	VelocityAccelDot = WorldVelocity.GetSafeNormal2D() | AccelerationNormal2D;

	const bool bHasOppositeAccelLastUpdate = bHasOppositeAccel;
	bHasOppositeAccel = VelocityAccelDot < -0.5f;
	bAccelDotReversed = !bHasOppositeAccelLastUpdate && bHasOppositeAccel;
}

void UMyAnimInstance::UpdateJumpFallData(float DeltaSeconds)
{
	bool bWasInAirLastUpdate = bIsJumping || bIsFalling;
	bIsJumping = bIsFalling = false;
	if (MovementMode == MOVE_Falling)
	{
		bIsJumping = WorldVelocity.Z > 0.0f;
		bIsFalling = !bIsJumping;

		bGroundMovedFarther = false;
		bGroundMovedCloser = false;

		if (!bWasInAirLastUpdate)
		{
			ExpectedGroundDistance = GroundDistance;
		}
		else
		{
			ExpectedGroundDistance += VerticalDisplacementSinceLastUpdate;

			if (bIsFalling)
			{
				const float GroundDistanceError = GroundDistance - ExpectedGroundDistance;
				if (GroundDistanceError > GroundDistanceErrorTolerance)
				{
					// 지면이 멀어졌다
					bGroundMovedFarther = true;
					ExpectedGroundDistance = GroundDistance;
				}
				else if (GroundDistanceError < -GroundDistanceErrorTolerance)
				{
					// 지면이 가까워졌다
					bGroundMovedCloser = true;
					ExpectedGroundDistance = GroundDistance;
				}
			}
		}
	}
	else
	{
		bGroundMovedFarther = false;
		bGroundMovedCloser = false;
		InitialJumpMaxHeight = 0.0f;
		ExpectedGroundDistance = 0.0f;
	}

	if (!bWasInAirLastUpdate && bIsJumping)
	{
		const bool bWasMovingState = LocomotionStateLastUpdate == ELocomotionState::Walk ||
			LocomotionStateLastUpdate == ELocomotionState::Run;
		constexpr float JumpFromMoveSpeedThreshold = 1.0f;
		bJumpStartedFromMove = bWasMovingState ||
			WorldVelocity.SizeSquared2D() > FMath::Square(JumpFromMoveSpeedThreshold);
	}

}

void UMyAnimInstance::DetermineLocomotionState()
{
	if (!bIsOnGround)
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
		if (CanEnterPivot())
		{
			LocomotionState = ELocomotionState::Pivot;
			return;
		}

		if (InputVector.IsNearlyZero())
		{
			LocomotionState = ELocomotionState::Idle;
			return;
		}
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
	if (TargetLockContext.bIsTargetLocked)
	{
		return false;
	}

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
	if (TargetLockContext.bIsTargetLocked)
	{
		return true;
	}

	if (!bHasAcceleration || bHasRootMotion)
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

bool UMyAnimInstance::CanUseTargetLockTurnInPlace() const
{
	return TargetLockContext.bIsTargetLocked &&
		bIsOnGround &&
		!bHasRootMotion &&
		LocomotionState == ELocomotionState::Idle;
}

void UMyAnimInstance::SetupTargetLockTurnInPlaceValues(float SignedTurnAngle)
{
	TargetLockTurnInPlaceAngle = FRotator::NormalizeAxis(SignedTurnAngle);

	FRotator TargetRotation = WorldRotation;
	TargetRotation.Yaw = FRotator::NormalizeAxis(WorldRotation.Yaw + TargetLockTurnInPlaceAngle);
	PrimaryTargetRotation = SecondaryTargetRotation = TargetRotation;
}

void UMyAnimInstance::SetupMoveState()
{
	StartAngle = 0.0f;

	bNeedsStartState = LocomotionStateLastUpdate != ELocomotionState::Pivot &&
		!TargetLockContext.bIsTargetLocked;
	if (!bNeedsStartState)
	{
		return;
	}

	const float InputRotationYaw = InputVector.IsNearlyZero() ? WorldRotation.Yaw : InputVector.Rotation().Yaw;
	const FRotator InputRotation = FRotator(0.f, InputRotationYaw, 0.f);

	PrimaryTargetRotation = SecondaryTargetRotation = InputRotation;
	StartAngle = (InputRotation - WorldRotation).GetNormalized().Yaw;
}

float UMyAnimInstance::ComputeCycleAnimPlayRate(float CharacterSpeed) const
{
	float NewPlayRate = UKismetMathLibrary::SafeDivide(CharacterSpeed, FMath::Clamp(GetCurveValue(TEXT("MoveData_Speed")), 50.f, 1000.f));
	return FMath::Max(0.8f, NewPlayRate);
}

float UMyAnimInstance::CalculateCurveRotationYaw()
{
	float StateWeight = 0.0f;
	float RotationAngle = 0.0f;
	FName RotationCurveName = NAME_None;

	if (LocomotionState == ELocomotionState::Pivot)
	{
		StateWeight = CurrentPivotStateWeight;
		RotationAngle = PivotAngle;
		RotationCurveName = TEXT("MoveData_PivotRotationDelta");
	}
	else if (LocomotionState == ELocomotionState::Walk)
	{
		// start curve 값이 0인 이후부터 cycle로 전이되므로 start state 대신 walk state를 사용해도 괜찮다
		StateWeight = UCachedAnimDataLibrary::StateMachine_GetGlobalWeight(this, WalkStateData);
		RotationAngle = StartAngle;
		RotationCurveName = TEXT("MoveData_WalkRotationDelta");
	}
	else if (LocomotionState == ELocomotionState::Run)
	{
		// start curve 값이 0인 이후부터 cycle로 전이되므로 start state 대신 run state를 사용해도 괜찮다
		StateWeight = UCachedAnimDataLibrary::StateMachine_GetGlobalWeight(this, RunStateData);
		RotationAngle = StartAngle;
		RotationCurveName = TEXT("MoveData_RunRotationDelta");
	}
	else
	{
		StateWeight = TargetLockTurnInPlaceStateWeight;
		RotationAngle = TargetLockTurnInPlaceAngle;
		RotationCurveName = TEXT("IdleData_TurnInPlaceRotationDelta");
	}

	const float ClampedStateWeight = FMath::Clamp(StateWeight, 0.0f, 1.0f);
	const float SourceCurveYaw = GetCurveValue(RotationCurveName);
	const float NormalizedCurveYaw = UKismetMathLibrary::SafeDivide(SourceCurveYaw, ClampedStateWeight);
	const float ScaledCurveYaw = NormalizedCurveYaw * -RotationAngle;

	return FMath::Lerp(-RotationAngle, ScaledCurveYaw, ClampedStateWeight);
}

void UMyAnimInstance::UpdateCharacterRotation()
{
	if (CustomMovementMode == EMyCustomMovementMode::Climbing)
	{
		PrimaryTargetRotation = SecondaryTargetRotation = WorldRotation;
		return;
	}

	if (bHasRootMotion)
	{
		PrimaryTargetRotation = SecondaryTargetRotation = WorldRotation;
		return;
	}

	if (!UCachedAnimDataLibrary::StateMachine_IsStateRelevant(this, LocomotionStateData))
	{
		PrimaryTargetRotation = SecondaryTargetRotation = WorldRotation;
		return;
	}

	const bool bIsTargetLockTurnInPlace =
		CanUseTargetLockTurnInPlace() &&
		TargetLockTurnInPlaceStateWeight > UE_KINDA_SMALL_NUMBER;
	const bool bHasRotationCurve = LocomotionState != ELocomotionState::Idle;
	if (!bHasRotationCurve && !bIsTargetLockTurnInPlace)
	{
		return;
	}

	// pivot 모션의 회전 방향과 반대로 회전하는 증상을 방지하기 위해
	if (!bIsTargetLockTurnInPlace && LocomotionState != ELocomotionState::Pivot)
	{
		if (TargetLockContext.bIsTargetLocked)
		{
			PrimaryTargetRotation = TargetLockContext.CharacterTargetRotation;
		}
		else if (!InputVector.IsNearlyZero())
		{
			PrimaryTargetRotation = FMath::RInterpConstantTo(PrimaryTargetRotation, InputVector.Rotation(), GetDeltaSeconds(), PrimaryRotationInterpSpeed);
		}
	}
	SecondaryTargetRotation = FMath::RInterpTo(SecondaryTargetRotation, PrimaryTargetRotation, GetDeltaSeconds(), SecondaryRotationInterpSpeed);

	const float CurveRotationYaw = CalculateCurveRotationYaw();

	FRotator NewRotation = UKismetMathLibrary::MakeRotator(
		SecondaryTargetRotation.Roll, SecondaryTargetRotation.Pitch, SecondaryTargetRotation.Yaw + CurveRotationYaw);
	Character->SetActorRotation(NewRotation);
}

void UMyAnimInstance::SetupIdleState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	const bool bWasMoving = LocomotionStateLastUpdate == ELocomotionState::Walk ||
		LocomotionStateLastUpdate == ELocomotionState::Run;

	bNeedsStopState = bWasMoving &&
		bIsOnGround &&
		!bHasRootMotion &&
		!TargetLockContext.bIsTargetLocked &&
		InputVector.IsNearlyZero();
}

void UMyAnimInstance::SetupWalkState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	if (LocomotionStateLastUpdate == ELocomotionState::Walk)
	{
		// 애님 그래프만 재초기화된 경우 Start를 다시 재생하지 않는다.
		bNeedsStartState = false;
		StartAngle = 0.0f;
		return;
	}

	SetupMoveState();
}

void UMyAnimInstance::SetupRunState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	if (LocomotionStateLastUpdate == ELocomotionState::Run)
	{
		// 애님 그래프만 재초기화된 경우 Start를 다시 재생하지 않는다.
		bNeedsStartState = false;
		StartAngle = 0.0f;
		return;
	}

	SetupMoveState();
}

void UMyAnimInstance::UpdateWalkState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	WalkStateWeight = UAnimExecutionContextLibrary::GetCurrentWeight(Context);
	PlayRate = ComputeCycleAnimPlayRate(Speed);
}

void UMyAnimInstance::UpdateRunState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	RunStateWeight = UAnimExecutionContextLibrary::GetCurrentWeight(Context);
	PlayRate = ComputeCycleAnimPlayRate(Speed);
}

void UMyAnimInstance::SetupJumpState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	// Jump 높이 커브의 환산 스케일
	InitialJumpMaxHeight = MaxJumpHeight;

	ExpectedGroundDistance = GroundDistance;
	bGroundMovedFarther = false;
	bGroundMovedCloser = false;
}

void UMyAnimInstance::SetupFallLandState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	ExpectedGroundDistance = GroundDistance;
	bGroundMovedFarther = false;
	bGroundMovedCloser = false;

	bFallLandEndNotified = false;
}

bool UMyAnimInstance::IsMovementWithinThresholds(float MinCurrentSpeed, float MinMaxSpeed, float MinInputAcceleration) const
{
	return MinCurrentSpeed <= Speed && MinMaxSpeed <= MaxSpeed && MinInputAcceleration <= InputVector.Length();
}

