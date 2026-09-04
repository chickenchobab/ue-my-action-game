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
#include "Animation/AnimInstance.h"
#include "MotionWarpingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "BonePose.h"
#include "DrawDebugHelpers.h"

static bool bShouldDrawTraversalTrace = true;
static const float TraversalDebugDrawTime = 2.0f;

// Move == Traversal
static const FName VaultJumpWarpTarget(TEXT("Vault_Jump"));
static const FName VaultMoveWarpTarget(TEXT("Vault_Move"));

static const FName TraversalHandBoneName(TEXT("hand_l"));

static inline void DrawTraversalSweep(UWorld* World, const FVector& Start, const FVector& End, float Radius, const FColor& Color)
{
	if (bShouldDrawTraversalTrace)
	{
		DrawDebugSphere(World, Start, Radius, 12, Color, false, TraversalDebugDrawTime, 0, 1.0f);
		DrawDebugSphere(World, End, Radius, 12, Color, false, TraversalDebugDrawTime, 0, 1.0f);
	}
}

static inline void DrawTraversalImpact(UWorld* World, const FVector& ImpactPoint, float Radius)
{
	if (bShouldDrawTraversalTrace)
	{
		DrawDebugSphere(World, ImpactPoint, Radius, 12, FColor::Yellow, false, TraversalDebugDrawTime, 0, 2.0f);
	}
}

static inline void DrawVaultDepthTrace(UWorld* World, const FVector& Start, const FVector& End,
	const FHitResult& Hit, bool bHit, float Radius)
{
	if (!bShouldDrawTraversalTrace)
	{
		return;
	}

	DrawDebugSphere(World, Start, Radius, 12, FColor::Cyan, false, TraversalDebugDrawTime, 0, 1.0f);
	DrawDebugSphere(World, End, Radius, 12, FColor::Blue, false, TraversalDebugDrawTime, 0, 1.0f);
	DrawDebugLine(World, Start, End, bHit ? FColor::Green : FColor::Red, false, TraversalDebugDrawTime, 0, 1.0f);
	if (bHit)
	{
		DrawDebugSphere(World, Hit.ImpactPoint, Radius, 12, FColor::Yellow, false, TraversalDebugDrawTime, 0, 2.0f);
	}
}

static inline void DrawVaultClearanceTrace(UWorld* World, const FVector& PreviousPoint,
	const FVector& CurrentPoint, float Radius)
{
	if (!bShouldDrawTraversalTrace)
	{
		return;
	}

	DrawDebugSphere(World, PreviousPoint, Radius, 12, FColor::Purple, false, TraversalDebugDrawTime, 0, 4.0f);
	DrawDebugSphere(World, CurrentPoint, Radius, 12, FColor::Purple, false, TraversalDebugDrawTime, 0, 4.0f);
	DrawDebugLine(World, PreviousPoint, CurrentPoint, FColor::Purple, false, TraversalDebugDrawTime, 0, 4.0f);
}

static inline void DrawVaultLandHit(UWorld* World, const FHitResult& LandHit, float Radius)
{
	if (bShouldDrawTraversalTrace && LandHit.bBlockingHit)
	{
		const FColor LandHitColor(255, 128, 0);
		DrawDebugSphere(World, LandHit.ImpactPoint, Radius, 16, LandHitColor, false, TraversalDebugDrawTime, 0, 3.0f);
		DrawDebugDirectionalArrow(World, LandHit.ImpactPoint, LandHit.ImpactPoint + LandHit.ImpactNormal * Radius * 2.f, Radius, LandHitColor, false, TraversalDebugDrawTime, 0, 2.0f);
	}
}

AMyPlayerCharacter::AMyPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetUsingAbsoluteRotation(true); // Animation �������� ĳ���� ȸ���� �����ϹǷ�

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	CachedWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
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

	UAnimMontage* EvadeMontage = RollMontage; // TODO: Evade montage ����(lock or unlock)

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

