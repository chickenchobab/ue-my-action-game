// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Characters/MyCharacter.h"
#include "Logging/LogMacros.h"
#include "MyPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
struct FInputActionInstance;

class UAnimMontage;
enum class EWeaponSkillType : uint8;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(abstract)
class AMyPlayerCharacter : public AMyCharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* EvadeAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* QuitGameAction;


	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> OnGroundMappingContext;

	UPROPERTY(EditAnywhere, Category = "Traversal")
	float TraversalReachDistance = 200.f;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	int32 ForwardTraceCount = 6;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	float TraversalTraceHighestZ = 130.f;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	float TraversalTraceLowestZ = -70.f;

	UPROPERTY(EditAnywhere, Category = "Traversal")
	float MaxHangHeightOffset = 400.f;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	float MinHangHeightOffset = 150.f;

	UPROPERTY(EditAnywhere, Category = "Traversal")
	float VaultForwardHandOffset = 10.f;
	UPROPERTY(EditAnywhere, Category = "Traversal", meta = (ClampMin = "1.0"))
	float VaultTraceDepthStep = 60.f;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	float MaxVaultDepth = 200.f;
	UPROPERTY(EditAnywhere, Category = "Traversal", meta = (ClampMin = "0.0"))
	float VaultClearanceHeight = 100.f;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	float MaxVaultLandingDrop = 300.f;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	TObjectPtr<UAnimMontage> VaultMontage_ToGround;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	TObjectPtr<UAnimMontage> VaultMontage_ToAir;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	TArray<TObjectPtr<UAnimMontage>> EvadeSideMontages;
	UPROPERTY(EditAnywhere, Category = "Traversal")
	TObjectPtr<UAnimMontage> RollMontage;

public:

	AMyPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	void OnTraversalWarpEnded(const FName& WarpTargetName, bool bTraversalSucceeded);

protected:

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

protected:

	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);
	void StartSprint(const FInputActionValue& Value);
	void StopSprint(const FInputActionValue& Value);

	void Look(const FInputActionValue& Value);

	void QuitGame(const FInputActionValue& Value);

	bool TraceTraversalObstacles(FHitResult& OutHit, const FVector& TraceDirection);


	bool TryVault(const FHitResult& ForwardHit, const FVector& VaultDirection);


	void BuildTraversalQueryParams(FCollisionQueryParams& OutQuery, FCollisionObjectQueryParams& OutObject) const;

	bool PlayEvadeMontage(const FVector& EvadeDirection);

	bool DoTraverse(UAnimMontage* Montage, const FVector& FacingDirection, const TArray<AActor*>& ObstacleActors, FName TraversalWarpTarget, float TraversalTopZ);

	FORCEINLINE void OnVaultTraversalEnded(bool bTraversalSucceeded);

	void OnTraversalMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 TraversalId, FName TraversalWarpTarget);

public:

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void Evade();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:

	FORCEINLINE void HandleWeaponSkillPressed(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType);
	FORCEINLINE void HandleWeaponSkillReleased(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType);

	// ���� ContactPoint�� �굵�� �ϴ� root transform�� ���Ѵ�.
	bool GetHandAlignedWarpTransform(UAnimMontage* Montage, const FName& WarpTargetName, const FVector& ContactPoint, const FRotator& ApproachRotation, FTransform& OutTarget) const;

	FORCEINLINE bool IsCapsuleBlockedAtLocation(const FVector& CapsuleBaseLocation) const;

	void SetupTraversalCamera(float TraversalTopZ);
	void RestoreTraversalCamera();

	FORCEINLINE void RestoreTraversalPhysics();

	FORCEINLINE FVector GetWorldMovementDirection(const FVector2D& MovementVector) const;

private:
	FVector2D CachedMovementVector = FVector2D::ZeroVector;

	uint64 ActiveTraversalId = 0;
	TSet<uint64> PendingTraversalWarpIds;

	TArray<TWeakObjectPtr<AActor>> TraversalIgnoredActors;

	FVector CachedCameraBoomTargetOffset = FVector::ZeroVector;
};

