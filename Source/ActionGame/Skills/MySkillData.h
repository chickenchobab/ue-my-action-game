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
class ACTIONGAME_API UMySkillData : public UPrimaryDataAsset
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
	TObjectPtr<UAnimMontage> SkillMontage = nullptr;

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

	virtual void InitWithAvatar(AActor* WeaponOwner);
	virtual void HandleSkillReleased();

	void TryExecuteSkill(AActor* Instigator);
	
	FORCEINLINE bool IsSkillActive() const { return bIsActive; }

	// TODO: for combo skill
	FORCEINLINE	void EnableExecution(float Duration);


protected:

	virtual bool CanExecuteSkill(AActor* Instigator);
	virtual void CommitCostsAndCooldown();
	virtual void ExecuteSkill(AActor* Instigator) {}
	virtual void OnSkillEnd();

	UMyCombatComponent* GetCombatComponentFromInstigator(AActor* Instigator) const;
	UAnimInstance* GetAnimInstanceFromInstigator(AActor* Instigator) const;

protected:

	UPROPERTY()
	TWeakObjectPtr<AActor> AvatarActor;

private:

	int32 ExecutionGrantCount = 1;

	bool bIsActive = false;
};
