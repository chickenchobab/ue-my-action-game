// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/MyAnimLayers.h"
#include "Animations/MyAnimInstance.h"
#include "Animations/Libraries/MyAnimFunctionLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationStateMachineLibrary.h"
#include "BlendSpacePlayerLibrary.h"
#include "SequenceEvaluatorLibrary.h"
#include "SequencePlayerLibrary.h"
#include "AnimExecutionContextLibrary.h"
#include "AnimCharacterMovementLibrary.h"
#include "AnimDistanceMatchingLibrary.h"
#include "Animation/AnimCurveCompressionCodec_UniformIndexable.h"

static FName DistanceCurveName(TEXT("MoveData_Distance"));
static FName JumpHeightCurveName(TEXT("JumpData_Height"));
static FName JumpDistanceCurveName(TEXT("JumpData_Distance"));

void UMyAnimLayers::NativeInitializeAnimation()
{
	MainAnimInstance = Cast<UMyAnimInstance>(GetOwningComponent()->GetAnimInstance());
}

void UMyAnimLayers::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	if (!MainAnimInstance) // 에디터 크래시 방지
	{
		return;
	}

	MainAnimInstance->bShouldRepivot = 
		MainAnimInstance->LocomotionStateLastUpdate == ELocomotionState::Pivot &&
		MainAnimInstance->bAccelDotReversed &&
		(PivotStartingAcceleration | MainAnimInstance->WorldAcceleration.GetSafeNormal2D()) < -0.5f;
}

void UMyAnimLayers::UpdateCycleBlendSpaceAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FBlendSpacePlayerReference BlendSpacePlayer;
	bool bSuccessed = false;
	UBlendSpacePlayerLibrary::ConvertToBlendSpacePlayerPure(Node, BlendSpacePlayer, bSuccessed);
	if (!bSuccessed)
	{
		return;
	}

	float Direction = 0.0f;
	if (MainAnimInstance->TargetLockContext.bIsTargetLocked)
	{
		if (MainAnimInstance->LocalVelocity.IsNearlyZero())
		{
			return;
		}
		Direction = FMath::RadiansToDegrees(FMath::Atan2(MainAnimInstance->LocalVelocity.Y, MainAnimInstance->LocalVelocity.X));
	}

	const FVector BlendPosition(Direction, 0.0f, 0.0f);
	UMyAnimFunctionLibrary::SetBlendSpacePosition(BlendSpacePlayer, BlendPosition);
}

void UMyAnimLayers::SetupTargetLockTurnInPlaceAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	const float TargetLockYawError = FMath::FindDeltaAngleDegrees(
		MainAnimInstance->WorldRotation.Yaw,
		MainAnimInstance->TargetLockContext.CharacterTargetRotation.Yaw);
	const float TargetLockTurnAngle = FMath::Clamp(TargetLockYawError, -TargetLockTurnInPlaceMaxAngle, TargetLockTurnInPlaceMaxAngle);
	UAnimSequence* Sequence = GetDesiredTargetLockTurnInPlaceSequence(TargetLockTurnAngle);
	if (!Sequence)
	{
		return;
	}

	FSequencePlayerReference SequencePlayer;
	bool bSuccessed = false;
	USequencePlayerLibrary::ConvertToSequencePlayerPure(Node, SequencePlayer, bSuccessed);
	if (!bSuccessed)
	{
		return;
	}

	USequencePlayerLibrary::SetSequence(SequencePlayer, Sequence);
	USequencePlayerLibrary::SetAccumulatedTime(SequencePlayer, 0.0f);

	TargetLockTurnInPlaceTarget = MainAnimInstance->TargetLockContext.LockedTarget;
	MainAnimInstance->SetupTargetLockTurnInPlaceValues(TargetLockTurnAngle);
}

