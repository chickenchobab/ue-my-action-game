#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SequenceEvaluatorLibrary.h"
#include "MyAnimFunctionLibrary.generated.h"

class UAnimSequenceBase;

UCLASS()
class ACTIONGAME_API UMyAnimFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "Animation|Curves", meta = (BlueprintThreadSafe))
	static float FindTimeFromMonotonicCurve(
		const UAnimSequenceBase* Sequence,
		FName CurveName,
		float TargetValue,
		int32 SearchStartSampleIndex,
		int32 SearchEndSampleIndex,
		float CurveValueScale,
		float MinimumTime);

	UFUNCTION(BlueprintCallable, Category = "Animation|Sequence Evaluator", meta = (BlueprintThreadSafe))
	static FSequenceEvaluatorReference SetSequenceEvaluatorLooping(
		const FSequenceEvaluatorReference& SequenceEvaluator,
		bool bShouldLoop);

	UFUNCTION(BlueprintCallable, Category = "Animation|Inertialization", meta = (BlueprintThreadSafe))
	static bool RequestInertialization(
		const FAnimUpdateContext& Context,
		float Duration);
};
