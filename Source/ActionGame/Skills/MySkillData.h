// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "MySkillData.generated.h"

UENUM(BlueprintType)
enum class ESkillInstancingPolicy : uint8
{
	PerAvatar,
	PerExecution,
	Max
};

class UMyCombatComponent;
class UMySkillInstance;
class UInputAction;
struct FInputActionInstance;
class UEnhancedPlayerInput;

USTRUCT(BlueprintType)
struct FComboLinkOption
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UMySkillData> LinkedSkill;

	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer RequiredAvatarTags;

	UPROPERTY(EditDefaultsOnly)
	float LinkWindowDuration;

	UPROPERTY(EditDefaultsOnly)
	TArray<FKey> RequiredHeldKeys;
};

/**
 *
 */
UCLASS(BlueprintType)
class ACTIONGAME_API UMySkillData : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly)
	FName SkillID;

	UPROPERTY(EditDefaultsOnly)
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UTexture2D> Icon = nullptr;

	// TODO: Anim Layer

	UPROPERTY(EditDefaultsOnly)
	TArray<TObjectPtr<UAnimMontage>> SkillMontages;

	UPROPERTY(EditDefaultsOnly)
	float Cooldown = 1.0f;

	UPROPERTY(EditDefaultsOnly)
	float Cost = 0.0f;

	UPROPERTY(EditDefaultsOnly)
	ESkillInstancingPolicy InstancingPolicy = ESkillInstancingPolicy::PerAvatar;

	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer RequiredTags;
	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer BlockingTags;

	UPROPERTY(EditDefaultsOnly)
	bool bComboOnly = false;

	UPROPERTY(EditDefaultsOnly)
	TArray<FComboLinkOption> LinkOptions;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UMySkillInstance> InstanceClassOverride;

public:

	TSubclassOf<UMySkillInstance> GetInstanceClass() const;
	virtual bool IsInstanceClassCompatible(TSubclassOf<UMySkillInstance> InstanceClass) const;

protected:

	virtual TSubclassOf<UMySkillInstance> GetDefaultInstanceClass() const;
};
