// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Animation/AnimInstance.h"
#include "MyAnimLayers.generated.h"

class UMyAnimInstance;
struct FAnimNodeReference;
struct FSequenceEvaluatorReference;
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

private:

	FORCEINLINE UAnimSequence* GetDesiredCycleSequence();
	FORCEINLINE UAnimSequence* GetDesiredPivotSequence();

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

	UPROPERTY(EditDefaultsOnly, meta = (Category = "AnimSet|Pivot"))
	TObjectPtr<UAnimSequence> WalkPivot_TurnRight;
	UPROPERTY(EditDefaultsOnly, meta = (Category = "AnimSet|Pivot"))
	TObjectPtr<UAnimSequence> WalkPivot_TurnLeft;
	UPROPERTY(EditDefaultsOnly, meta = (Category = "AnimSet|Pivot"))
	TObjectPtr<UAnimSequence> RunPivot_TurnRight;
	UPROPERTY(EditDefaultsOnly, meta = (Category = "AnimSet|Pivot"))
	TObjectPtr<UAnimSequence> RunPivot_TurnLeft;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly, meta = (Category = "Pivot"))
	bool bShouldRepivot = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly, meta = (Category = "Pivot"))
	float PivotStateWeight;

	FVector PivotStartingAcceleration;
};