void UMyAnimLayers::UpdateTargetLockTurnInPlaceState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FAnimationStateResultReference StateResult;
	bool bSucceeded = false;
	UAnimationStateMachineLibrary::ConvertToAnimationStateResultPure(Node, StateResult, bSucceeded);
	if (!bSucceeded)
	{
		return;
	}

	if (UAnimationStateMachineLibrary::IsStateBlendingOut(Context, StateResult))
	{
		// 블렌드 아웃 커브가 액터 회전을 시작 각도로 되돌리지 않게 한다.
		MainAnimInstance->TargetLockTurnInPlaceStateWeight = 0.0f;
		return;
	}
	MainAnimInstance->TargetLockTurnInPlaceStateWeight = UAnimExecutionContextLibrary::GetCurrentWeight(Context);;
}

void UMyAnimLayers::SetupTargetLockTurnInPlaceRecoveryState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	MainAnimInstance->TargetLockTurnInPlaceStateWeight = 0.0f;
}

void UMyAnimLayers::SetupTargetLockTurnInPlaceRecoveryAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	if (!IdleSequence)
	{
		return;
	}

	FSequencePlayerReference SequencePlayer;
	bool bSuccessed = false;
	USequencePlayerLibrary::ConvertToSequencePlayerPure(Node, SequencePlayer, bSuccessed);
	if (!bSuccessed)
	{
		return;
	}

	USequencePlayerLibrary::SetSequence(SequencePlayer, IdleSequence);
	USequencePlayerLibrary::SetAccumulatedTime(SequencePlayer, 0.0f);
}

void UMyAnimLayers::SetupHangMoveAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);
	if (!bSuccessed)
	{
		return;
	}

	UAnimSequence* Sequence = GetDesiredHangMoveSequence();
	USequenceEvaluatorLibrary::SetSequence(SequenceEvaluator, Sequence);
}

void UMyAnimLayers::UpdateHangMoveAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);
	if (!bSuccessed)
	{
		return;
	}

	UAnimSequence* Sequence = GetDesiredHangMoveSequence();
	USequenceEvaluatorLibrary::SetSequenceWithInertialBlending(Context, SequenceEvaluator, Sequence, 0.2f);
	
	MainAnimInstance->PlayRate = MainAnimInstance->ComputeCycleAnimPlayRate(FMath::Abs(MainAnimInstance->LocalVelocity.Y));
	USequenceEvaluatorLibrary::AdvanceTime(Context, SequenceEvaluator, MainAnimInstance->PlayRate);
}

void UMyAnimLayers::SetupPivotAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);

	MainAnimInstance->SetupPivotValues();
	PivotStartingAcceleration = MainAnimInstance->WorldAcceleration.GetSafeNormal2D();

	UAnimSequence* Sequence = GetDesiredPivotSequence();
	USequenceEvaluatorLibrary::SetSequence(SequenceEvaluator, Sequence);

	USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, 0.0f);

	MainAnimInstance->bShouldRepivot = false;
}

void UMyAnimLayers::UpdatePivotAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	// Inertial transition이라 pivot state 하나만 업데이트된다
	MainAnimInstance->CurrentPivotStateWeight = UAnimExecutionContextLibrary::GetCurrentWeight(Context);

	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);

	const FVector PredictedPivotLocation = UAnimCharacterMovementLibrary::PredictGroundMovementPivotLocation(
		MainAnimInstance->WorldAcceleration, MainAnimInstance->WorldVelocity, MainAnimInstance->GroundFriction);

	if (!PredictedPivotLocation.IsZero())
	{
		const float PivotDistance = PredictedPivotLocation.Size2D();
		UAnimDistanceMatchingLibrary::DistanceMatchToTarget(SequenceEvaluator, PivotDistance, DistanceCurveName);
	}
	else
	{
		const float Displacement = MainAnimInstance->DisplacementSinceLastUpdate;
		UAnimDistanceMatchingLibrary::AdvanceTimeByDistanceMatching(Context, SequenceEvaluator, Displacement, DistanceCurveName);
	}
}

