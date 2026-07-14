// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "MyCombatComponent.generated.h"

enum class EWeaponSkillType : uint8;
class AMyWeapon;
class UMySkillData;
class UInputAction;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ACTIONGAME_API UMyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UMyCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void InitAttributes(UCurveTable* CurveTable, float Level);

	const TSoftClassPtr<AMyWeapon>& GetDefaultWeaponClass() const { return DefaultWeaponClass; }
	FORCEINLINE AMyWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	void EquipWeapon(AMyWeapon* Weapon);
	void EquipDefaultWeapon();

	FORCEINLINE float GetCurrentMana() const { return CurrentMana; }

	FORCEINLINE const UInputAction* GetWeaponSkillInputAction(EWeaponSkillType SkillType) const { return WeaponSkillInputActions.FindRef(SkillType); }

	FORCEINLINE bool GetSkillEnabled() const { return bSkillEnabled; }
	FORCEINLINE void SetSkillEnabled(bool bEnabled) { bSkillEnabled = bEnabled; }

	void OnWeaponSkillPressed(EWeaponSkillType SkillType);
	void OnWeaponSkillReleased(EWeaponSkillType SkillType);

private:

	UPROPERTY(EditAnywhere)
	TSoftClassPtr<AMyWeapon> DefaultWeaponClass;

	UPROPERTY(EditAnywhere)
	TMap<EWeaponSkillType, TObjectPtr<UInputAction>> WeaponSkillInputActions;

protected:

	UPROPERTY()
	TObjectPtr<AMyWeapon> CurrentWeapon;

	bool bSkillEnabled;

	UPROPERTY()
	float MaxMana;
	UPROPERTY()
	float CurrentMana;
	UPROPERTY()
	float ManaRegenRate;
};
