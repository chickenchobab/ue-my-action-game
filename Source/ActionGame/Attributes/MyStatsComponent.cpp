// Fill out your copyright notice in the Description page of Project Settings.


#include "Attributes/MyStatsComponent.h"

UMyStatsComponent::UMyStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMyStatsComponent::InitAttributes(UCurveTable* CurveTable, float Level)
{
}

void UMyStatsComponent::BeginPlay()
{
	Super::BeginPlay();
}

