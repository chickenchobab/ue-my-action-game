// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/MyCharacterMovementComponent.h"
#include "Characters/MyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

UMyCharacterMovementComponent::UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float UMyCharacterMovementComponent::GetMaxAcceleration() const
{
	if (IsClimbing())
	{
		return MaxClimbAcceleration;
	}

	return MaxWalkSpeed;
}

float UMyCharacterMovementComponent::GetMaxSpeed() const
{
	return IsClimbing() ? MaxClimbSpeed : Super::GetMaxSpeed();
}

void UMyCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (IsClimbing())
	{
		PhysClimbing(DeltaTime, Iterations);
		return;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

void UMyCharacterMovementComponent::PhysClimbing(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME || CharacterOwner == nullptr || UpdatedComponent == nullptr)
	{
		return;
	}

	FHitResult NewWallHit;
	FHitResult NewLedgeHit;
	if (!FindClimbingWall(NewWallHit) || !FindClimbingLedge(NewWallHit, NewLedgeHit))
	{
		// TODO: 직육면체 건물의 모서리에 위치하여 난간이 캐릭터 앞으로 이어지는 경우 wall이 없을 수 있고 
		// 난간이 캐릭터 뒤로 이어지는 경우는 penetration이 발생할 수 있다.
		StopMovementImmediately();
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	FVector WallNormal = NewWallHit.ImpactNormal;
	WallNormal.Z = 0.0f;
	WallNormal.Normalize();
	NewWallHit.ImpactNormal = WallNormal;
	ClimbingWallHit = NewWallHit;
	ClimbingLedgeHit = NewLedgeHit;
	ClimbingLedgeHeight = NewLedgeHit.ImpactPoint.Z;

	const FVector WallForward = -WallNormal;
	const FVector WallRight = FVector::CrossProduct(FVector::UpVector, WallForward).GetSafeNormal();

	RestorePreAdditiveRootMotionVelocity();

	const bool bHasClimbingAnimRootMotion = HasAnimRootMotion();
	if (!bHasClimbingAnimRootMotion && !CurrentRootMotion.HasOverrideVelocity())
	{	
		// TODO: 현재는 난간을 따라 수평으로 이동하지만 지형 지물에 따른 수직 이동 구현
		Acceleration = WallRight * FVector::DotProduct(Acceleration, WallRight);
		Velocity = WallRight * FVector::DotProduct(Velocity, WallRight);
		CalcVelocity(DeltaTime, ClimbingFriction, false, ClimbingBrakingDeceleration);
	}

	ApplyRootMotionToVelocity(DeltaTime);

	if (!bHasClimbingAnimRootMotion)
	{
		Velocity = WallRight * FVector::DotProduct(Velocity, WallRight);
	}

	Iterations++;
	bJustTeleported = false;

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FVector TargetLocation = NewWallHit.ImpactPoint + WallNormal * (CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() + ClimbingWallOffset);
	TargetLocation.Z = NewLedgeHit.ImpactPoint.Z - ClimbingLedgeToCharacterZ;

	const FVector SnapDelta = bHasClimbingAnimRootMotion
		? FVector::ZeroVector
		: (TargetLocation - OldLocation).GetClampedToMaxSize(ClimbingWallSnapSpeed * DeltaTime);
	const FVector Adjusted = Velocity * DeltaTime + SnapDelta;
	const FQuat DesiredRotation = bHasClimbingAnimRootMotion
		? UpdatedComponent->GetComponentQuat()
		: WallForward.Rotation().Quaternion();

	FHitResult MoveHit(1.0f);
	SafeMoveUpdatedComponent(Adjusted, DesiredRotation, true, MoveHit);

	if (MoveHit.Time < 1.0f)
	{
		HandleImpact(MoveHit, DeltaTime, Adjusted);
		SlideAlongSurface(Adjusted, 1.0f - MoveHit.Time, MoveHit.Normal, MoveHit, true);
	}

	if (bHasClimbingAnimRootMotion)
	{
		ClimbingLedgeToCharacterZ = ClimbingLedgeHeight - UpdatedComponent->GetComponentLocation().Z;
	}

	if (!bJustTeleported && !bHasClimbingAnimRootMotion && !CurrentRootMotion.HasOverrideVelocity())
	{
		const FVector ActualVelocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
		Velocity = WallRight * FVector::DotProduct(ActualVelocity, WallRight);
	}
}

void UMyCharacterMovementComponent::StartClimbing(const FHitResult& WallHit, const FHitResult& LedgeHit)
{
	FVector WallNormal = WallHit.ImpactNormal;
	WallNormal.Z = 0.0f;
	if (!WallNormal.Normalize() || !LedgeHit.bBlockingHit || UpdatedComponent == nullptr)
	{
		return;
	}

	ClimbingWallHit = WallHit;
	ClimbingWallHit.ImpactNormal = WallNormal;
	ClimbingLedgeHit = LedgeHit;
	ClimbingLedgeHeight = LedgeHit.ImpactPoint.Z;
	ClimbingLedgeToCharacterZ = ClimbingLedgeHeight - UpdatedComponent->GetComponentLocation().Z;

	StopMovementImmediately();
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EMyCustomMovementMode::Climbing));
}

