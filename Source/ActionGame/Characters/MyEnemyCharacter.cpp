// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/MyEnemyCharacter.h"
#include "Player/MyPlayerState.h"
#include "Engine/AssetManager.h"
#include "Items/MyWeapon.h"
#include "Combat/MyCombatComponent.h"

AMyEnemyCharacter::AMyEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AMyEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();	
}

void AMyEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
