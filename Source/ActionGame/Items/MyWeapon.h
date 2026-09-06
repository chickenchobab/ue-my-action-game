// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "MyWeapon.generated.h"

class UMySkillData;
class USkillInstance_Attack;

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

	void InitWeaponForAttack(USkillInstance_Attack* Skill, const UAnimMontage* SkillMontage);
	void OnAttackBegin();
	void OnAttackEnd();

	FORCEINLINE UMySkillData* GetSkillData(EWeaponSkillType SkillType) const { return SkillSet.FindRef(SkillType); }
	FORCEINLINE const TMap<EWeaponSkillType, TObjectPtr<UMySkillData>>& GetSkillSet() const { return SkillSet; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	virtual void HitCheck(USkinnedMeshComponent* MeshComp, float DeltaTime, bool bNeedsValidRootMotion);

protected:

	UPROPERTY(VisibleDefaultsOnly)
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	FName DefaultAttachedSocket;

	UPROPERTY(EditDefaultsOnly)
	TMap<EWeaponSkillType, TObjectPtr<UMySkillData>> SkillSet;
	
	TWeakObjectPtr<USkillInstance_Attack> CurrentActiveSkill;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAnimInstance> AnimLayerClass;

private:

	UPROPERTY(VisibleDefaultsOnly)
	TObjectPtr<class USphereComponent> PickupCollision;

	bool bIsEquipped : 1;

private:

	FDelegateHandle HitCheckDelegateHandle;

	bool bIsAttacking : 1;

	TArray<FName> WeaponSockets;

	// 공격 시작부터 종료까지의 hit 확인을 위해 공통적으로 참조되는 context
	struct FHitCheckContext
	{
		const UAnimMontage* Montage = nullptr;
		const UAnimSequence* AnimSequence = nullptr;
		const FAnimMontageInstance* MontageInstance = nullptr;

		USkeletalMeshComponent* OwnerMesh = nullptr;
		FTransform OwnerMeshTransformLastFrame;
		FName GripBoneName = NAME_None;
		int32 GripBoneIndex = INDEX_NONE;

		FBoneContainer BoneContainer;
		TArray<FTransform> WeaponSocketToGripBone;

		TArray<AActor*> HitActorsThisFrame;
		TSet<AActor*> SkillHandledActors;

		FORCEINLINE bool IsValid() const;
		FORCEINLINE void Reset();
	} HitCheckContext;

	struct FWeaponHitQuery
	{
		FVector Tip_Old, Tip_New;
		FVector Base_Old, Base_New;
		FBox Box;
	};

	struct FSocketSamples
	{
		FName SocketName;
		TArray<FVector> Locations;
	};

private:

	FORCEINLINE void SampleSocketPositions(TArray<FSocketSamples>& OutSamples);

	FORCEINLINE bool IntersectQuadWithCapsule(const FWeaponHitQuery& Query, const FVector& CapsuleBase, const FVector& CapsuleTop, float Radius);
	FORCEINLINE bool IntersectTriangleWithCapsule(const FVector& V0, const FVector& V1, const FVector& V2, const FVector& P0, const FVector& P1, float R);

	FORCEINLINE void InitHitCheckContext(const UAnimMontage* Montage);
	FORCEINLINE void BuildBoneContainer(USkeletalMesh* SkelMesh);

	FORCEINLINE FBox GetBoxFromHitQuery(const FWeaponHitQuery& Query);
	FORCEINLINE void GetCandidatesByBoxOverlap(TArray<FOverlapResult>& OutCandidates, const FBox& InBox, FCollisionQueryParams& QueryParams, FCollisionObjectQueryParams& ObjectQueryParams);
	
};
