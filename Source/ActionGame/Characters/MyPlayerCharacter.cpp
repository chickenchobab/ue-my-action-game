// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/MyPlayerCharacter.h"
#include "Characters/MyCharacterMovementComponent.h"
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
#include "Attributes/MyGameplayTags.h"
#include "Player/MyPlayerController.h"
#include "Animation/AnimInstance.h"
#include "MotionWarpingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "BonePose.h"

// modified: traversal 시각화 함수와 호출부를 제거했다.
// Move = Traversal
static const FName HangJumpWarpTarget(TEXT("Hang_Jump"));
static const FName MantleJumpWarpTarget(TEXT("Mantle_Jump"));
static const FName MantleMoveWarpTarget(TEXT("Mantle_Move"));
static const FName VaultJumpWarpTarget(TEXT("Vault_Jump"));
static const FName VaultMoveWarpTarget(TEXT("Vault_Move"));

static const FName LeftHandBoneName(TEXT("hand_l"));
static const FName RightHandBoneName(TEXT("hand_r"));

AMyPlayerCharacter::AMyPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetUsingAbsoluteRotation(true); // Animation 로직에서 캐릭터 회전을 수행하므로

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	CachedWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
}

void AMyPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (CameraBoom != nullptr)
	{
		DefaultCameraBoomTargetOffset = CameraBoom->TargetOffset;
	}
}

void AMyPlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	const bool bFellFromClimbing = PrevMovementMode == MOVE_Custom
		&& PreviousCustomMode == static_cast<uint8>(EMyCustomMovementMode::Climbing)
		&& GetCharacterMovement()->MovementMode == MOVE_Falling;

	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (ClimbMappingContext != nullptr)
			{
				Subsystem->RemoveMappingContext(ClimbMappingContext);
			}
			if (OnGroundMappingContext != nullptr)
			{
				Subsystem->RemoveMappingContext(OnGroundMappingContext);
			}

			const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
			if (MovementComponent->MovementMode == MOVE_Walking || MovementComponent->MovementMode == MOVE_NavWalking)
			{
				if (OnGroundMappingContext != nullptr)
				{
					Subsystem->AddMappingContext(OnGroundMappingContext, 0);
				}
			}
			else if (MovementComponent->MovementMode == MOVE_Custom
				&& MovementComponent->CustomMovementMode == static_cast<uint8>(EMyCustomMovementMode::Climbing))
			{
				if (ClimbMappingContext != nullptr)
				{
					Subsystem->AddMappingContext(ClimbMappingContext, 0);
				}
			}
		}
	}

	if (!bFellFromClimbing)
	{
		return;
	}

	CachedHangForwardHit = FHitResult();
	CachedHangTopHit = FHitResult();
	RestoreTraversalCamera();
}

void AMyPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::DoJumpEnd);

		EnhancedInputComponent->BindAction(EvadeAction, ETriggerEvent::Started, this, &ThisClass::Evade);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyPlayerCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AMyPlayerCharacter::StopMove);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMyPlayerCharacter::Look);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AMyPlayerCharacter::StartSprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AMyPlayerCharacter::StopSprint);
		EnhancedInputComponent->BindAction(ClimbAction, ETriggerEvent::Triggered, this, &ThisClass::Climb);

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
	CachedMovementVector = Value.Get<FVector2D>();

	if (GetStatsComponent() != nullptr)
	{
		if (!GetStatsComponent()->CanMove())
		{
			return;
		}
	}

	DoMove(CachedMovementVector.X, CachedMovementVector.Y);
}

void AMyPlayerCharacter::StopMove(const FInputActionValue& Value)
{
	CachedMovementVector = FVector2D::ZeroVector;
}

void AMyPlayerCharacter::StartSprint(const FInputActionValue& Value)
{
	bIsSprintActive = true;
}

void AMyPlayerCharacter::StopSprint(const FInputActionValue& Value)
{
	bIsSprintActive = false;
}

