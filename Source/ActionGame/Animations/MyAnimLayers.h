// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Animation/AnimInstance.h"
#include "MyAnimLayers.generated.h"

class UMyAnimInstance;
struct FAnimNodeReference;
struct FAnimUpdateContext;
class UAnimSequence;
class UAnimSequenceBase;

UCLASS()
class ACTIONGAME_API UMyAnimLayers : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe))
	UMyAnimInstance* GetMainAnimInstance() const { return MainAnimInstance; }

protected:

	UMyAnimInstance* MainAnimInstance;

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void SetupCycleState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateCycleAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	// Pivot

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void SetupPivotAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdatePivotAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	// Jump

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void SetupJumpAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateJumpAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	// Fall

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateFallLoopAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void SetupFallLandAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateFallLandAnim(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

private:

	FORCEINLINE UAnimSequence* GetDesiredCycleSequence();
	FORCEINLINE UAnimSequence* GetDesiredPivotSequence();

	bool bJumpHeightCurveValid = false;
	float JumpApexTime = 0.0f;
	int32 JumpApexSampleIndex = INDEX_NONE;

	bool CacheJumpHeightCurveData(const UAnimSequenceBase* Sequence);

	// FallLoop 시퀀스 플레이어의 재생 상태 캐시다
	// FallLand evaluator가 이 값을 이어받아 발 위상이 끊기지 않게 한다
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CachedFallLoopSequence;

	float CachedFallLoopAccumulatedTime = 0.0f;

	bool bUseFallLandDistanceMatching = false;
	bool bFallLandDistanceCurveValid = false;
	float FallLandStartDistance = 0.0f;
	bool bGroundSnapDone = false;

	bool CacheFallLandDistanceCurveData();

protected:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Idle"))
	TObjectPtr<UAnimSequence> IdleSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|WalkStart"))
	TObjectPtr<UAnimSequence> WalkStartLeft180;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|WalkStart"))
	TObjectPtr<UAnimSequence> WalkStartLeft90;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|WalkStart"))
	TObjectPtr<UAnimSequence> WalkStartForward;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|WalkStart"))
	TObjectPtr<UAnimSequence> WalkStartRight90;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|WalkStart"))
	TObjectPtr<UAnimSequence> WalkStartRight180;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|RunStart"))
	TObjectPtr<UAnimSequence> RunStartLeft180;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|RunStart"))
	TObjectPtr<UAnimSequence> RunStartLeft90;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|RunStart"))
	TObjectPtr<UAnimSequence> RunStartForward;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|RunStart"))
	TObjectPtr<UAnimSequence> RunStartRight90;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|RunStart"))
	TObjectPtr<UAnimSequence> RunStartRight180;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Cycle"))
	TObjectPtr<UAnimSequence> WalkForward;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Cycle"))
	TObjectPtr<UAnimSequence> WalkBackward;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Cycle"))
	TObjectPtr<UAnimSequence> WalkLeft;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Cycle"))
	TObjectPtr<UAnimSequence> WalkRight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Cycle"))
	TObjectPtr<UAnimSequence> Run;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Stop"))
	TObjectPtr<UAnimSequence> WalkStop;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Stop"))
	TObjectPtr<UAnimSequence> RunStop;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Jump"))
	TObjectPtr<UAnimSequence> Jump_FromMove;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Jump"))
	TObjectPtr<UAnimSequence> Jump_FromIdle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Fall"))
	TObjectPtr<UAnimSequence> Fall_Loop;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Category = "AnimSet|Fall"))
	TObjectPtr<UAnimSequence> Fall_Land;

	UPROPERTY(EditDefaultsOnly, meta = (Category = "AnimSet|Pivot"))
	TObjectPtr<UAnimSequence> WalkPivot_TurnRight;
	UPROPERTY(EditDefaultsOnly, meta = (Category = "AnimSet|Pivot"))
	TObjectPtr<UAnimSequence> WalkPivot_TurnLeft;
	UPROPERTY(EditDefaultsOnly, meta = (Category = "AnimSet|Pivot"))
	TObjectPtr<UAnimSequence> RunPivot_TurnRight;
	UPROPERTY(EditDefaultsOnly, meta = (Category = "AnimSet|Pivot"))
	TObjectPtr<UAnimSequence> RunPivot_TurnLeft;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly, meta = (Category = "Pivot"))
	float PivotStateWeight;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly, meta = (Category = "Fall"))
	bool bShouldLoopFallLandEvaluator = true;

	FVector PivotStartingAcceleration;
};