void UMyAnimLayers::SetupJumpAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);

	UAnimSequence* Sequence = MainAnimInstance->bJumpStartedFromMove ? Jump_FromMove : Jump_FromIdle;
	USequenceEvaluatorLibrary::SetSequence(SequenceEvaluator, Sequence);
	USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, 0.0f);

	CacheJumpHeightCurveData(Sequence);
}

void UMyAnimLayers::UpdateJumpAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);

	if (!bJumpHeightCurveValid)
	{
		USequenceEvaluatorLibrary::AdvanceTime(Context, SequenceEvaluator, 1.0f);
		return;
	}

	// 상승 중에는 원래 재생 속도를 유지하되 apex 프레임을 넘지 않는다.
	if (MainAnimInstance->bIsJumping)
	{
		USequenceEvaluatorLibrary::AdvanceTime(Context, SequenceEvaluator, 1.0f);

		const float AdvancedTime = USequenceEvaluatorLibrary::GetAccumulatedTime(SequenceEvaluator);
		if (AdvancedTime > JumpApexTime)
		{
			USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, JumpApexTime);
		}
		return;
	}

	// 하강 중에는 지면까지 남은 거리로 커브의 감소 구간을 역탐색한다
	if (MainAnimInstance->bIsFalling)
	{
		const UAnimSequenceBase* Sequence = USequenceEvaluatorLibrary::GetSequence(SequenceEvaluator);
		const float CurrentTime = USequenceEvaluatorLibrary::GetAccumulatedTime(SequenceEvaluator);
		const float TargetHeight = FMath::Max(MainAnimInstance->GroundDistance, 0.0f);

		const float MatchedTime = UMyAnimFunctionLibrary::FindTimeFromMonotonicCurve(
			Sequence,
			JumpHeightCurveName,
			TargetHeight,
			JumpApexSampleIndex,
			INDEX_NONE,
			MainAnimInstance->InitialJumpMaxHeight,
			JumpApexTime);

		// 지면 거리 노이즈로 시간이 역행하지 않게 하고, apex 이전으로도 돌아가지 않게 한다
		const float NewTime = FMath::Max3(CurrentTime, JumpApexTime, MatchedTime);

		// 기존 예상 지면보다 높은 지면을 탐지해 시간이 앞으로 건너뛰면 포즈가 튄다.
		// 시간은 그대로 적용하고 출력 포즈만 inertialization으로 연결한다
		const float ExplicitTimeDelta = NewTime - CurrentTime;
		if (MainAnimInstance->bGroundMovedCloser && ExplicitTimeDelta > UE_KINDA_SMALL_NUMBER)
		{
			UMyAnimFunctionLibrary::RequestInertialization(Context, 0.2f);
		}
		
		USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, NewTime);
		return;
	}

	// 착지 후
	USequenceEvaluatorLibrary::AdvanceTime(Context, SequenceEvaluator, 1.0f);
}

void UMyAnimLayers::UpdateFallLoopAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FSequencePlayerReference SequencePlayer;
	bool bSuccessed;
	USequencePlayerLibrary::ConvertToSequencePlayerPure(Node, SequencePlayer, bSuccessed);

	CachedFallLoopSequence = USequencePlayerLibrary::GetSequencePure(SequencePlayer);
	CachedFallLoopAccumulatedTime = USequencePlayerLibrary::GetAccumulatedTime(SequencePlayer);
}