void AMyPlayerCharacter::Climb(const FInputActionValue& Value)
{
	if (GetStatsComponent())
	{
		if (!GetStatsComponent()->CanMove())
		{
			return;
		}
	}

	UMyCharacterMovementComponent* MovementComponent = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
	if (!MovementComponent || !MovementComponent->IsClimbing())
	{
		return;
	}

	// TODO: 현재는 난간에서의 수평 이동만 구현함
	// 벽에 손잡이들을 배치하고 수직 이동도 구현할 예정
	const FVector2D ClimbInput = Value.Get<FVector2D>();
	constexpr float MantleInputThreshold = 0.5f;
	if (ClimbInput.Y > MantleInputThreshold)
	{
		TryMantle();
		return;
	}

	if (!FMath::IsNearlyZero(ClimbInput.X))
	{
		AddMovementInput(MovementComponent->GetClimbingRightDirection(), ClimbInput.X);
	}
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
		const FVector2D MovementVector(Right, Forward);
		const FVector WorldMovementDirection = GetWorldMovementDirection(MovementVector);
		if (!WorldMovementDirection.IsNearlyZero())
		{
			AddMovementInput(WorldMovementDirection, MovementVector.Size());
		}

		if (bIsSprintActiveLastMove != bIsSprintActive)
		{
			if (bIsSprintActive)
			{
				CachedWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
				GetCharacterMovement()->MaxWalkSpeed = 600.f;
			}
			else
			{
				GetCharacterMovement()->MaxWalkSpeed = CachedWalkSpeed;
			}
		}

		bIsSprintActiveLastMove = bIsSprintActive;
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

void AMyPlayerCharacter::HandleWeaponSkillPressed(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType)
{
	if (UMyCombatComponent* CombatComp = GetCombatComponent())
	{
		CombatComp->OnWeaponSkillPressed(ActionInstance, SkillType, CachedMovementVector);
	}
}

void AMyPlayerCharacter::HandleWeaponSkillReleased(const FInputActionInstance& ActionInstance, EWeaponSkillType SkillType)
{
	if (UMyCombatComponent* CombatComp = GetCombatComponent())
	{
		CombatComp->OnWeaponSkillReleased(ActionInstance, SkillType, CachedMovementVector);
	}
}

void AMyPlayerCharacter::DoJumpStart()
{
	if (UMyStatsComponent* StatsComp = GetStatsComponent())
	{
		if (!StatsComp->CanMove())
		{
			return;
		}
	}

	FHitResult ForwardHit;
	if (!CachedMovementVector.IsNearlyZero() && TraceTraversalObstacles(ForwardHit, GetActorForwardVector()))
	{
		if (TryHang(ForwardHit))
		{
			return;
		}
	}

	Jump();
}

void AMyPlayerCharacter::DoJumpEnd()
{
	StopJumping();
}

void AMyPlayerCharacter::Evade()
{
	if (UMyStatsComponent* StatsComp = GetStatsComponent())
	{
		if (!StatsComp->CanMove())
		{
			return;
		}
	}

	FVector EvadeDirection = GetWorldMovementDirection(CachedMovementVector);
	if (EvadeDirection.IsNearlyZero())
	{
		EvadeDirection = GetActorForwardVector();
	}

	FHitResult ForwardHit;
	if (TraceTraversalObstacles(ForwardHit, EvadeDirection) && TryVault(ForwardHit, EvadeDirection))
	{
		return;
	}

	PlayEvadeMontage(EvadeDirection);
}

bool AMyPlayerCharacter::PlayEvadeMontage(const FVector& EvadeDirection)
{
	if (RollMontage == nullptr)
	{
		return false;
	}

	if (!EvadeDirection.IsNearlyZero())
	{
		SetActorRotation(EvadeDirection.Rotation());
	}

	UAnimMontage* EvadeMontage = RollMontage; // TODO: Evade montage 세팅(lock or unlock)

	UAnimInstance* AnimInstance = (GetMesh() != nullptr) ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr)
	{
		return false;
	}

	if (AnimInstance->Montage_Play(EvadeMontage) <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Evade: Montage_Play failed for %s"), *GetNameSafe(EvadeMontage));
		return false;
	}

	return true;
}

bool AMyPlayerCharacter::TryHang(const FHitResult& ForwardHit)
{
	UWorld* World = GetWorld();
	UMotionWarpingComponent* WarpingComponent = GetMotionWarpingComponent();
	if (World == nullptr || WarpingComponent == nullptr)
	{
		return false;
	}

	// 이보다 누운 면은 걸어 올라갈 경사면이지 traversal 대상이 아니다.
	// 앞면과 윗면에 서로 다른 경사 기준을 적용한다.
	constexpr float MinFaceNormalZ = 0.7f;
	constexpr float MinTopNormalZ = 0.94f;

	const FVector ActorLocation = GetActorLocation();

	// 비스듬한 벽에서는 면 법선이 액터 forward보다 정확하다.
	FVector ApproachDirection = -ForwardHit.ImpactNormal;
	ApproachDirection.Z = 0.f;
	if (!ApproachDirection.Normalize())
	{
		ApproachDirection = GetActorForwardVector();
	}

	FCollisionQueryParams QueryParams;
	FCollisionObjectQueryParams ObjectQueryParams;
	BuildTraversalQueryParams(QueryParams, ObjectQueryParams);

	const FRotator ApproachRotation = ApproachDirection.Rotation();

	const float TopTraceStartZ = ActorLocation.Z + TraversalTraceHighestZ;
	const float TopTraceEndZ = ActorLocation.Z + TraversalTraceLowestZ;

	const float FaceNormalZ = ForwardHit.ImpactNormal.Z;
	const float FaceNormalSize2D = ForwardHit.ImpactNormal.Size2D();

	if (FMath::IsNearlyZero(FaceNormalSize2D))
	{
		UE_LOG(LogTemp, Display, TEXT("Traversal: rejected, face normal is vertical (floor or ceiling, not a wall)"));
		return false;
	}

	// 이걸 넘으면 그냥 걸어 올라갈 수 있는 경사면.
	if (FaceNormalZ >= MinFaceNormalZ)
	{
		UE_LOG(LogTemp, Display, TEXT("Traversal: rejected, face is a walkable slope (normal.Z=%.2f)"), FaceNormalZ);
		return false;
	}

	const float DeltaZ = TopTraceStartZ - ForwardHit.ImpactPoint.Z;
	const float TopTraceForwardOffset = DeltaZ * (FaceNormalZ / FaceNormalSize2D);

	const FVector TopTraceXY = ForwardHit.ImpactPoint + ApproachDirection * TopTraceForwardOffset;
	const FVector TopTraceStart(TopTraceXY.X, TopTraceXY.Y, TopTraceStartZ);
	const FVector TopTraceEnd(TopTraceXY.X, TopTraceXY.Y, TopTraceEndZ);

	FHitResult TopHit;
	const bool bTopHit = World->LineTraceSingleByObjectType(TopHit, TopTraceStart, TopTraceEnd, ObjectQueryParams, QueryParams);
	if (!bTopHit)
	{
		UE_LOG(LogTemp, Display, TEXT("Traversal: rejected, no top found within the probe range"));
		return false;
	}

	if (TopHit.bStartPenetrating)
	{
		UE_LOG(LogTemp, Display, TEXT("Traversal: rejected, top trace started inside geometry (obstacle taller than the probe range)"));
		return false;
	}

	if (TopHit.ImpactNormal.Z < MinTopNormalZ)
	{
		UE_LOG(LogTemp, Display, TEXT("Traversal: rejected, top surface too steep (normal.Z=%.2f)"), TopHit.ImpactNormal.Z);
		return false;
	}

	const float TopHeightOffset = TopHit.ImpactPoint.Z - ActorLocation.Z;

	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float MaxJumpableTopOffset = GetCharacterMovement()->GetMaxJumpHeight() - CapsuleHalfHeight;

	if (TopHeightOffset <= MaxJumpableTopOffset)
	{
		UE_LOG(LogTemp, Display, TEXT("Traversal: rejected, top is within normal jump height (TopHeightOffset=%.1f <= MaxJumpableTopOffset=%.1f)"),
			TopHeightOffset, MaxJumpableTopOffset);
		return false;
	}

	if (TopHeightOffset > MaxHangHeightOffset)
	{
		UE_LOG(LogTemp, Display, TEXT("Traversal: rejected, top too high (TopHeightOffset=%.1f > MaxHangHeightOffset=%.1f)"), TopHeightOffset, MaxHangHeightOffset);
		return false;
	}

	if (TopHeightOffset >= MinHangHeightOffset)
	{
		// TODO: hang montage 선택
		FTransform HangJumpTarget;
		GetHandAlignedWarpTransform(HangMontage_FromGround, HangJumpWarpTarget, TopHit.ImpactPoint, ApproachRotation, ETraversalHandAlignment::BothHands, HangJumpTarget);

		WarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(HangJumpWarpTarget, HangJumpTarget.GetLocation(), HangJumpTarget.Rotator());

		const bool bTraversalStarted = DoTraverse(HangMontage_FromGround, ApproachDirection,{ ForwardHit.GetActor(), TopHit.GetActor() }, HangJumpWarpTarget, TopHit.ImpactPoint.Z - ActorLocation.Z);
		if (bTraversalStarted)
		{
			CachedHangForwardHit = ForwardHit;
			CachedHangTopHit = TopHit;
		}

		return bTraversalStarted;
	}

	return false;
}

