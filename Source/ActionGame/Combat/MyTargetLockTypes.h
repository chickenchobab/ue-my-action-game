#pragma once

#include "CoreMinimal.h"
#include "MyTargetLockTypes.generated.h"

USTRUCT(BlueprintType)
struct FTargetLockContext
{
	GENERATED_BODY()

	UPROPERTY(Transient, BlueprintReadOnly)
	TWeakObjectPtr<AActor> LockedTarget;

	UPROPERTY(Transient, BlueprintReadOnly)
	FRotator CharacterTargetRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly)
	FRotator CameraTargetRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly)
	bool bIsTargetLocked = false;
};
