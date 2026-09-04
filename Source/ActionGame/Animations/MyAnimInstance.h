// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Animation/AnimInstance.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/CachedAnimData.h"
#include "MyAnimInstance.generated.h"

class AMyCharacter;
class UMyCharacterMovementComponent;
struct FAnimInstanceProxy;
struct FAnimUpdateContext;
struct FAnimNodeReference;

UENUM(BlueprintType)
enum class ECardinalDirection : uint8
{
	Forward,
	Backward,
	Left,
	Right
};

UENUM(BlueprintType)
enum class ELocomotionState : uint8
{
	Idle,
	Run,
	Walk,
	Pivot
};

/**
 * 
 */
UCLASS()
class ACTIONGAME_API UMyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	UMyAnimInstance();

protected:

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativePostEvaluateAnimation() override;

protected:

	AMyCharacter* Character;

	UMyCharacterMovementComponent* MovementComponent;

public:

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	FVector WorldVelocity;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float Speed = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float MaxSpeed;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bHasVelocity = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bWasMovingLastUpdate = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float PlayRate;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float CardinalDirectionDeadZone = 10.0f;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	FVector WorldAcceleration;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bHasAcceleration = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float GroundFriction;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float VelocityAccelDot = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float ForwardAccelDot = 0.0f;
	bool bAccelDotReversed = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bHasOppositeAccel = false;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bIsFalling = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bIsOnGround = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bJumpStartedFromMove = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bIsJumping = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float TimeToJumpApex = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float GravityZ = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float MaxJumpHeight = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float TimeToLand = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float GroundDistance = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float InitialJumpMaxHeight = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float ExpectedGroundDistance = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bGroundMovedFarther = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bGroundMovedCloser = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bFallLandEndNotified = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float VerticalDisplacementSinceLastUpdate = 0.0f;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	FVector Location;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float DisplacementSinceLastUpdate;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float DisplacementSpeed;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	FRotator WorldRotation;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	FVector Forward;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float YawDelta;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	ELocomotionState LocomotionState = ELocomotionState::Idle;
	// Pivot -> Walk/Run 이탈 시 회전 타겟을 리셋하지 않기 위해 필요하다
	ELocomotionState LocomotionStateLastUpdate = ELocomotionState::Idle;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float WalkStartAngle;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float RunStartAngle;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bNeedsStartState = true;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	FVector InputVector;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	FRotator PrimaryTargetRotation;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	FRotator SecondaryTargetRotation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float PrimaryRotationInterpSpeed = 1000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float SecondaryRotationInterpSpeed = 10.0f;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bIsRunning = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bHasRootMotion = false;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float PivotAngle = 0.0f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float PivotInitialSpeed = 0.f;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bPivotExitNotified = false;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bShouldRepivot = false;

	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float IdleStateWeight;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float WalkStateWeight;
	UPROPERTY(Transient, VisibleDefaultsOnly, BlueprintReadOnly)
	float RunStateWeight;

protected:

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void SetupIdleState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateIdleState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateWalkState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateRunState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void SetupPivotState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdatePivotState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void SetupJumpState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void SetupFallLandState(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

public:

	FORCEINLINE void NotifyPivotEnd() { bPivotExitNotified = true; }
	FORCEINLINE void NotifyFallLandEnd() { bFallLandEndNotified = true; }

	float ComputeLocomotionPlayRate(float CharacterSpeed) const;

	void SetupPivotValues();

private:

	bool bIsFirstUpdate;

	FVector LocationLastUpdate;
	FRotator RotationLastUpdate;

	float VelocityAccelDotLastUpdate = 0.0f;

	FORCEINLINE void UpdateLocationData(float DeltaSeconds);
	FORCEINLINE void UpdateRotationData(float DeltaSeconds);
	FORCEINLINE void UpdateVelocityData(float DeltaSeconds);
	FORCEINLINE void UpdateAccelerationData(float DeltaSeconds);
	FORCEINLINE void UpdateJumpFallData(float DeltaSeconds);
	FORCEINLINE void DetermineLocomotionState();
	FORCEINLINE void SetupMoveState();
	FORCEINLINE void UpdateLocomotionValues();
	FORCEINLINE void UpdateCharacterRotation();

	FORCEINLINE bool CanEnterPivot() const;
	FORCEINLINE bool ShouldExitPivot() const;

	FORCEINLINE bool IsMovementWithinThresholds(float MinCurrentSpeed, float MinMaxSpeed, float MinInputAcceleration) const;
	FORCEINLINE ECardinalDirection SelectCardinalDirectionFromAngle(float Angle, float DeadZone, ECardinalDirection CurrentDirection, bool bUseCurrentDirection);
	
	FCachedAnimStateData LocomotionStateData;
	FCachedAnimStateData WalkStateData;
	FCachedAnimStateData RunStateData;
	FCachedAnimStateData PivotStateData;
};