bool AMyPlayerCharacter::TryMantle()
{
	UMyCharacterMovementComponent* MovementComponent = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UMotionWarpingComponent* WarpingComponent = GetMotionWarpingComponent();
	UWorld* World = GetWorld();
	if (!MovementComponent || !MovementComponent->IsClimbing() || !Capsule || !WarpingComponent || !World)
	{
		return false;
	}

	// PhysClimbing에서 업데이트하는 정보들
	const FHitResult ForwardHit = MovementComponent->GetClimbingWallHit();
	const FHitResult TopHit = MovementComponent->GetClimbingLedgeHit();

	if (!ForwardHit.bBlockingHit || !TopHit.bBlockingHit || TopHit.ImpactNormal.Z <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FVector ApproachDirection = -ForwardHit.ImpactNormal;
	ApproachDirection.Z = 0.0f;
	if (!ApproachDirection.Normalize())
	{
		return false;
	}

	const FVector MantleMoveLocation = TopHit.ImpactPoint + ApproachDirection;

	if (IsCapsuleBlockedAtLocation(MantleMoveLocation))
	{
		UE_LOG(LogTemp, Display, TEXT("Traversal: mantle rejected, no room for the capsule at %s"),
			*MantleMoveLocation.ToCompactString());
		return false;
	}

	FTransform MantleJumpTarget;
	GetHandAlignedWarpTransform(MantleMontage, MantleJumpWarpTarget, MantleMoveLocation, ApproachDirection.Rotation(), ETraversalHandAlignment::BothHands, MantleJumpTarget);

	WarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(MantleJumpWarpTarget, MantleJumpTarget.GetLocation(), MantleJumpTarget.Rotator());
	WarpingComponent->AddOrUpdateWarpTargetFromLocation(MantleMoveWarpTarget, MantleMoveLocation);

	return DoTraverse(MantleMontage, ApproachDirection, { ForwardHit.GetActor(), TopHit.GetActor()}, MantleMoveWarpTarget, TopHit.ImpactPoint.Z - GetActorLocation().Z);
}

bool AMyPlayerCharacter::TryVault(const FHitResult& ForwardHit, const FVector& VaultDirection)
{
	UWorld* World = GetWorld();
	UMotionWarpingComponent* WarpingComponent = GetMotionWarpingComponent();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (World == nullptr || WarpingComponent == nullptr || Capsule == nullptr)
	{
		return false;
	}

	// 앞면과 윗면에 서로 다른 경사 기준을 적용한다.
	constexpr float MinFaceNormalZ = 0.7f;
	constexpr float MinTopNormalZ = 0.94f;
	const FVector ActorLocation = GetActorLocation();
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	FCollisionQueryParams QueryParams;
	FCollisionObjectQueryParams ObjectQueryParams;
	BuildTraversalQueryParams(QueryParams, ObjectQueryParams);

	///////////////////////////////////////////////////////////////
	// Initial top trace for slope check and hand contact
	///////////////////////////////////////////////////////////////

	const float TopTraceStartZ = ActorLocation.Z + TraversalTraceHighestZ;
	const float TopTraceEndZ = ActorLocation.Z + TraversalTraceLowestZ;
	const float FaceNormalZ = ForwardHit.ImpactNormal.Z;
	const float FaceNormalSize2D = ForwardHit.ImpactNormal.Size2D();
	if (FMath::IsNearlyZero(FaceNormalSize2D))
	{
		UE_LOG(LogTemp, Display, TEXT("Evade: traversal rejected, face normal is vertical"));
		return false;
	}

	if (FaceNormalZ >= MinFaceNormalZ)
	{
		UE_LOG(LogTemp, Display, TEXT("Evade: traversal rejected, face is a walkable slope (normal.Z=%.2f)"), FaceNormalZ);
		return false;
	}

	// 앞면에 경사가 있을 때 Top trace 위치 찾기
	const float FaceTangent = FaceNormalZ / FaceNormalSize2D;
	const float TopTraceForwardOffset = (TopTraceStartZ - ForwardHit.ImpactPoint.Z) * FMath::Max(FaceTangent, 0.f) + VaultForwardHandOffset;

	FVector ApproachDirection = VaultDirection;
	ApproachDirection.Z = 0.f;
	if (!ApproachDirection.Normalize())
	{
		UE_LOG(LogTemp, Display, TEXT("Evade: traversal rejected, vault direction is vertical"));
		return false;
	}

	const FRotator ApproachRotation = ApproachDirection.Rotation();
	const FVector TopTraceXY = ForwardHit.ImpactPoint + ApproachDirection * TopTraceForwardOffset;
	const FVector TopTraceStart(TopTraceXY.X, TopTraceXY.Y, TopTraceStartZ);
	const FVector TopTraceEnd(TopTraceXY.X, TopTraceXY.Y, TopTraceEndZ);

	FHitResult TopHit;
	const bool bTopHit = World->LineTraceSingleByObjectType(TopHit, TopTraceStart, TopTraceEnd, ObjectQueryParams, QueryParams);

	if (!bTopHit || TopHit.bStartPenetrating)
	{
		UE_LOG(LogTemp, Display, TEXT("Vault: traversal rejected, no usable top found"));
		return false;
	}

	if (TopHit.ImpactNormal.Z < MinTopNormalZ)
	{
		UE_LOG(LogTemp, Display, TEXT("Vault: traversal rejected, top surface too steep (normal.Z=%.2f)"), TopHit.ImpactNormal.Z);
		return false;
	}

	const float TopHeightOffset = TopHit.ImpactPoint.Z - ActorLocation.Z;
	if (TopHeightOffset >= MinHangHeightOffset)
	{
		return false;
	}
	
	////////////////////////////////////////////////////////////////
	// Additional top traces for depth check to find land point
	////////////////////////////////////////////////////////////////

	enum class EVaultDepthCheckResult : uint8
	{
		Invalid,
		BackEdgeFound,
		TopStopsAtObstacle,
		TopContinuesToMaxDepth
	};

	if (MaxVaultDepth <= 0.f || VaultTraceDepthStep <= KINDA_SMALL_NUMBER || MaxVaultLandingDrop <= 0.f)
	{
		return false;
	}

	const float TopHeightTolerance = CapsuleRadius;
	const FCollisionShape ClearanceTraceShape = FCollisionShape::MakeSphere(FMath::Max(0.f, CapsuleRadius));
	// top 표면이 경사인 경우 sphere collision을 표면에서 떨어트리기 위한 offset을 접할 때 기준으로 계산한다
	const auto GetClearanceOffsetZ = [CapsuleRadius](const float SurfaceNormalZ) { return CapsuleRadius / SurfaceNormalZ + 1.f; };
	FVector PreviousClearancePoint = TopHit.ImpactPoint + GetClearanceOffsetZ(TopHit.ImpactNormal.Z) * FVector::UpVector;
	FHitResult DeepestOnTopHit = TopHit;
	FVector FirstOffTopXY = FVector::ZeroVector;
	float FirstOffTopZ = TopHit.ImpactPoint.Z;
	float PreviousTopDepth = 0.f;
	EVaultDepthCheckResult DepthCheckResult = EVaultDepthCheckResult::Invalid;

	// top 표면의 경사가 일정하다는 것을 전제로 한다(새로운 장애물 제외).
	const auto GetExpectedTopZ = [&TopHit, &ApproachDirection](const float Depth)
	{
		const FVector HorizontalOffset = ApproachDirection * Depth;
		return TopHit.ImpactPoint.Z - FVector::DotProduct(TopHit.ImpactNormal, HorizontalOffset) / TopHit.ImpactNormal.Z;
	};

	const auto TraceTopAtDepth = [&](const float Depth, FVector& OutSampleXY, float& OutExpectedTopZ, FHitResult& OutHit)
	{
		OutSampleXY = TopTraceXY + ApproachDirection * Depth;
		OutExpectedTopZ = GetExpectedTopZ(Depth);
		const FVector SampleStart(OutSampleXY.X, OutSampleXY.Y, OutExpectedTopZ + CapsuleRadius);
		const FVector SampleEnd(OutSampleXY.X, OutSampleXY.Y, OutExpectedTopZ - TopHeightTolerance);
		const bool bHit = World->LineTraceSingleByObjectType(OutHit, SampleStart, SampleEnd, ObjectQueryParams, QueryParams);
		return bHit;
	};

	float CurrentDepth = 0.f;
	while (CurrentDepth < MaxVaultDepth)
	{
		const float RemainingDepth = MaxVaultDepth - CurrentDepth;
		CurrentDepth += FMath::Min(VaultTraceDepthStep, RemainingDepth);

		FVector SampleXY;
		float ExpectedTopZ = 0.f;
		FHitResult SampleHit;
		const bool bSampleHit = TraceTopAtDepth(CurrentDepth, SampleXY, ExpectedTopZ, SampleHit);
		if (!bSampleHit)
		{
			FirstOffTopXY = SampleXY;
			FirstOffTopZ = ExpectedTopZ;
			DepthCheckResult = EVaultDepthCheckResult::BackEdgeFound;
			UE_LOG(LogTemp, Display, TEXT("Vault: back edge found between depth %.1f and %.1f"), PreviousTopDepth, CurrentDepth);
			break;
		}

		if (SampleHit.bStartPenetrating)
		{
			DepthCheckResult = EVaultDepthCheckResult::TopStopsAtObstacle;
			UE_LOG(LogTemp, Display, TEXT("Vault: top depth trace started inside geometry at depth %.1f"), CurrentDepth);
			break;
		}

		if (SampleHit.ImpactNormal.Z < MinTopNormalZ)
		{
			DepthCheckResult = EVaultDepthCheckResult::TopStopsAtObstacle;
			UE_LOG(LogTemp, Display, TEXT("Vault: top depth surface is too steep at depth %.1f (normal.Z=%.2f)"), CurrentDepth, SampleHit.ImpactNormal.Z);
			break;
		}

		const FVector CurrentClearancePoint = SampleHit.ImpactPoint + GetClearanceOffsetZ(SampleHit.ImpactNormal.Z) * FVector::UpVector;
		const bool bClearanceBlocked = World->SweepTestByObjectType(PreviousClearancePoint, CurrentClearancePoint, FQuat::Identity, ObjectQueryParams, ClearanceTraceShape, QueryParams);
		if (bClearanceBlocked)
		{
			DepthCheckResult = EVaultDepthCheckResult::TopStopsAtObstacle;
			UE_LOG(LogTemp, Display, TEXT("Vault: top clearance sweep is blocked at depth %.1f"), CurrentDepth);
			break;
		}

		PreviousClearancePoint = CurrentClearancePoint;
		PreviousTopDepth = CurrentDepth;
		DeepestOnTopHit = SampleHit;

		if (CurrentDepth >= MaxVaultDepth - KINDA_SMALL_NUMBER)
		{
			DepthCheckResult = EVaultDepthCheckResult::TopContinuesToMaxDepth;
			UE_LOG(LogTemp, Display, TEXT("Vault: top continues through max depth %.1f; selecting a top landing"), MaxVaultDepth);
			break;
		}
	}

	////////////////////////////////
	// Try vault
	////////////////////////////////

	UAnimMontage* VaultMontage = VaultMontage_ToGround;

	FVector VaultMoveLocation = FVector::ZeroVector;
	if (DepthCheckResult == EVaultDepthCheckResult::TopContinuesToMaxDepth || DepthCheckResult == EVaultDepthCheckResult::TopStopsAtObstacle)
	{
		VaultMoveLocation = DeepestOnTopHit.ImpactPoint;
	}
	else if (DepthCheckResult == EVaultDepthCheckResult::BackEdgeFound)
	{
		const FVector OverEdgeClearancePoint(FirstOffTopXY.X, FirstOffTopXY.Y, FirstOffTopZ + GetClearanceOffsetZ(DeepestOnTopHit.ImpactNormal.Z));
		const bool bOverEdgeClearanceBlocked = World->SweepTestByObjectType(PreviousClearancePoint, OverEdgeClearancePoint, FQuat::Identity, ObjectQueryParams, ClearanceTraceShape, QueryParams);
		if (bOverEdgeClearanceBlocked)
		{
			UE_LOG(LogTemp, Display, TEXT("Vault: over-edge clearance sweep is blocked after depth %.1f"), PreviousTopDepth);
			return false;
		}

		const FVector LandingProbeXY = FirstOffTopXY + ApproachDirection * CapsuleRadius;
		const FVector LandingFeetStart(LandingProbeXY.X, LandingProbeXY.Y, FirstOffTopZ + VaultClearanceHeight);
		const FVector LandingFeetEnd(LandingProbeXY.X, LandingProbeXY.Y, TopHit.ImpactPoint.Z - MaxVaultLandingDrop);
		const FVector LandingSweepStart = LandingFeetStart + CapsuleHalfHeight * FVector::UpVector;
		const FVector LandingSweepEnd = LandingFeetEnd + CapsuleHalfHeight * FVector::UpVector;
		const FCollisionShape LandingSweepShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
		FHitResult LandHit;
		const bool bLandHit = World->SweepSingleByObjectType(LandHit, LandingSweepStart, LandingSweepEnd, FQuat::Identity, ObjectQueryParams, LandingSweepShape, QueryParams);
		if (!bLandHit)
		{
			UE_LOG(LogTemp, Display, TEXT("Vault: landing capsule sweep missed after the back edge at depth %.1f"), CurrentDepth);
			VaultMoveLocation = LandingFeetStart;
			VaultMontage = VaultMontage_ToAir;
		}
		else if (LandHit.bStartPenetrating)
		{
			UE_LOG(LogTemp, Display, TEXT("Vault: landing capsule sweep started inside geometry at depth %.1f"), CurrentDepth + CapsuleRadius);
			VaultMoveLocation = DeepestOnTopHit.ImpactPoint;
		}
		else
		{
			VaultMoveLocation = LandHit.Location - (CapsuleHalfHeight - 1.0f) * FVector::UpVector;
		}
	}
	else
	{
		return false;
	}

	FTransform VaultJumpTarget;
	GetHandAlignedWarpTransform(VaultMontage, VaultJumpWarpTarget, TopHit.ImpactPoint, ApproachRotation, ETraversalHandAlignment::LeftHand, VaultJumpTarget);
	WarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(VaultJumpWarpTarget, VaultJumpTarget.GetLocation(), VaultJumpTarget.Rotator());
	WarpingComponent->AddOrUpdateWarpTargetFromLocation(VaultMoveWarpTarget, VaultMoveLocation);

	return DoTraverse(VaultMontage, ApproachDirection, { ForwardHit.GetActor(), TopHit.GetActor(), DeepestOnTopHit.GetActor() }, VaultMoveWarpTarget, VaultMoveLocation.Z - ActorLocation.Z);
}

void AMyPlayerCharacter::OnHangTraversalEnded(bool bTraversalSucceeded)
{
	if (!bTraversalSucceeded)
	{
		CachedHangForwardHit = FHitResult();
		CachedHangTopHit = FHitResult();
		RestoreTraversalPhysics();
		return;
	}

	UMyCharacterMovementComponent* MovementComponent = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
	if (MovementComponent != nullptr)
	{
		MovementComponent->StartClimbing(CachedHangForwardHit, CachedHangTopHit);
	}

	if (!MovementComponent || !MovementComponent->IsClimbing())
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: hang succeeded but climbing did not start, restoring physics"));
		CachedHangForwardHit = FHitResult();
		CachedHangTopHit = FHitResult();
		RestoreTraversalPhysics();
	}
}

