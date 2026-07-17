// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MySkillData.generated.h"

UENUM(BlueprintType)
enum class ESkillInstancingPolicy : uint8
{
	PerAvatar,
	PerExecution,
	Max
};

class UMyCombatComponent;
class UInputAction;

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

public:

	virtual void InitWithAvatar(AActor* NewAvatarActor);
	virtual void InitWithItem(AActor* OwningItem) {}
	virtual void HandleSkillReleased();

	void TryExecuteSkill();
	
	FORCEINLINE bool IsSkillActive() const { return ActiveCount > 0; }

	// TODO: for combo skill
	FORCEINLINE	void EnableExecution(float Duration);

protected:

	virtual bool CanExecuteSkill();
	virtual void CommitCostsAndCooldown();
	virtual void ExecuteSkill() {}
	virtual void OnSkillEnd();

	void PlaySkillMontage();
	virtual void OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted) {}
	virtual void OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted) {}

	UMyCombatComponent* GetCombatComponentFromAvatarActor() const;
	UAnimInstance* GetAnimInstanceFromAvatarActor() const;

protected:

	UPROPERTY()
	TWeakObjectPtr<AActor> AvatarActor;

	int32 CurrentMontageIndex = 0;

private:

	uint32 ExecutionGrantCount = 1;

	uint32 ActiveCount = 0;
};
