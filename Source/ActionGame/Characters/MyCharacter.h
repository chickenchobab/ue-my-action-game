// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "MyCharacter.generated.h"

class UMyCombatComponent;
class UMyHealthComponent;
class UMyStatsComponent;

UCLASS()
class ACTIONGAME_API AMyCharacter : public ACharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AMyCharacter();

	FORCEINLINE UMyCombatComponent* GetCombatComponent() const { return CombatComponent; }
	FORCEINLINE UMyHealthComponent* GetHealthComponent() const { return HealthComponent; }
	FORCEINLINE UMyStatsComponent* GetStatsComponent() const { return StatsComponent; }

	void AddStatusTag(const FGameplayTag& Tag, int32 Count=1);
	void RemoveStatusTag(const FGameplayTag& Tag, int32 Count=1);

	//~IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	//~End of IGameplayTagAssetInterface

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

	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UCurveTable> AttributeCurveTable;
};

template <typename TComponent>
void AMyCharacter::LoadAttributes(TComponent* Component, UCurveTable* CurveTable, float Level)
{
	static_assert(TIsDerivedFrom<TComponent, UActorComponent>::Value);
	Component->InitAttributes(CurveTable, Level);
}