void AMyPlayerCharacter::OnMantleTraversalEnded(bool bTraversalSucceeded)
{
	(void)bTraversalSucceeded;
	RestoreTraversalPhysics();
	CachedHangForwardHit = FHitResult();
	CachedHangTopHit = FHitResult();
}

void AMyPlayerCharacter::OnVaultTraversalEnded(bool bTraversalSucceeded)
{
	(void)bTraversalSucceeded;
	RestoreTraversalPhysics();
}

void AMyPlayerCharacter::OnTraversalWarpEnded(const FName& WarpTargetName, bool bTraversalSucceeded)
{
	PendingTraversalWarpIds.Remove(ActiveTraversalId);
	RemoveStatusTag(MyGameplayTags::Status_Channeling);

	if (WarpTargetName == HangJumpWarpTarget)
	{
		OnHangTraversalEnded(bTraversalSucceeded);
	}
	else if (WarpTargetName == MantleMoveWarpTarget)
	{
		OnMantleTraversalEnded(bTraversalSucceeded);
	}
	else if (WarpTargetName == VaultMoveWarpTarget)
	{
		OnVaultTraversalEnded(bTraversalSucceeded);
	}

	if (UMotionWarpingComponent* WarpingComponent = GetMotionWarpingComponent())
	{
		WarpingComponent->RemoveAllWarpTargets();
	}
}

