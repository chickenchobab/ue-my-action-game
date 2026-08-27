// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/MyAnimLayers.h"
#include "Animations/MyAnimInstance.h"
#include "AnimationStateMachineLibrary.h"
#include "SequenceEvaluatorLibrary.h"
#include "AnimExecutionContextLibrary.h"
#include "SequencePlayerLibrary.h"
#include "AnimCharacterMovementLibrary.h"
#include "AnimDistanceMatchingLibrary.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"

static FName DistanceCurveName(TEXT("MoveData_Distance"));

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
	// Inertial transition이라 pivot state 하나만 업데이트된다
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