void UMyAnimLayers::SetupFallLandAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	bGroundSnapDone = false;

	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);

	CacheFallLandDistanceCurveData();

	// 이미 Land 클립의 거리 범위 안이면 곧바로 착지 클립으로 시작한다
	if (bFallLandDistanceCurveValid && MainAnimInstance->GroundDistance <= FallLandStartDistance)
	{
		bUseFallLandDistanceMatching = true;
		UMyAnimFunctionLibrary::SetSequenceEvaluatorLooping(SequenceEvaluator, false);

		USequenceEvaluatorLibrary::SetSequence(SequenceEvaluator, Fall_Land);
		USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, 0.0f);
		return;
	}

	// 아직 범위 밖이면 FallLoop을 이어서 반복 재생한다
	bUseFallLandDistanceMatching = false;
	UMyAnimFunctionLibrary::SetSequenceEvaluatorLooping(SequenceEvaluator, true);
	if (CachedFallLoopSequence)
	{
		USequenceEvaluatorLibrary::SetSequence(SequenceEvaluator, CachedFallLoopSequence);
		USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, CachedFallLoopAccumulatedTime);
	}
	else
	{
		USequenceEvaluatorLibrary::SetSequence(SequenceEvaluator, Fall_Loop);
		USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, 0.0f);
	}
}

void UMyAnimLayers::UpdateFallLandAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);

	if (MainAnimInstance->bIsOnGround)
	{
		if (!bGroundSnapDone)
		{
			UAnimDistanceMatchingLibrary::DistanceMatchToTarget(SequenceEvaluator, 0.0f, JumpDistanceCurveName);
			bGroundSnapDone = true;
		}
		else
		{
			USequenceEvaluatorLibrary::AdvanceTime(Context, SequenceEvaluator, 1.0f);
		}
		return;
	}

	// Land 클립의 거리 범위로 처음 진입한 경우
	if (!bUseFallLandDistanceMatching &&
		bFallLandDistanceCurveValid &&
		MainAnimInstance->GroundDistance <= FallLandStartDistance)
	{
		bUseFallLandDistanceMatching = true;
		USequenceEvaluatorLibrary::SetSequenceWithInertialBlending(Context, SequenceEvaluator, Fall_Land, 0.2f);
	}

	if (bUseFallLandDistanceMatching)
	{
		UMyAnimFunctionLibrary::SetSequenceEvaluatorLooping(SequenceEvaluator, false);

		const float DistanceToMatch = FMath::Clamp(
			MainAnimInstance->GroundDistance, 0.0f, FallLandStartDistance);
		UAnimDistanceMatchingLibrary::DistanceMatchToTarget(
			SequenceEvaluator, DistanceToMatch, JumpDistanceCurveName);
		return;
	}

	// 아직 커브 범위 밖이라 FallLoop을 계속 반복 재생한다
	UMyAnimFunctionLibrary::SetSequenceEvaluatorLooping(SequenceEvaluator, true);
	USequenceEvaluatorLibrary::AdvanceTime(Context, SequenceEvaluator, 1.0f);
}

bool UMyAnimLayers::ShouldEnterTargetLockTurnInPlace() const
{
	if (!MainAnimInstance->CanUseTargetLockTurnInPlace())
	{
		return false;
	}

	const float TargetLockYawError = FMath::FindDeltaAngleDegrees(
		MainAnimInstance->WorldRotation.Yaw,
		MainAnimInstance->TargetLockContext.CharacterTargetRotation.Yaw);
	return FMath::Abs(TargetLockYawError) >= TargetLockTurnInPlaceStartAngle;
}

bool UMyAnimLayers::HasTargetLockTurnInPlaceTargetChanged() const
{
	return TargetLockTurnInPlaceTarget != MainAnimInstance->TargetLockContext.LockedTarget;
}

bool UMyAnimLayers::ShouldReenterTargetLockTurnInPlace() const
{
	if (!MainAnimInstance->CanUseTargetLockTurnInPlace())
	{
		return false;
	}

	const float TargetLockYawError = FMath::FindDeltaAngleDegrees(
		MainAnimInstance->WorldRotation.Yaw,
		MainAnimInstance->TargetLockContext.CharacterTargetRotation.Yaw);
	return FMath::Abs(TargetLockYawError) >= TargetLockTurnInPlaceStartAngle;
}