bool AMyPlayerCharacter::TryVault(const FHitResult& ForwardHit, const FVector& VaultDirection)
{
	UWorld* World = GetWorld();
	UMotionWarpingComponent* WarpingComponent = GetMotionWarpingComponent();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (World == nullptr || WarpingComponent == nullptr || Capsule == nullptr)
	{
		return false;
	}

	// �ո�� ���鿡 ���� �ٸ� ��� ������ �����Ѵ�.
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

	// �ո鿡 ��簡 ���� �� Top trace ��ġ ã��
	const float FaceTangent = FaceNormalZ / FaceNormalSize2D;
	const float TopTraceForwardOffset = (TopTraceStartZ - ForwardHit.ImpactPoint.Z) * FMath::Max(FaceTangent, 0.f) + ForwardHandOffset;

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

	DrawTraversalSweep(World, TopTraceStart, TopTraceEnd, 5.f, bTopHit ? FColor::Red : FColor::Green);
	if (!bTopHit || TopHit.bStartPenetrating)
	{
		UE_LOG(LogTemp, Display, TEXT("Vault: traversal rejected, no usable top found"));
		return false;
	}

	DrawTraversalImpact(World, TopHit.ImpactPoint, 5.f);
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
	// top ǥ���� ����� ��� sphere collision�� ǥ�鿡�� ����Ʈ���� ���� offset�� ���� �� �������� ����Ѵ�
	const auto GetClearanceOffsetZ = [CapsuleRadius](const float SurfaceNormalZ) { return CapsuleRadius / SurfaceNormalZ + 1.f; };
	FVector PreviousClearancePoint = TopHit.ImpactPoint + GetClearanceOffsetZ(TopHit.ImpactNormal.Z) * FVector::UpVector;
	FHitResult DeepestOnTopHit = TopHit;
	FVector FirstOffTopXY = FVector::ZeroVector;
	float FirstOffTopZ = TopHit.ImpactPoint.Z;
	float PreviousTopDepth = 0.f;
	EVaultDepthCheckResult DepthCheckResult = EVaultDepthCheckResult::Invalid;

	// top ǥ���� ��簡 �����ϴٴ� ���� ������ �Ѵ�(���ο� ��ֹ� ����).
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
		//DrawVaultDepthTrace(World, SampleStart, SampleEnd, OutHit, bHit, 10.f);
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
		DrawVaultClearanceTrace(World, PreviousClearancePoint, CurrentClearancePoint, CapsuleRadius / 5.f);
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
		DrawVaultClearanceTrace(World, PreviousClearancePoint, OverEdgeClearancePoint, CapsuleRadius / 5);
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
			DrawVaultLandHit(World, LandHit, 15.f);
			VaultMoveLocation = LandHit.Location - (CapsuleHalfHeight - 1.0f) * FVector::UpVector;
		}
	}
	else
	{
		return false;
	}

	FTransform VaultJumpTarget;
	GetHandAlignedWarpTransform(VaultMontage, VaultJumpWarpTarget, TopHit.ImpactPoint, ApproachRotation, VaultJumpTarget);
	WarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(VaultJumpWarpTarget, VaultJumpTarget.GetLocation(), VaultJumpTarget.Rotator());
	WarpingComponent->AddOrUpdateWarpTargetFromLocation(VaultMoveWarpTarget, VaultMoveLocation);

	// return DoTraverse(VaultMontage, ApproachDirection, { ForwardHit.GetActor(), TopHit.GetActor(), DeepestOnTopHit.GetActor() }, VaultMoveWarpTarget);
	// modified: use the highest accepted top sample for the traversal camera clearance.
	const float TraversalTopZ = FMath::Max(TopHit.ImpactPoint.Z, DeepestOnTopHit.ImpactPoint.Z);
	return DoTraverse(VaultMontage, ApproachDirection, { ForwardHit.GetActor(), TopHit.GetActor(), DeepestOnTopHit.GetActor() }, VaultMoveWarpTarget, TraversalTopZ);
}

// modified: �Ŵ޸��Ⱑ �Ϸ�Ǹ� �Ϲ� �Է� ������ ������ �Է� �������� ��ü�Ѵ�.
void AMyPlayerCharacter::OnHangTraversalEnded()
{
	if (ClimbMappingContext == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: ClimbMappingContext is not set"));
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(ClimbMappingContext, 0);
		}
	}
}

