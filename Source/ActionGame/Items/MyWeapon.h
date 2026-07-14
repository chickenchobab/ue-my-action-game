// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "MyWeapon.generated.h"

class UMySkillData;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, AActor*, TArray<AActor*>&);

UENUM(BlueprintType)
enum class EWeaponSkillType : uint8
{
	Attack,
	SpecialAttack,
	WeaponUltimate,
	Max				UMETA(Hidden)
};

FORCEINLINE EWeaponSkillType& operator++(EWeaponSkillType& Value)
{
	Value = static_cast<EWeaponSkillType>(static_cast<uint8>(Value) + 1);

	return Value;
}

FORCEINLINE EWeaponSkillType operator++(EWeaponSkillType& Value, int)
{
	EWeaponSkillType OldValue = Value;

	Value = static_cast<EWeaponSkillType>(static_cast<uint8>(Value) + 1);

	return OldValue;
}

UCLASS()
class ACTIONGAME_API AMyWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	AMyWeapon();

	virtual void Equip(USceneComponent* NewParent, const FName& OverrideSocket = NAME_None);
	virtual void UnEquip();

	//FORCEINLINE void Register_OnAttackHit(FOnAttackHit::FDelegate&& Delegate);

	FORCEINLINE UMySkillData* GetSkillData(EWeaponSkillType SkillType) const { return SkillSet.FindRef(SkillType); }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	//virtual void Tick(float DeltaSeconds) override;

protected:

	UPROPERTY(VisibleDefaultsOnly)
	USceneComponent* GripRoot;

	UPROPERTY(VisibleDefaultsOnly)
	UStaticMeshComponent* WeaponMesh;

	FName DefaultAttachedSocket;

	UPROPERTY(EditDefaultsOnly)
	TMap<EWeaponSkillType, TObjectPtr<UMySkillData>> SkillSet;

private:

	UPROPERTY(VisibleDefaultsOnly)
	class USphereComponent* PickupCollision;

	bool bIsEquipped : 1;

	bool bIsAttacking : 1;

	FOnAttackHit OnAttackHit;

	TArray<FName> WeaponMeshSocketNames;
};
