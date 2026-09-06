// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "MySkillInstance.generated.h"

class UMySkillData;
class UMyCombatComponent;
class UAnimInstance;
class UAnimMontage;
struct FInputActionInstance;

/**
 * 스킬 데이터(정의)의 실행 인스턴스.
 * 아바타 하나당 하나씩 생성되며 런타임 상태를 소유한다. Outer는 소유자의 UMyCombatComponent다.
 */
UCLASS()
class ACTIONGAME_API UMySkillInstance : public UObject
{
	GENERATED_BODY()

public:
	virtual bool Initialize(UMySkillData* InSkillData, AActor* InAvatarActor, AActor* InSourceItem);
	virtual void Deinitialize();

	FORCEINLINE const UMySkillData* GetSkillData() const { return SkillData; }
	virtual void HandleSkillReleased(const FInputActionInstance& Instance, const FVector2D& MovementVector);

	void TryExecuteSkill(const FInputActionInstance& Instance, const FVector2D& MovementVector);
	void CancelSkill();

	FORCEINLINE bool IsSkillActive() const { return ActiveCount > 0; }
	FORCEINLINE bool IsInitialized() const { return bInitialized; }

	// TODO: for combo skill
	void EnableExecution(float Duration);

protected:

	virtual bool CanExecuteSkill();
	virtual void CommitCostsAndCooldown();
	virtual bool ExecuteSkill(const FInputActionInstance& Instance, const FVector2D& MovementVector) { return false; }
	virtual void OnSkillEnd(bool bCanceled);

	bool PlaySkillMontage();
	virtual void OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted) {}
	virtual void OnSkillMontageBlendingOutStarted(UAnimMontage* Montage, bool bInterrupted) {}

	void ExpireExecutionGrant();
	void ClearExecutionGrantTimers();
	void StopActiveMontage();

	UMyCombatComponent* GetCombatComponentFromAvatarActor() const;
	UAnimInstance* GetAnimInstanceFromAvatarActor() const;

protected:

	UPROPERTY(Transient)
	TObjectPtr<UMySkillData> SkillData;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> AvatarActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> SourceItem;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimMontage> ActiveMontage;

	TArray<FTimerHandle> ExecutionGrantTimerHandles;

	int32 CurrentMontageIndex = 0;

	uint32 ExecutionGrantCount = 1;

	uint32 ActiveCount = 0;

	bool bInitialized = false;
};
