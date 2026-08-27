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
