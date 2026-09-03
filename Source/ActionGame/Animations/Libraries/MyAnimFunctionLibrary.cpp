#include "Animations/Libraries/MyAnimFunctionLibrary.h"
#include "Animation/AnimCurveCompressionCodec_UniformIndexable.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "Animation/AnimInertializationRequest.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimTrace.h"

float UMyAnimFunctionLibrary::FindTimeFromMonotonicCurve(
	const UAnimSequenceBase* Sequence,
	FName CurveName,
	float TargetValue,
	int32 SearchStartSampleIndex,
	int32 SearchEndSampleIndex,
	float CurveValueScale,
	float MinimumTime)
{
	if (!Sequence)
	{
		return MinimumTime;
	}

	FAnimCurveBufferAccess BufferAccess(Sequence, CurveName);
	if (!BufferAccess.IsValid())
	{
		return MinimumTime;
	}

	const int32 NumSamples = BufferAccess.GetNumSamples();
	const int32 LastSampleIndex = SearchEndSampleIndex == INDEX_NONE
		? NumSamples - 1
		: SearchEndSampleIndex;

	if (NumSamples < 2 ||
		SearchStartSampleIndex < 0 ||
		LastSampleIndex >= NumSamples ||
		SearchStartSampleIndex >= LastSampleIndex)
	{
		return MinimumTime;
	}

	const float StartValue = BufferAccess.GetValue(SearchStartSampleIndex) * CurveValueScale;
	const float EndValue = BufferAccess.GetValue(LastSampleIndex) * CurveValueScale;
	const bool bIsIncreasing = StartValue <= EndValue;
	TargetValue = FMath::Clamp(TargetValue, FMath::Min(StartValue, EndValue), FMath::Max(StartValue, EndValue));

	for (int32 Index = SearchStartSampleIndex; Index < LastSampleIndex; ++Index)
	{
		const float ValueA = BufferAccess.GetValue(Index) * CurveValueScale;
		const float ValueB = BufferAccess.GetValue(Index + 1) * CurveValueScale;
		const bool bContainsTarget = bIsIncreasing
			? TargetValue >= ValueA && TargetValue <= ValueB
			: TargetValue <= ValueA && TargetValue >= ValueB;

		if (!bContainsTarget)
		{
			continue;
		}

		const float Diff = ValueB - ValueA;
		const float Alpha = FMath::IsNearlyZero(Diff) ? 0.0f : (TargetValue - ValueA) / Diff;
		const float MatchedTime = FMath::Lerp(
			BufferAccess.GetTime(Index),
			BufferAccess.GetTime(Index + 1),
			Alpha);
		return FMath::Max(MatchedTime, MinimumTime);
	}

	return FMath::Max(BufferAccess.GetTime(LastSampleIndex), MinimumTime);
}

FSequenceEvaluatorReference UMyAnimFunctionLibrary::SetSequenceEvaluatorLooping(
	const FSequenceEvaluatorReference& SequenceEvaluator,
	bool bShouldLoop)
{
	SequenceEvaluator.CallAnimNodeFunction<FAnimNode_SequenceEvaluator>(
		TEXT("SetSequenceEvaluatorLooping"),
		[bShouldLoop](FAnimNode_SequenceEvaluator& InSequenceEvaluator)
		{
			if (!InSequenceEvaluator.SetShouldLoop(bShouldLoop))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("Could not set looping on sequence evaluator, value is not dynamic. Set it as Always Dynamic."));
			}
		});

	return SequenceEvaluator;
}

bool UMyAnimFunctionLibrary::RequestInertialization(
	const FAnimUpdateContext& Context,
	float Duration)
{
	if (Duration <= 0.0f)
	{
		return false;
	}

	const FAnimationUpdateContext* AnimationUpdateContext = Context.GetContext();
	if (!AnimationUpdateContext)
	{
		return false;
	}

	UE::Anim::IInertializationRequester* InertializationRequester =
		AnimationUpdateContext->GetMessage<UE::Anim::IInertializationRequester>();
	if (!InertializationRequester)
	{
		return false;
	}

	FInertializationRequest Request;
	Request.Duration = Duration;
#if ANIM_TRACE_ENABLED
	Request.NodeId = AnimationUpdateContext->GetCurrentNodeId();
	Request.AnimInstance = AnimationUpdateContext->AnimInstanceProxy->GetAnimInstanceObject();
#endif

	InertializationRequester->RequestInertialization(Request);
	return true;
}
