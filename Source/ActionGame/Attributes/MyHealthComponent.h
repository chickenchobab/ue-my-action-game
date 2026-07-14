// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "MyHealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeath, AActor*);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, AActor*, float, float);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ACTIONGAME_API UMyHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	UMyHealthComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void InitAttributes(UCurveTable* CurveTable, float Level);

	void CallOrRegister_OnDeath(FOnDeath::FDelegate&& Delegate);
	void CallOrRegister_OnHealthChanged(FOnHealthChanged::FDelegate&& Delegate);

	FORCEINLINE bool IsDead() const { return CurrentHealth <= 0.0f; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	float MaxHealth;
	UPROPERTY()
	float CurrentHealth;
	UPROPERTY()
	float HealthRegenRate;

	FOnDeath OnDeath;
	FOnHealthChanged OnHealthChanged;

private:

	UPROPERTY()
	TObjectPtr<AActor> LastDamageCauser = nullptr;
};
