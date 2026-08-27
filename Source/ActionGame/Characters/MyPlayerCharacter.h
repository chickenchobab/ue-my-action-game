// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Characters/MyCharacter.h"
#include "Logging/LogMacros.h"
#include "MyPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
struct FInputActionInstance;
class UMySkillSlotComponent;
enum class EWeaponSkillType : uint8;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AMyPlayerCharacter : public AMyCharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

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

public:

	/** Constructor */
	AMyPlayerCharacter(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	void Move(const FInputActionValue& Value);
	void StartSprint(const FInputActionValue& Value);
	void StopSprint(const FInputActionValue& Value);

	void Look(const FInputActionValue& Value);

	void QuitGame(const FInputActionValue& Value);

	// modified
	// Sweeps a stack of spheres forward to find a traversable obstacle in front of the character.
	void TraceTraversalObstacles();

public:

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:

	UPROPERTY()
	TObjectPtr<UMySkillSlotComponent> SkillSlotComponent;

private:

	FORCEINLINE void HandleWeaponSkillPressed(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType);
	FORCEINLINE void HandleWeaponSkillReleased(const FInputActionInstance& ActionInstance,  EWeaponSkillType SkillType);
};

