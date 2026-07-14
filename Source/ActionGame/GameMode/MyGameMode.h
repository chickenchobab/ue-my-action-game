// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameModeBase.h"
#include "MyGameMode.generated.h"

class AMyWeapon;

UCLASS(abstract)
class AMyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMyGameMode();

	FORCEINLINE bool ShouldAutoEquipWeapon() const { return bAutoEquipWeapon; }

private:
	bool bAutoEquipWeapon : 1;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<AMyWeapon> DefaultWeapon;
};