void AMyPlayerCharacter::OnTraversalMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 TraversalId, FName TraversalWarpTarget)
{
	if (TraversalId != ActiveTraversalId)
	{
		PendingTraversalWarpIds.Remove(TraversalId);
		return;
	}

	const bool bTraversalSucceeded = !PendingTraversalWarpIds.Contains(TraversalId);
	if (!bTraversalSucceeded)
	{
		OnTraversalWarpEnded(TraversalWarpTarget, false);
		return;
	}

	// 애니메이션 에셋 때문에 추가한 로직. montage가 재생되는 동안
	// pelvis 역보정을 상쇄시키기 위해 root motion으로 capsule을 움직여야 한다
	if (TraversalWarpTarget == HangJumpWarpTarget)
	{
		RestoreTraversalCollision();
	}
}

bool AMyPlayerCharacter::TraceTraversalObstacles(FHitResult& OutHit, const FVector& TraceDirection)
{
	UWorld* World = GetWorld();
	if (World == nullptr || ForwardTraceCount <= 0)
	{
		return false;
	}

	const FVector ActorLocation = GetActorLocation();
	FVector ForwardDirection = TraceDirection;
	ForwardDirection.Z = 0.f;
	if (!ForwardDirection.Normalize())
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	FCollisionObjectQueryParams ObjectQueryParams;
	BuildTraversalQueryParams(QueryParams, ObjectQueryParams);

	constexpr float MinApproachAngleDeg = 20.f;
	const float MinApproachDot = FMath::Cos(FMath::DegreesToRadians(90.f - MinApproachAngleDeg));
	const FVector Forward2D = FVector(ForwardDirection.X, ForwardDirection.Y, 0.f).GetSafeNormal();

	// 트레이스가 하나뿐이면 보간할 게 없으므로 최저 높이에 놓인다.
	const float ZStep = (ForwardTraceCount > 1)
		? (TraversalTraceHighestZ - TraversalTraceLowestZ) / static_cast<float>(ForwardTraceCount - 1)
		: 0.f;

	for (int32 TraceIndex = 0; TraceIndex < ForwardTraceCount; ++TraceIndex)
	{
		const float TraceZ = TraversalTraceLowestZ + ZStep * TraceIndex;
		const FVector TraceStart = ActorLocation + FVector(0.f, 0.f, TraceZ);
		const FVector TraceEnd = TraceStart + ForwardDirection * TraversalReachDistance;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByObjectType(
			Hit,
			TraceStart,
			TraceEnd,
			ObjectQueryParams,
			QueryParams
		);

		if (bHit)
		{
			// 노멀이 순수 수직이면(바닥/천장) 0벡터가 되어 dot도 0이 되므로 그대로 걸러진다.
			const FVector Normal2D = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
			const float ApproachDot = FVector::DotProduct(Forward2D, -Normal2D);

			if (ApproachDot < MinApproachDot)
			{
				// 이 높이만 스친 것일 수 있으므로 다음 높이의 트레이스로 넘어간다.
				UE_LOG(LogTemp, Display, TEXT("Traversal: trace [%d] rejected, grazing approach (dot=%.2f < %.2f)"),
					TraceIndex, ApproachDot, MinApproachDot);
				continue;
			}

			OutHit = Hit;
			return true;
		}
	}

	return false;
}

