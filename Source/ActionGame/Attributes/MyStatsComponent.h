// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "MyStatsComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ACTIONGAME_API UMyStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UMyStatsComponent();

	void InitAttributes(UCurveTable* CurveTable, float Level);

protected:
	virtual void BeginPlay() override;

protected:

	/*float AttackDamage;
	float Armor;
	float MoveSpeed;*/
};
