// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Characters/MyCharacter.h"
#include "MyEnemyCharacter.generated.h"

class UMyAbilitySystemComponent;

UCLASS()
class ACTIONGAME_API AMyEnemyCharacter : public AMyCharacter
{
	GENERATED_BODY()

public:
	AMyEnemyCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	
public:	
	virtual void Tick(float DeltaTime) override;
};