void AMyPlayerCharacter::OnVaultTraversalEnded()
{
	RestoreTraversalPhysics();
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
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(30.f);

	constexpr float MinApproachAngleDeg = 20.f;
	const float MinApproachDot = FMath::Cos(FMath::DegreesToRadians(90.f - MinApproachAngleDeg));
	const FVector Forward2D = FVector(ForwardDirection.X, ForwardDirection.Y, 0.f).GetSafeNormal();

	// Ʈ���̽��� �ϳ����̸� ������ �� �����Ƿ� ���� ���̿� ���δ�.
	const float ZStep = (ForwardTraceCount > 1)
		? (TraversalTraceHighestZ - TraversalTraceLowestZ) / static_cast<float>(ForwardTraceCount - 1)
		: 0.f;

	for (int32 TraceIndex = 0; TraceIndex < ForwardTraceCount; ++TraceIndex)
	{
		const float TraceZ = TraversalTraceLowestZ + ZStep * TraceIndex;
		const FVector TraceStart = ActorLocation + FVector(0.f, 0.f, TraceZ);
		const FVector TraceEnd = TraceStart + ForwardDirection * TraversalReachDistance;

		FHitResult Hit;
		const bool bHit = World->SweepSingleByObjectType(
			Hit,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			ObjectQueryParams,
			TraceShape,
			QueryParams
		);

		if (bHit)
		{
			// ����� ���� �����̸�(�ٴ�/õ��) 0���Ͱ� �Ǿ� dot�� 0�� �ǹǷ� �״�� �ɷ�����.
			const FVector Normal2D = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
			const float ApproachDot = FVector::DotProduct(Forward2D, -Normal2D);

			if (ApproachDot < MinApproachDot)
			{
				// �� ���̸� ��ģ ���� �� �����Ƿ� ���� ������ Ʈ���̽��� �Ѿ��.
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

bool AMyPlayerCharacter::DoTraverse(UAnimMontage* Montage, const FVector& FacingDirection, const TArray<AActor*>& ObstacleActors, FName TraversalWarpTarget, float TraversalTopZ)
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

	if (!FacingDirection.IsNearlyZero())
	{
		SetActorRotation(FacingDirection.Rotation());
	}

	AnimInstance->Montage_Play(Montage);

	const uint64 TraversalId = ++ActiveTraversalId;
	PendingTraversalWarpIds.Add(TraversalId);
	FOnMontageEnded EndDelegate = FOnMontageEnded::CreateUObject(this, &ThisClass::OnTraversalMontageEnded, TraversalId, TraversalWarpTarget);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

	AddStatusTag(MyGameplayTags::Status_Channeling);
	SetupTraversalCamera(TraversalTopZ);

	// MOVE_Walking�� ���� ��Ʈ ����� ������. 
	// PhysWalking�� �ӵ��� MoveAlongFloor�� �ѱ�鼭 �߷� ������ ������ �����Ѵ�
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);

	TraversalIgnoredActors.Reset();
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		for (AActor* Obstacle : ObstacleActors)
		{
			// ���� ��� ������ ���� ���� �������� �ݵ�� �׷����� �ʴ�.
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
	const FVector& ContactPoint, const FRotator& ApproachRotation, FTransform& OutTarget) const
{
	// �����ص� ȣ��ΰ� �� �� �ֵ��� �������� ���� �־� �д�.
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

	// ���� ������ ��Ƽ���̿��� �д´�. �ð��� �ϵ��ڵ����� �����Ƿ�
	// �����Ϳ��� â�� �ű�� ���ĵ� ���� �����δ�.
	TArray<FMotionWarpingWindowData> Windows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, WarpTargetName, Windows);
	if (Windows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: %s has no %s window, using the contact point as is"),
			*GetNameSafe(Montage), *WarpTargetName.ToString());
		return false;
	}

	const int32 HandBoneIndex = GetMesh()->GetBoneIndex(TraversalHandBoneName);
	if (HandBoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: bone %s not found, using the contact point as is"), *TraversalHandBoneName.ToString());
		return false;
	}

	// AMyWeapon::BuildBoneContainer�� ���� ������� �����̳ʸ� ���� �����.
	// RefSkeleton �����̶� LOD�� required bones�� �����ϴ�.
	const FReferenceSkeleton& RefSkel = SkelMesh->GetRefSkeleton();
	TArray<FBoneIndexType> RequiredBones;

	int32 Current = HandBoneIndex;
	while (Current != INDEX_NONE)
	{
		RequiredBones.AddUnique(static_cast<FBoneIndexType>(Current));
		Current = RefSkel.GetParentIndex(Current);
	}
	RequiredBones.Sort();

	FBoneContainer BoneContainer;
	BoneContainer.InitializeTo(RequiredBones, UE::Anim::FCurveFilterSettings(), *SkelMesh);

	FCSPose<FCompactPose> CSPose;
	UMotionWarpingUtilities::ExtractComponentSpacePose(Montage, BoneContainer, Windows[0].EndTime, false, CSPose);

	// ������ Bone ���� ����Ʈ�� FCompactPoseBoneIndex(1)�� �ϵ��ڵ��ؼ�
	// ��Ʈ ���� �ڽ��� �ƴ� ������ �� �� ����. ���� ����Ʈ �ε����� ��ȯ�Ѵ�.
	const FCompactPoseBoneIndex RootCompact = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(0));
	const FCompactPoseBoneIndex HandCompact = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandBoneIndex));
	if (RootCompact.GetInt() == INDEX_NONE || HandCompact.GetInt() == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Traversal: compact pose index lookup failed, using the contact point as is"));
		return false;
	}

	const FTransform MeshToActor = FTransform(GetBaseRotationOffset());
	const FTransform RootBoneToActor = CSPose.GetComponentSpaceTransform(RootCompact) * MeshToActor;
	const FTransform HandBoneToActor = CSPose.GetComponentSpaceTransform(HandCompact) * MeshToActor;
	const FVector HandInRootSpace = RootBoneToActor.InverseTransformPosition(HandBoneToActor.GetLocation());
	const FVector HandOffsetInWorldSpace = ApproachRotation.RotateVector(RootBoneToActor.TransformVector(HandInRootSpace));
	OutTarget = FTransform(ApproachRotation, ContactPoint - HandOffsetInWorldSpace);

	return true;
}

