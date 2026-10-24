// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HitReceiver.generated.h"

USTRUCT(BlueprintType)
struct ACTIONGAME_API FHitReceiveContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> DamageCauser = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FVector ImpactPoint = FVector::ZeroVector;

	// 공격자 위치에서 충돌점으로 향하는 방향이다.
	UPROPERTY(BlueprintReadOnly)
	FVector ImpactDirection = FVector::ZeroVector;
};

UINTERFACE(MinimalAPI)
class UHitReceiver : public UInterface
{
	GENERATED_BODY()
};

class ACTIONGAME_API IHitReceiver
{
	GENERATED_BODY()

public:
	virtual void ReceiveHit(const FHitReceiveContext& HitContext) = 0;
};
