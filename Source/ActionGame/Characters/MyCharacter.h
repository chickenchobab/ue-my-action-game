// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "Combat/HitReceiver.h"
#include "MyCharacter.generated.h"

class UMyCombatComponent;
class UMyHealthComponent;
class UMyStatsComponent;
class UMotionWarpingComponent;
class UAnimMontage;

UCLASS()
class ACTIONGAME_API AMyCharacter : public ACharacter, public IGameplayTagAssetInterface, public IHitReceiver
{
	GENERATED_BODY()

public:
	AMyCharacter(const FObjectInitializer& ObjectInitializer);

	FORCEINLINE UMyCombatComponent* GetCombatComponent() const { return CombatComponent; }
	FORCEINLINE UMyHealthComponent* GetHealthComponent() const { return HealthComponent; }
	FORCEINLINE UMyStatsComponent* GetStatsComponent() const { return StatsComponent; }
	FORCEINLINE UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	void AddStatusTag(const FGameplayTag& Tag, int32 Count=1);
	void RemoveStatusTag(const FGameplayTag& Tag, int32 Count=1);

	//~IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	//~End of IGameplayTagAssetInterface

	virtual void ReceiveHit(const FHitReceiveContext& HitContext) override;

	FORCEINLINE bool IsSprintActive() const { return bIsSprintActive; }

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

public:	
	virtual void Tick(float DeltaTime) override;

private:
	template <typename TComponent>
	void LoadAttributes(TComponent* Component, UCurveTable* CurveTable, float Level);

private:

	UPROPERTY(VisibleDefaultsOnly)
	TObjectPtr<UMyCombatComponent> CombatComponent;

	UPROPERTY(VisibleDefaultsOnly)
	TObjectPtr<UMyHealthComponent> HealthComponent;

	UPROPERTY(VisibleDefaultsOnly)
	TObjectPtr<UMyStatsComponent> StatsComponent;

	UPROPERTY(VisibleDefaultsOnly)
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UCurveTable> AttributeCurveTable;

	UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> HitReactionFrontMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> HitReactionBackMontage;

	// modified:
	// UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction")
	// TObjectPtr<UAnimMontage> HitReactionLeftMontage;
	//
	// UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction")
	// TObjectPtr<UAnimMontage> HitReactionRightMontage;

protected:

	bool bIsSprintActive = false;
	bool bIsSprintActiveLastMove = false;
	float CachedWalkSpeed;
};

template <typename TComponent>
void AMyCharacter::LoadAttributes(TComponent* Component, UCurveTable* CurveTable, float Level)
{
	static_assert(TIsDerivedFrom<TComponent, UActorComponent>::Value);
	Component->InitAttributes(CurveTable, Level);
}