void AMyPlayerCharacter::OnTraversalWarpEnded(const FName& WarpTargetName)
{
	PendingTraversalWarpIds.Remove(ActiveTraversalId);
	RemoveStatusTag(MyGameplayTags::Status_Channeling);


	if (WarpTargetName == VaultMoveWarpTarget)
	{
		OnVaultTraversalEnded();
	}

	if (UMotionWarpingComponent* WarpingComponent = GetMotionWarpingComponent())
	{
		WarpingComponent->RemoveAllWarpTargets();
	}
}

void AMyPlayerCharacter::SetupTraversalCamera(float TraversalTopZ)
{
	if (CameraBoom == nullptr)
	{
		return;
	}

	CachedCameraBoomTargetOffset = CameraBoom->TargetOffset;

	constexpr float CameraClearanceMargin = 10.f;
	const float CurrentArmOriginZ = CameraBoom->GetComponentLocation().Z + CachedCameraBoomTargetOffset.Z;
	const float SafeArmOriginZ = TraversalTopZ + CameraBoom->ProbeSize + CameraClearanceMargin;
	const float RequiredHeightOffset = FMath::Max(0.f, SafeArmOriginZ - CurrentArmOriginZ);
	CameraBoom->TargetOffset = CachedCameraBoomTargetOffset + RequiredHeightOffset * FVector::UpVector;
}

void AMyPlayerCharacter::RestoreTraversalCamera()
{
	if (CameraBoom != nullptr)
	{
		CameraBoom->TargetOffset = CachedCameraBoomTargetOffset;
	}
}

void AMyPlayerCharacter::RestoreTraversalPhysics()
{
	for (const TWeakObjectPtr<AActor>& IgnoredActor : TraversalIgnoredActors)
	{
		if (AActor* Obstacle = IgnoredActor.Get())
		{
			GetCapsuleComponent()->IgnoreActorWhenMoving(Obstacle, false);
		}
	}

	TraversalIgnoredActors.Reset();

	// �ٸ� ������ ���� ����Ǿ����� Ȯ��
	if (GetCharacterMovement()->MovementMode == MOVE_Flying) 
	{
		GetCharacterMovement()->SetDefaultMovementMode();
	}

	RestoreTraversalCamera();
}

void AMyPlayerCharacter::OnTraversalMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 TraversalId, FName TraversalWarpTarget)
{
	if (PendingTraversalWarpIds.Contains(TraversalId))
	{
		OnTraversalWarpEnded(TraversalWarpTarget);
	}
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