bool UMyCharacterMovementComponent::IsClimbing() const
{
	return MovementMode == MOVE_Custom
		&& CustomMovementMode == static_cast<uint8>(EMyCustomMovementMode::Climbing);
}

FVector UMyCharacterMovementComponent::GetClimbingRightDirection() const
{
	FVector WallNormal = ClimbingWallHit.ImpactNormal;
	WallNormal.Z = 0.0f;
	WallNormal.Normalize();

	const FVector WallForward = -WallNormal;
	return FVector::CrossProduct(FVector::UpVector, WallForward).GetSafeNormal();
}

bool UMyCharacterMovementComponent::FindClimbingWall(FHitResult& OutWallHit) const
{
	FVector WallNormal = ClimbingWallHit.ImpactNormal;
	WallNormal.Z = 0.0f;
	if (!WallNormal.Normalize())
	{
		WallNormal = -UpdatedComponent->GetForwardVector();
	}

	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (Capsule == nullptr)
	{
		return false;
	}

	const FVector TraceStart = UpdatedComponent->GetComponentLocation();
	const FVector TraceEnd = TraceStart - WallNormal * (Capsule->GetScaledCapsuleRadius() + ClimbingWallCheckDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbingWallTrace), false, CharacterOwner);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	const bool bHit = GetWorld()->SweepSingleByObjectType(
		OutWallHit,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(1.0f, ClimbingWallProbeRadius)),
		QueryParams);

	if (!bHit || OutWallHit.bStartPenetrating || FMath::Abs(OutWallHit.ImpactNormal.Z) > MaxClimbingWallNormalZ)
	{
		return false;
	}

	FVector NewWallNormal = OutWallHit.ImpactNormal;
	NewWallNormal.Z = 0.0f;
	return NewWallNormal.Normalize();
}

bool UMyCharacterMovementComponent::FindClimbingLedge(const FHitResult& WallHit, FHitResult& OutLedgeHit) const
{
	FVector WallNormal = WallHit.ImpactNormal;
	WallNormal.Z = 0.0f;
	if (!WallNormal.Normalize())
	{
		return false;
	}

	const FVector WallForward = -WallNormal;
	const FVector ProbeXY = WallHit.ImpactPoint + WallForward * ClimbingLedgeProbeDepth;
	const FVector TraceStart(ProbeXY.X, ProbeXY.Y, ClimbingLedgeHeight + ClimbingLedgeVerticalTolerance);
	const FVector TraceEnd(ProbeXY.X, ProbeXY.Y, ClimbingLedgeHeight - ClimbingLedgeVerticalTolerance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ClimbingLedgeTrace), false, CharacterOwner);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	const bool bHit = GetWorld()->LineTraceSingleByObjectType(
		OutLedgeHit,
		TraceStart,
		TraceEnd,
		ObjectQueryParams,
		QueryParams);

	return bHit && !OutLedgeHit.bStartPenetrating && OutLedgeHit.ImpactNormal.Z >= MinClimbingTopNormalZ;
}

void UMyCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	const bool bEnteredFalling = PreviousMovementMode != MOVE_Falling && MovementMode == MOVE_Falling;
	const bool bExitedFalling = PreviousMovementMode == MOVE_Falling && MovementMode != MOVE_Falling;
	const bool bExitedClimbing = PreviousMovementMode == MOVE_Custom && PreviousCustomMode == static_cast<uint8>(EMyCustomMovementMode::Climbing) && !IsClimbing();

	if (bEnteredFalling)
	{
		RotationRateBeforeFalling = RotationRate;
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (bEnteredFalling)
	{
		RotationRate = FRotator(0.0f, FallingRotationRateYaw, 0.0f);
	}
	else if (bExitedFalling)
	{
		RotationRate = RotationRateBeforeFalling;
	}

	if (bExitedClimbing)
	{
		ClimbingWallHit = FHitResult();
		ClimbingLedgeHit = FHitResult();
		ClimbingLedgeHeight = 0.0f;
		ClimbingLedgeToCharacterZ = 0.0f;
	}
}
