// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "MyCombatComponent.generated.h"

enum class EWeaponSkillType : uint8;
class AMyWeapon;
class UMySkillData;
class UMySkillInstance;
class UInputAction;
class UEnhancedPlayerInput;
struct FInputActionInstance;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ACTIONGAME_API UMyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UMyCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void InitAttributes(UCurveTable* CurveTable, float Level);

	FORCEINLINE const TSoftClassPtr<AMyWeapon>& GetDefaultWeaponClass() const { return DefaultWeaponClass; }
	FORCEINLINE AMyWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	void EquipWeapon(AMyWeapon* Weapon);
	void EquipDefaultWeapon();
	void UnequipCurrentWeapon();

	FORCEINLINE float GetCurrentMana() const { return CurrentMana; }

	FORCEINLINE const UInputAction* GetWeaponSkillInputAction(EWeaponSkillType SkillType) const { return WeaponSkillInputActions.FindRef(SkillType); }

	FORCEINLINE bool GetSkillEnabled() const { return bSkillEnabled; }
	FORCEINLINE void SetSkillEnabled(bool bEnabled) { bSkillEnabled = bEnabled; }

	void OnWeaponSkillPressed(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType, const FVector2D& MovementVector);
	void OnWeaponSkillReleased(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType, const FVector2D& MovementVector);

	UMySkillInstance* FindSkillInstance(UMySkillData* SkillData) const;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void GrantSkillsFromWeapon(AMyWeapon* Weapon);
	void RevokeSkillsFromWeapon(AMyWeapon* Weapon);
	void RevokeAllSkills();

private:

	UPROPERTY(EditAnywhere)
	TSoftClassPtr<AMyWeapon> DefaultWeaponClass;

	UPROPERTY(EditAnywhere)
	TMap<EWeaponSkillType, TObjectPtr<UInputAction>> WeaponSkillInputActions;

protected:

	UPROPERTY(Transient)
	TObjectPtr<AMyWeapon> CurrentWeapon;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UMySkillData>, TObjectPtr<UMySkillInstance>> SkillInstances;

	bool bSkillEnabled;

	UPROPERTY()
	float MaxMana;
	UPROPERTY()
	float CurrentMana;
	UPROPERTY()
	float ManaRegenRate;
};
