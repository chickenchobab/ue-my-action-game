// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyPlayerCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ActionGame.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Combat/MyCombatComponent.h"
#include "Skills/MySkillData.h"
#include "Items/MyWeapon.h"
#include "Attributes/MyStatsComponent.h"

AMyPlayerCharacter::AMyPlayerCharacter()
{
	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AMyPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyPlayerCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMyPlayerCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyPlayerCharacter::Look);

		// Quit game
		EnhancedInputComponent->BindAction(QuitGameAction, ETriggerEvent::Triggered, this, &AMyPlayerCharacter::QuitGame);

		// Weapon switching input is determined by the character's currently equipped weapon, not using IMC.
		for (EWeaponSkillType SkillType = static_cast<EWeaponSkillType>(0); SkillType < EWeaponSkillType::Max; ++SkillType)
		{
			if (const UInputAction* SkillInputAction = GetCombatComponent()->GetWeaponSkillInputAction(SkillType))
			{
				EnhancedInputComponent->BindAction(SkillInputAction, ETriggerEvent::Triggered, this, &ThisClass::HandleWeaponSkillPressed, SkillType);
				EnhancedInputComponent->BindAction(SkillInputAction, ETriggerEvent::Completed, this, &ThisClass::HandleWeaponSkillReleased, SkillType);
			}
		}
	}
}

void AMyPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (GetStatsComponent() != nullptr)
	{
		if (!GetStatsComponent()->CanMove())
		{
			return;
		}
	}

	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AMyPlayerCharacter::Look(const FInputActionValue& Value)
{
	if (GetStatsComponent() != nullptr)
	{
		if (!GetStatsComponent()->CanMove())
		{
			return;
		}
	}

	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AMyPlayerCharacter::QuitGame(const FInputActionValue& Value)
{
	UKismetSystemLibrary::QuitGame(this, Cast<APlayerController>(GetController()), EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

void AMyPlayerCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AMyPlayerCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMyPlayerCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AMyPlayerCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AMyPlayerCharacter::HandleWeaponSkillPressed(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedPlayerInput* PlayerInput = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
		{
			FInputActionValue Value = PlayerInput->GetActionValue(MoveAction);
			FVector2D MovementVector = Value.Get<FVector2D>();
			if (UMyCombatComponent* CombatComp = GetCombatComponent())
			{
				CombatComp->OnWeaponSkillPressed(ActionInstance, SkillType, MovementVector);
			}
		}
	}
}

void AMyPlayerCharacter::HandleWeaponSkillReleased(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedPlayerInput* PlayerInput = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
		{
			FInputActionValue Value = PlayerInput->GetActionValue(MoveAction);
			FVector2D MovementVector = Value.Get<FVector2D>();
			if (UMyCombatComponent* CombatComp = GetCombatComponent())
			{
				CombatComp->OnWeaponSkillReleased(ActionInstance, SkillType, MovementVector);
			}
		}
	}
}
