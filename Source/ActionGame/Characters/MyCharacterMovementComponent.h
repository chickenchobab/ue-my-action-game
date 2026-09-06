// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EMyCustomMovementMode : uint8
{
	None UMETA(Hidden),
	Climbing UMETA(DisplayName = "Climbing")
};

/**
 * 
 */
UCLASS()
class ACTIONGAME_API UMyCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxSpeed() const override;

	void StartClimbing(const FHitResult& WallHit, const FHitResult& LedgeHit);
	bool IsClimbing() const;
	FVector GetClimbingRightDirection() const;
	const FHitResult& GetClimbingWallHit() const { return ClimbingWallHit; }
	const FHitResult& GetClimbingLedgeHit() const { return ClimbingLedgeHit; }

protected:

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Rotation")
	float FallingRotationRateYaw = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float MaxClimbSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float MaxClimbAcceleration = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float ClimbingBrakingDeceleration = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float ClimbingFriction = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float ClimbingWallCheckDistance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float ClimbingWallProbeRadius = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float ClimbingWallOffset = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float ClimbingWallSnapSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float ClimbingLedgeProbeDepth = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0"))
	float ClimbingLedgeVerticalTolerance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxClimbingWallNormalZ = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinClimbingTopNormalZ = 0.94f;

private:

	void PhysClimbing(float DeltaTime, int32 Iterations);
	bool FindClimbingWall(FHitResult& OutWallHit) const;
	bool FindClimbingLedge(const FHitResult& WallHit, FHitResult& OutLedgeHit) const;

	FRotator RotationRateBeforeFalling = FRotator::ZeroRotator;
	FHitResult ClimbingWallHit;
	FHitResult ClimbingLedgeHit;
	float ClimbingLedgeHeight = 0.0f;
	float ClimbingLedgeToCharacterZ = 0.0f;
};
