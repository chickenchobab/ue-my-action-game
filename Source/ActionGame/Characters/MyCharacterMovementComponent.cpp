// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/MyCharacterMovementComponent.h"
#include "Characters/MyCharacter.h"

UMyCharacterMovementComponent::UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float UMyCharacterMovementComponent::GetMaxAcceleration() const
{
	return MaxWalkSpeed;
}

void UMyCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	const bool bEnteredFalling = PreviousMovementMode != MOVE_Falling && MovementMode == MOVE_Falling;
	const bool bExitedFalling = PreviousMovementMode == MOVE_Falling && MovementMode != MOVE_Falling;

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
}