void AMyPlayerCharacter::BuildTraversalQueryParams(FCollisionQueryParams& OutQuery, FCollisionObjectQueryParams& OutObject) const
{
	OutQuery.AddIgnoredActor(this);

	OutObject.AddObjectTypesToQuery(ECC_WorldStatic);
	OutObject.AddObjectTypesToQuery(ECC_WorldDynamic);
}

bool AMyPlayerCharacter::IsCapsuleBlockedAtLocation(const FVector& CapsuleBaseLocation) const
{
	FCollisionQueryParams QueryParams;
	FCollisionObjectQueryParams ObjectQueryParams;
	BuildTraversalQueryParams(QueryParams, ObjectQueryParams);

	constexpr float StandClearance = 2.f;
	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector CapsuleCenter = CapsuleBaseLocation + FVector(0.f, 0.f, CapsuleHalfHeight + StandClearance);
	const bool bBlocked = GetWorld()->OverlapAnyTestByObjectType(CapsuleCenter, FQuat::Identity, ObjectQueryParams,
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), QueryParams);

	return bBlocked;
}

bool AMyPlayerCharacter::DoTraverse(UAnimMontage* Montage, const FVector& FacingDirection, const TArray<AActor*>& ObstacleActors, FName TraversalWarpTarget, float TraversalTopHeightOffset)
{
	if (Montage == nullptr)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	if (AnimInstance->Montage_Play(Montage) <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: Montage_Play failed for %s"), *GetNameSafe(Montage));
		if (UMotionWarpingComponent* WarpingComponent = GetMotionWarpingComponent())
		{
			WarpingComponent->RemoveAllWarpTargets();
		}

		return false;
	}

	if (!FacingDirection.IsNearlyZero())
	{
		SetActorRotation(FacingDirection.Rotation());
	}

	const uint64 TraversalId = ++ActiveTraversalId;
	PendingTraversalWarpIds.Add(TraversalId);
	FOnMontageEnded EndDelegate = FOnMontageEnded::CreateUObject(this, &ThisClass::OnTraversalMontageEnded, TraversalId, TraversalWarpTarget);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

	AddStatusTag(MyGameplayTags::Status_Channeling);
	SetupTraversalCamera(TraversalTopHeightOffset);

	// MOVE_Walking은 수직 루트 모션을 버린다. 
	// PhysWalking이 속도를 MoveAlongFloor로 넘기면서 중력 성분을 투영해 제거한다
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);

	TraversalIgnoredActors.Reset();
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		for (AActor* Obstacle : ObstacleActors)
		{
			// 전방 면과 윗면은 보통 같은 액터지만 반드시 그렇지는 않다.
			if (Obstacle != nullptr && !TraversalIgnoredActors.Contains(Obstacle))
			{
				Capsule->IgnoreActorWhenMoving(Obstacle, true);
				TraversalIgnoredActors.Add(Obstacle);
			}
		}
	}

	return true;
}

