// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/MyAnimLayers.h"
#include "Animations/MyAnimInstance.h"
#include "Animations/Libraries/MyAnimFunctionLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationStateMachineLibrary.h"
#include "SequenceEvaluatorLibrary.h"
#include "AnimExecutionContextLibrary.h"
#include "SequencePlayerLibrary.h"
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
	if (!MainAnimInstance) // ������ ũ���� ����
	{
		return;
	}

	bShouldRepivot = MainAnimInstance->LocomotionStateLastUpdate == ELocomotionState::Pivot && 
		MainAnimInstance->bAccelDotReversed &&
		(PivotStartingAcceleration | MainAnimInstance->WorldAcceleration.GetSafeNormal2D()) < -0.5f;
}

void UMyAnimLayers::SetupCycleState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	MainAnimInstance->bNeedsStartState = true;
}

void UMyAnimLayers::UpdateCycleAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
}

void UMyAnimLayers::SetupPivotAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FSequenceEvaluatorReference SequenceEvaluator;
	bool bSuccessed;
	USequenceEvaluatorLibrary::ConvertToSequenceEvaluatorPure(Node, SequenceEvaluator, bSuccessed);

	MainAnimInstance->SetupPivotValues();
	PivotStartingAcceleration = MainAnimInstance->WorldAcceleration.GetSafeNormal2D();

	UAnimSequence* Sequence = GetDesiredPivotSequence();
	float BlendTime = bShouldRepivot ? 0.f : 0.2f;
	USequenceEvaluatorLibrary::SetSequence(SequenceEvaluator, Sequence);

	MainAnimInstance->PlayRate = MainAnimInstance->ComputeLocomotionPlayRate(MainAnimInstance->PivotInitialSpeed);
	USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, 0.0f);

	bShouldRepivot = false;
}

void UMyAnimLayers::UpdatePivotAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	// Inertial transition�̶� pivot state �ϳ��� ������Ʈ�ȴ�
	PivotStateWeight = UAnimExecutionContextLibrary::GetCurrentWeight(Context);

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

	// ��� �߿��� ���� ��� �ӵ��� �����ϵ� apex �������� ���� �ʴ´�.
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

	// �ϰ� �߿��� ������� ���� �Ÿ��� Ŀ���� ���� ������ ��Ž���Ѵ�
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

		// ���� �Ÿ� ������� �ð��� �������� �ʰ� �ϰ�, apex �������ε� ���ư��� �ʰ� �Ѵ�
		const float NewTime = FMath::Max3(CurrentTime, JumpApexTime, MatchedTime);

		// ���� ���� ���麸�� ���� ������ Ž���� �ð��� ������ �ǳʶٸ� ��� Ƥ��.
		// �ð��� �״�� �����ϰ� ��� ��� inertialization���� �����Ѵ�
		const float ExplicitTimeDelta = NewTime - CurrentTime;
		if (MainAnimInstance->bGroundMovedCloser && ExplicitTimeDelta > UE_KINDA_SMALL_NUMBER)
		{
			UMyAnimFunctionLibrary::RequestInertialization(Context, 0.2f);
		}

		USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, NewTime);
		return;
	}

	// ���� ��
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

	// �̹� Land Ŭ���� �Ÿ� ���� ���̸� ��ٷ� ���� Ŭ������ �����Ѵ�
	if (bFallLandDistanceCurveValid && MainAnimInstance->GroundDistance <= FallLandStartDistance)
	{
		bUseFallLandDistanceMatching = true;
		bShouldLoopFallLandEvaluator = false;
		UMyAnimFunctionLibrary::SetSequenceEvaluatorLooping(SequenceEvaluator, false);

		USequenceEvaluatorLibrary::SetSequence(SequenceEvaluator, Fall_Land);
		USequenceEvaluatorLibrary::SetExplicitTime(SequenceEvaluator, 0.0f);
		return;
	}

	// ���� ���� ���̸� FallLoop�� �̾ �ݺ� ����Ѵ�
	bUseFallLandDistanceMatching = false;
	bShouldLoopFallLandEvaluator = true;
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

	// Land Ŭ���� �Ÿ� ������ ó�� ������ ���
	if (!bUseFallLandDistanceMatching &&
		bFallLandDistanceCurveValid &&
		MainAnimInstance->GroundDistance <= FallLandStartDistance)
	{
		bUseFallLandDistanceMatching = true;
		USequenceEvaluatorLibrary::SetSequenceWithInertialBlending(Context, SequenceEvaluator, Fall_Land, 0.2f);
	}

	if (bUseFallLandDistanceMatching)
	{
		bShouldLoopFallLandEvaluator = false;
		UMyAnimFunctionLibrary::SetSequenceEvaluatorLooping(SequenceEvaluator, false);

		const float DistanceToMatch = FMath::Clamp(
			MainAnimInstance->GroundDistance, 0.0f, FallLandStartDistance);
		UAnimDistanceMatchingLibrary::DistanceMatchToTarget(
			SequenceEvaluator, DistanceToMatch, JumpDistanceCurveName);
		return;
	}

	// ���� Ŀ�� ���� ���̶� FallLoop�� ��� �ݺ� ����Ѵ�
	bShouldLoopFallLandEvaluator = true;
	UMyAnimFunctionLibrary::SetSequenceEvaluatorLooping(SequenceEvaluator, true);
	USequenceEvaluatorLibrary::AdvanceTime(Context, SequenceEvaluator, 1.0f);
}

UAnimSequence* UMyAnimLayers::GetDesiredCycleSequence()
{
	if (!MainAnimInstance->bIsRunning)
	{
		return WalkForward;
	}
	else
	{
		return Run;
	}
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

	// FAnimCurveBufferAccess �����ڰ� �������� �� üũ ���� �������Ѵ�
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