bool UMyAnimLayers::ShouldExitTargetLockTurnInPlaceRecovery() const
{
	if (!MainAnimInstance->CanUseTargetLockTurnInPlace())
	{
		return true;
	}

	const float TargetLockYawError = FMath::FindDeltaAngleDegrees(
		MainAnimInstance->WorldRotation.Yaw,
		MainAnimInstance->TargetLockContext.CharacterTargetRotation.Yaw);
	return FMath::Abs(TargetLockYawError) < TargetLockTurnInPlaceStartAngle;
}

UAnimSequence* UMyAnimLayers::GetDesiredTargetLockTurnInPlaceSequence(float SignedTurnAngle) const
{
	const bool bTurnRight = SignedTurnAngle > 0.0f;
	const bool bUse180 = FMath::Abs(SignedTurnAngle) >= TargetLockTurnInPlace180Threshold;

	if (bUse180)
	{
		UAnimSequence* Turn180Sequence = bTurnRight ? TargetLockTurnInPlaceRight180 : TargetLockTurnInPlaceLeft180;
		if (Turn180Sequence)
		{
			return Turn180Sequence;
		}
	}

	return bTurnRight ? TargetLockTurnInPlaceRight90 : TargetLockTurnInPlaceLeft90;
}

UAnimSequence* UMyAnimLayers::GetDesiredHangMoveSequence()
{
	return MainAnimInstance->LocalVelocity.Y >= 0.0f ? HangMoveRight : HangMoveLeft;
}

UAnimSequence* UMyAnimLayers::GetDesiredPivotSequence()
{
	const bool bTurnRight = MainAnimInstance->PivotAngle > 0.f;

	if (!MainAnimInstance->bIsRunning)
	{
		return bTurnRight ? WalkPivot_TurnRight : WalkPivot_TurnLeft;
	}
	else
	{
		return bTurnRight ? RunPivot_TurnRight : RunPivot_TurnLeft;
	}
}

bool UMyAnimLayers::CacheJumpHeightCurveData(const UAnimSequenceBase* Sequence)
{
	bJumpHeightCurveValid = false;
	JumpApexTime = 0.0f;
	JumpApexSampleIndex = INDEX_NONE;

	if (!Sequence)
	{
		return false;
	}

	FAnimCurveBufferAccess BufferAccess(Sequence, JumpHeightCurveName);
	if (!BufferAccess.IsValid())
	{
		return false;
	}

	const int32 NumSamples = BufferAccess.GetNumSamples();
	if (NumSamples < 2)
	{
		return false;
	}

	int32 MaxIndex = 0;
	float MaxValue = BufferAccess.GetValue(0);
	for (int32 Index = 1; Index < NumSamples; ++Index)
	{
		const float Value = BufferAccess.GetValue(Index);
		if (Value > MaxValue)
		{
			MaxValue = Value;
			MaxIndex = Index;
		}
	}

	if (MaxIndex >= NumSamples - 1 || BufferAccess.GetValue(NumSamples - 1) >= MaxValue)
	{
		return false;
	}

	JumpApexSampleIndex = MaxIndex;
	JumpApexTime = BufferAccess.GetTime(MaxIndex);
	bJumpHeightCurveValid = true;
	return true;
}

bool UMyAnimLayers::CacheFallLandDistanceCurveData()
{
	bFallLandDistanceCurveValid = false;
	FallLandStartDistance = 0.0f;

	// FAnimCurveBufferAccess 생성자가 시퀀스를 널 체크 없이 역참조한다
	if (!Fall_Land)
	{
		return false;
	}

	FAnimCurveBufferAccess BufferAccess(Fall_Land, JumpDistanceCurveName);
	if (!BufferAccess.IsValid())
	{
		return false;
	}

	const int32 NumSamples = BufferAccess.GetNumSamples();
	if (NumSamples < 2)
	{
		return false;
	}

	FallLandStartDistance = -BufferAccess.GetValue(0);
	bFallLandDistanceCurveValid = true;
	return true;
}