bool AMyPlayerCharacter::GetHandAlignedWarpTransform(UAnimMontage* Montage, const FName& WarpTargetName,
	const FVector& ContactPoint, const FRotator& ApproachRotation, ETraversalHandAlignment HandAlignment, FTransform& OutTarget) const
{
	// 실패해도 호출부가 쓸 수 있도록 접촉점을 먼저 넣어 둔다.
	OutTarget = FTransform(ApproachRotation, ContactPoint);

	if (Montage == nullptr || GetMesh() == nullptr)
	{
		return false;
	}

	USkeletalMesh* SkelMesh = GetMesh()->GetSkeletalMeshAsset();
	if (SkelMesh == nullptr)
	{
		return false;
	}

	// 접촉 시점을 notify state에서 읽는다.
	TArray<FMotionWarpingWindowData> Windows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, WarpTargetName, Windows);
	if (Windows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: %s has no %s window, using the contact point as is"), *GetNameSafe(Montage), *WarpTargetName.ToString());
		return false;
	}

	TArray<FName> HandBoneNames;
	if (HandAlignment != ETraversalHandAlignment::RightHand)
	{
		HandBoneNames.Add(LeftHandBoneName);
	}
	if (HandAlignment != ETraversalHandAlignment::LeftHand)
	{
		HandBoneNames.Add(RightHandBoneName);
	}

	// 한쪽 손 본이 없어도 남은 손으로 진행한다.
	TArray<int32> HandBoneIndices;
	for (const FName& HandBoneName : HandBoneNames)
	{
		const int32 BoneIndex = GetMesh()->GetBoneIndex(HandBoneName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("Traversal: bone %s not found"), *HandBoneName.ToString());
			continue;
		}

		HandBoneIndices.Add(BoneIndex);
	}

	if (HandBoneIndices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: no usable hand bone, using the contact point as is"));
		return false;
	}

	// AMyWeapon::BuildBoneContainer와 같은 방식으로 컨테이너를 직접 만든다.
	// RefSkeleton 기준이라 LOD의 required bones와 무관하다.
	const FReferenceSkeleton& RefSkel = SkelMesh->GetRefSkeleton();
	TArray<FBoneIndexType> RequiredBones;

	for (const int32 HandBoneIndex : HandBoneIndices)
	{
		int32 Current = HandBoneIndex;
		while (Current != INDEX_NONE)
		{
			RequiredBones.AddUnique(static_cast<FBoneIndexType>(Current));
			Current = RefSkel.GetParentIndex(Current);
		}
	}
	RequiredBones.Sort();

	FBoneContainer BoneContainer;
	BoneContainer.InitializeTo(RequiredBones, UE::Anim::FCurveFilterSettings(), *SkelMesh);

	FCSPose<FCompactPose> CSPose;
	UMotionWarpingUtilities::ExtractComponentSpacePose(Montage, BoneContainer, Windows[0].EndTime, false, CSPose);

	// 엔진의 Bone 워프 포인트는 FCompactPoseBoneIndex(1)을 하드코딩해서
	// 루트 직계 자식이 아닌 본에는 쓸 수 없다. 실제 컴팩트 인덱스로 변환한다.
	const FCompactPoseBoneIndex RootCompact = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(0));
	if (RootCompact.GetInt() == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: compact pose index lookup failed, using the contact point as is"));
		return false;
	}

	const FTransform MeshToActor = FTransform(GetBaseRotationOffset());
	const FTransform RootBoneToActor = CSPose.GetComponentSpaceTransform(RootCompact) * MeshToActor;

	FVector HandLocationSum = FVector::ZeroVector;
	int32 HandSampleCount = 0;
	for (const int32 HandBoneIndex : HandBoneIndices)
	{
		const FCompactPoseBoneIndex HandCompact = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandBoneIndex));
		if (HandCompact.GetInt() == INDEX_NONE)
		{
			continue;
		}

		HandLocationSum += (CSPose.GetComponentSpaceTransform(HandCompact) * MeshToActor).GetLocation();
		++HandSampleCount;
	}

	if (HandSampleCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: compact pose index lookup failed, using the contact point as is"));
		return false;
	}

	const FVector HandLocationInActorSpace = HandLocationSum / static_cast<float>(HandSampleCount);
	const FVector HandOffsetInActorSpace = HandLocationInActorSpace - RootBoneToActor.GetLocation();
	const FVector HandOffsetInWorldSpace = ApproachRotation.RotateVector(HandOffsetInActorSpace);
	//const FTransform HandBoneToActor = CSPose.GetComponentSpaceTransform(HandCompact) * MeshToActor;
	//const FVector HandInRootSpace = RootBoneToActor.InverseTransformPosition(HandBoneToActor.GetLocation());
	//const FVector HandOffsetInWorldSpace = ApproachRotation.RotateVector(RootBoneToActor.TransformVector(HandInRootSpace));

	OutTarget = FTransform(ApproachRotation, ContactPoint - HandOffsetInWorldSpace);

	return true;
}

