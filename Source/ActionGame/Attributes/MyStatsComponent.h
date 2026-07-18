// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "MyGameplayTags.h"
#include "GameplayTagAssetInterface.h"
#include "MyStatsComponent.generated.h"

USTRUCT(BlueprintType)
struct FStatusTagCountContainer
{
	GENERATED_BODY()

	void UpdateTag(const FGameplayTag& Tag, int32 CountDelta)
	{
		int32& TagCountRef = StatusTagCountMap.FindOrAdd(Tag);
		TagCountRef = FMath::Max(TagCountRef + CountDelta, 0);

		if (CountDelta > 0)
		{
			StatusTags.AddTag(Tag);
		}
		else if (TagCountRef == 0)
		{
			StatusTags.RemoveTag(Tag);
		}
	}

	const FGameplayTagContainer& GetStatusTags() const { return StatusTags; }

	bool HasMatchingStatusTag(FGameplayTag TagToCheck) const
	{
		return StatusTagCountMap.FindRef(TagToCheck) > 0;
	}

	bool HasAllMatchingStatusTags(const FGameplayTagContainer& TagContainer) const
	{
		if (TagContainer.Num() == 0)
		{
			return true;
		}

		for (const FGameplayTag& Tag : TagContainer)
		{
			if (StatusTagCountMap.FindRef(Tag) <= 0)
			{
				return false;
			}
		}
		return true;
	}

	bool HasAnyMatchingStatusTags(const FGameplayTagContainer& TagContainer) const
	{
		if (TagContainer.Num() == 0)
		{
			return true;
		}

		for (const FGameplayTag& Tag : TagContainer)
		{
			if (StatusTagCountMap.FindRef(Tag) > 0)
			{
				return true;
			}
		}
		return false;
	}

private:
	UPROPERTY()
	FGameplayTagContainer StatusTags;
	UPROPERTY()
	TMap<FGameplayTag, int32> StatusTagCountMap;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ACTIONGAME_API UMyStatsComponent : public UActorComponent, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:	
	UMyStatsComponent();

	void InitAttributes(UCurveTable* CurveTable, float Level);

	FORCEINLINE bool CanMove() const;

	void AddStatusTag(const FGameplayTag& Tag, int32 Count=1)
	{
		if (Count > 0)
		{
			StatusTagCountContainer.UpdateTag(Tag, Count);
		}
	}

	void RemoveStatusTag(const FGameplayTag& Tag, int32 Count=1)
	{
		if (Count > 0)
		{
			StatusTagCountContainer.UpdateTag(Tag, -Count);
		}
	}

	//~IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override
	{
		TagContainer.Reset();
		TagContainer.AppendTags(StatusTagCountContainer.GetStatusTags());
	}
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override
	{
		return StatusTagCountContainer.HasMatchingStatusTag(TagToCheck);
	}
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override
	{
		return StatusTagCountContainer.HasAllMatchingStatusTags(TagContainer);
	}
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override
	{
		return StatusTagCountContainer.HasAnyMatchingStatusTags(TagContainer);
	}
	//~End of IGameplayTagAssetInterface

protected:
	virtual void BeginPlay() override;

protected:

	float AttackDamage;
	float Armor;
	float MoveSpeed;

	UPROPERTY()
	FStatusTagCountContainer StatusTagCountContainer;
};

bool UMyStatsComponent::CanMove() const
{
	static FGameplayTagContainer Tags;
	if (Tags.IsEmpty())
	{
		Tags.AddTag(MyGameplayTags::Status_Channeling);
	}
	return !HasAnyMatchingGameplayTags(Tags);
}