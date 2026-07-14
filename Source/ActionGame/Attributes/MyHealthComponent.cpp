// Fill out your copyright notice in the Description page of Project Settings.


#include "Attributes/MyHealthComponent.h"

UMyHealthComponent::UMyHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMyHealthComponent::InitAttributes(UCurveTable* CurveTable, float Level)
{
	if (const FRealCurve* Curve = CurveTable->FindCurve(FName("MaxHealth"), TEXT("")))
	{
		MaxHealth = Curve->Eval(Level);
	}

	if (const FRealCurve* Curve = CurveTable->FindCurve(FName("HealthRegenRate"), TEXT("")))
	{
		HealthRegenRate = Curve->Eval(Level);
	}

	CurrentHealth = MaxHealth;
}

void UMyHealthComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMyHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMyHealthComponent::CallOrRegister_OnDeath(FOnDeath::FDelegate&& Delegate)
{
	if (IsDead())
	{
		Delegate.Execute(LastDamageCauser);
	}
	else
	{
		OnDeath.Add(MoveTemp(Delegate));
	}
}

void UMyHealthComponent::CallOrRegister_OnHealthChanged(FOnHealthChanged::FDelegate&& Delegate)
{
	Delegate.Execute(LastDamageCauser, CurrentHealth, MaxHealth);
	OnHealthChanged.Add(MoveTemp(Delegate));
}