void AMyPlayerCharacter::SetupTraversalCamera(float TraversalTopHeightOffset)
{
	if (CameraBoom == nullptr || FollowCamera == nullptr)
	{
		return;
	}

	constexpr float CameraClearanceMargin = 10.f;
	const FVector DefaultArmOrigin = CameraBoom->GetComponentLocation() + DefaultCameraBoomTargetOffset;
	const FVector ArmOriginToCameraDirection =
		(FollowCamera->GetComponentLocation() - DefaultArmOrigin).GetSafeNormal();

	const float SafeArmOriginHeightOffset = TraversalTopHeightOffset + CameraBoom->ProbeSize + CameraClearanceMargin;
	const float ArmOriginHeightOffset = static_cast<float>(DefaultArmOrigin.Z - GetActorLocation().Z);
	const float RequiredPullBackDistance = FMath::Max(0.f, SafeArmOriginHeightOffset - ArmOriginHeightOffset);

	CameraBoom->TargetOffset = DefaultCameraBoomTargetOffset
		+ RequiredPullBackDistance * ArmOriginToCameraDirection;
	bTraversalCameraActive = true;
}

void AMyPlayerCharacter::RestoreTraversalCamera()
{
	if (CameraBoom != nullptr)
	{
		CameraBoom->TargetOffset = DefaultCameraBoomTargetOffset;
	}

	bTraversalCameraActive = false;
}

void AMyPlayerCharacter::RestoreTraversalCollision()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		for (const TWeakObjectPtr<AActor>& IgnoredActor : TraversalIgnoredActors)
		{
			if (AActor* Obstacle = IgnoredActor.Get())
			{
				Capsule->IgnoreActorWhenMoving(Obstacle, false);
			}
		}
	}

	TraversalIgnoredActors.Reset();
}

void AMyPlayerCharacter::RestoreTraversalPhysics()
{
	RestoreTraversalCollision();

	// TODO: 다른 로직에 의해 변경되었는지 확인하는 로직 보완
	if (GetCharacterMovement()->MovementMode == MOVE_Flying) 
	{
		GetCharacterMovement()->SetDefaultMovementMode();
	}

	RestoreTraversalCamera();
}

FVector AMyPlayerCharacter::GetWorldMovementDirection(const FVector2D& MovementVector) const
{
	if (MovementVector.IsNearlyZero() || GetController() == nullptr)
	{
		return FVector::ZeroVector;
	}

	const FRotator ControlRotation = GetController()->GetControlRotation();
	const FRotator ControlYawRotation(0.f, ControlRotation.Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(ControlYawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(ControlYawRotation).GetUnitAxis(EAxis::Y);
	return (ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X).GetSafeNormal();
}
