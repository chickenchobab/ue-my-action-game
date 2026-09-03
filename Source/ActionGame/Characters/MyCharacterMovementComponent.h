// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.generated.h"

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

protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Movement: Rotation")
	float FallingRotationRateYaw = 300.0f;

private:

	FRotator RotationRateBeforeFalling = FRotator::ZeroRotator;
};
