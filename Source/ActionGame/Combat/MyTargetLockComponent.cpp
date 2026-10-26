#include "Combat/MyTargetLockComponent.h"

#include "Attributes/MyHealthComponent.h"
#include "Camera/CameraComponent.h"
#include "Characters/MyEnemyCharacter.h"
#include "Characters/MyPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

UMyTargetLockComponent::UMyTargetLockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UMyTargetLockComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<AMyPlayerCharacter>(GetOwner());
	if (!IsValid(OwnerCharacter))
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
	{
		Mesh->AddTickPrerequisiteComponent(this);
	}

	if (USpringArmComponent* CameraBoom = OwnerCharacter->GetCameraBoom())
	{
		CameraBoom->AddTickPrerequisiteComponent(this);
	}
}

void UMyTargetLockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(OwnerCharacter) || !TargetLockContext.bIsTargetLocked)
	{
		SetComponentTickEnabled(false);
		return;
	}

	UpdateTargetSwitchInputState();

	if (!ValidateLockedTarget())
	{
		ReleaseTargetLock();
		return;
	}

	// 시야는 일정 시간 이상 연속으로 가려졌을 때만 해제한다
	if (HasLineOfSightToTarget(TargetLockContext.LockedTarget.Get()))
	{
		LockedTargetLineOfSightLostTime = 0.0f;
	}
	else
	{
		LockedTargetLineOfSightLostTime += DeltaTime;
		if (LockedTargetLineOfSightLostTime >= LockedTargetLineOfSightLossDuration)
		{
			ReleaseTargetLock();
			return;
		}
	}

	if (OwnerCharacter->IsAirborneFromJumpInput())
	{
		// 입력 점프 중에는 경로를 검사하지 않고, 착지한 첫 틱에 바로 검사한다
		LockedTargetReachabilityTimeRemaining = 0.0f;
	}
	else
	{
		LockedTargetReachabilityTimeRemaining -= DeltaTime;
		if (LockedTargetReachabilityTimeRemaining <= 0.0f)
		{
			LockedTargetReachabilityTimeRemaining = LockedTargetReachabilityCheckInterval;

			const AMyEnemyCharacter* LockedTarget = Cast<AMyEnemyCharacter>(TargetLockContext.LockedTarget.Get());
			if (!IsGroundWarpReachable(LockedTarget))
			{
				ReleaseTargetLock();
				return;
			}
		}
	}

	UpdateTargetLockRotations();
	UpdateTargetLockCamera(DeltaTime);
}

void UMyTargetLockComponent::ToggleTargetLock()
{
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	if (TargetLockContext.bIsTargetLocked)
	{
		ReleaseTargetLock();
		return;
	}

	TryAcquireTarget();
}

void UMyTargetLockComponent::HandleTargetSwitchInput(const FVector2D& LookInput)
{
	UWorld* World = GetWorld();
	if (!TargetLockContext.bIsTargetLocked || World == nullptr)
	{
		return;
	}

	const float HorizontalInput = LookInput.X;
	if (FMath::Abs(HorizontalInput) < TargetSwitchMouseInputEpsilon)
	{
		return;
	}

	LastMeaningfulMouseInputTime = World->GetTimeSeconds();
	if (!bTargetSwitchInputReady)
	{
		return;
	}

	const float InputDirection = FMath::Sign(HorizontalInput);
	if (!FMath::IsNearlyZero(TargetSwitchInputAccumulator) &&
		FMath::Sign(TargetSwitchInputAccumulator) != InputDirection)
	{
		TargetSwitchInputAccumulator = 0.0f;
	}

	TargetSwitchInputAccumulator += HorizontalInput;
	if (FMath::Abs(TargetSwitchInputAccumulator) < TargetSwitchInputThreshold)
	{
		return;
	}

	const float SwitchDirection = FMath::Sign(TargetSwitchInputAccumulator);
	TargetSwitchInputAccumulator = 0.0f;
	bTargetSwitchInputReady = false;
	TrySwitchTarget(SwitchDirection);
}

bool UMyTargetLockComponent::TryAcquireTarget()
{
	if (AMyEnemyCharacter* NewTarget = FindBestTarget())
	{
		SetLockedTarget(NewTarget);
		ResetTargetSwitchInputState();
		return true;
	}

	return false;
}

bool UMyTargetLockComponent::TrySwitchTarget(float Direction)
{
	AActor* CurrentTarget = TargetLockContext.LockedTarget.Get();
	if (!IsValid(CurrentTarget) || FMath::IsNearlyZero(Direction))
	{
		return false;
	}

	TArray<AMyEnemyCharacter*> Candidates;
	FindTargetCandidates(Candidates, ETargetLockValidationMode::TargetSwitch, CurrentTarget);
	if (Candidates.IsEmpty())
	{
		return false;
	}

	const USpringArmComponent* CameraBoom = OwnerCharacter->GetCameraBoom();
	const FVector ViewLocation = CameraBoom != nullptr ? CameraBoom->GetComponentLocation() : OwnerCharacter->GetActorLocation();
	const FVector CurrentDirection = (GetTargetLockPoint(CurrentTarget) - ViewLocation).GetSafeNormal2D();
	if (CurrentDirection.IsNearlyZero())
	{
		return false;
	}

	const float CurrentYaw = CurrentDirection.Rotation().Yaw;
	AMyEnemyCharacter* BestTarget = nullptr;
	float BestYawDistance = MAX_flt;

	for (AMyEnemyCharacter* Candidate : Candidates)
	{
		const FVector CandidateDirection = (GetTargetLockPoint(Candidate) - ViewLocation).GetSafeNormal2D();
		if (CandidateDirection.IsNearlyZero())
		{
			continue;
		}

		const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, CandidateDirection.Rotation().Yaw);
		if (DeltaYaw * Direction <= 0.0f)
		{
			continue;
		}

		const float YawDistance = FMath::Abs(DeltaYaw);
		if (YawDistance < BestYawDistance)
		{
			BestYawDistance = YawDistance;
			BestTarget = Candidate;
		}
	}

	if (!BestTarget)
	{
		return false;
	}

	SetLockedTarget(BestTarget);
	return true;
}

void UMyTargetLockComponent::SetLockedTarget(AActor* NewTarget)
{
	TargetLockContext.LockedTarget = NewTarget;
	TargetLockContext.bIsTargetLocked = true;
	LockedTargetReachabilityTimeRemaining = LockedTargetReachabilityCheckInterval;
	LockedTargetLineOfSightLostTime = 0.0f;

	UpdateTargetLockRotations();
	SetComponentTickEnabled(true);
}

void UMyTargetLockComponent::ReleaseTargetLock()
{
	TargetLockContext = FTargetLockContext();
	LockedTargetReachabilityTimeRemaining = 0.0f;
	LockedTargetLineOfSightLostTime = 0.0f;
	ResetTargetSwitchInputState();
	SetComponentTickEnabled(false);
}

void UMyTargetLockComponent::UpdateTargetSwitchInputState()
{
	if (bTargetSwitchInputReady || GetWorld() == nullptr)
	{
		return;
	}

	const float IdleTime = GetWorld()->GetTimeSeconds() - LastMeaningfulMouseInputTime;
	if (IdleTime >= TargetSwitchMouseIdleDuration)
	{
		ResetTargetSwitchInputState();
	}
}

void UMyTargetLockComponent::ResetTargetSwitchInputState()
{
	TargetSwitchInputAccumulator = 0.0f;
	LastMeaningfulMouseInputTime = -1000000.0f;
	bTargetSwitchInputReady = true;
}

bool UMyTargetLockComponent::ValidateLockedTarget() const
{
	const AMyEnemyCharacter* LockedTarget = Cast<AMyEnemyCharacter>(TargetLockContext.LockedTarget.Get());
	return IsValidTargetCandidate(LockedTarget, ETargetLockValidationMode::LockedTargetMaintenance);
}

bool UMyTargetLockComponent::IsValidTargetCandidate(const AMyEnemyCharacter* Candidate,
	ETargetLockValidationMode ValidationMode) const
{
	if (!IsValid(OwnerCharacter) || !IsValid(Candidate))
	{
		return false;
	}

	if (const UMyHealthComponent* CandidateHealthComponent = Candidate->GetHealthComponent())
	{
		if (CandidateHealthComponent->IsDead())
		{
			return false;
		}
	}

	if (FVector::DistSquared(OwnerCharacter->GetActorLocation(), Candidate->GetActorLocation()) > FMath::Square(TargetLockMaxDistance))
	{
		return false;
	}

	if (ValidationMode == ETargetLockValidationMode::InitialAcquire && !IsTargetWithinInitialYawAngle(Candidate))
	{
		return false;
	}

	if (ValidationMode != ETargetLockValidationMode::LockedTargetMaintenance && !HasLineOfSightToTarget(Candidate))
	{
		return false;
	}

	if (ValidationMode != ETargetLockValidationMode::LockedTargetMaintenance && !IsGroundWarpReachable(Candidate))
	{
		return false;
	}

	return true;
}

bool UMyTargetLockComponent::HasLineOfSightToTarget(const AActor* Target) const
{
	if (!IsValid(OwnerCharacter) || !IsValid(Target) || GetWorld() == nullptr)
	{
		return false;
	}

	const UCameraComponent* FollowCamera = OwnerCharacter->GetFollowCamera();
	const FVector TraceStart = FollowCamera != nullptr ? FollowCamera->GetComponentLocation() : OwnerCharacter->GetActorLocation();
	const FVector TraceEnd = GetTargetLockPoint(Target);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TargetLockLineOfSight), false, OwnerCharacter);
	FHitResult HitResult;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	return !bBlocked || HitResult.GetActor() == Target;
}

bool UMyTargetLockComponent::IsTargetWithinInitialYawAngle(const AActor* Target) const
{
	if (!IsValid(OwnerCharacter) || !IsValid(Target))
	{
		return false;
	}

	const UCameraComponent* FollowCamera = OwnerCharacter->GetFollowCamera();
	const FVector ViewLocation = FollowCamera != nullptr ? FollowCamera->GetComponentLocation() : OwnerCharacter->GetActorLocation();
	const FVector ViewForward = (FollowCamera != nullptr
		? FollowCamera->GetForwardVector()
		: OwnerCharacter->GetActorForwardVector()).GetSafeNormal2D();
	const FVector DirectionToTarget = (GetTargetLockPoint(Target) - ViewLocation).GetSafeNormal2D();
	if (DirectionToTarget.IsNearlyZero())
	{
		return true;
	}
	if (ViewForward.IsNearlyZero())
	{
		return false;
	}

	const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
		ViewForward.Rotation().Yaw,
		DirectionToTarget.Rotation().Yaw));
	return YawDelta <= InitialTargetLockMaxYawAngle;
}

bool UMyTargetLockComponent::IsGroundWarpReachable(const AMyEnemyCharacter* Target) const
{
	UWorld* World = GetWorld();
	if (!IsValid(OwnerCharacter) || !IsValid(Target) || World == nullptr)
	{
		return false;
	}

	const UCapsuleComponent* OwnerCapsule = OwnerCharacter->GetCapsuleComponent();
	const UCapsuleComponent* TargetCapsule = Target->GetCapsuleComponent();
	const UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();
	if (!OwnerCapsule || !TargetCapsule || !MovementComponent)
	{
		return false;
	}

	const FVector OwnerLocation = OwnerCapsule->GetComponentLocation();
	const FVector TargetLocation = TargetCapsule->GetComponentLocation();
	FVector DirectionToTarget = TargetLocation - OwnerLocation;
	DirectionToTarget.Z = 0.0f;
	const float CurrentDistance = DirectionToTarget.Size();
	if (!DirectionToTarget.Normalize())
	{
		return true;
	}

	const float CapsuleContactDistance = OwnerCapsule->GetScaledCapsuleRadius()
		+ TargetCapsule->GetScaledCapsuleRadius();
	const float PathDistance = FMath::Max(0.0f, CurrentDistance - CapsuleContactDistance);
	const FVector Destination = OwnerLocation + DirectionToTarget * PathDistance;
	const float SampleSpacing = FMath::Max(1.0f, GroundPathSampleSpacing);
	const int32 SampleCount = FMath::Max(1, FMath::CeilToInt(PathDistance / SampleSpacing));

	FCollisionQueryParams FloorQueryParams(SCENE_QUERY_STAT(TargetLockGroundProbe), false, OwnerCharacter);
	FloorQueryParams.AddIgnoredActor(Target);
	FCollisionObjectQueryParams EnvironmentObjectQueryParams;
	EnvironmentObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	EnvironmentObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams MovementQueryParams(SCENE_QUERY_STAT(TargetLockGroundPath), false, OwnerCharacter);
	MovementQueryParams.AddIgnoredActor(Target);

	constexpr float CapsuleClearance = 2.0f;
	const float CapsuleRadius = FMath::Max(1.0f, OwnerCapsule->GetScaledCapsuleRadius() - CapsuleClearance);
	const float CapsuleHalfHeight = FMath::Max(CapsuleRadius,
		OwnerCapsule->GetScaledCapsuleHalfHeight() - CapsuleClearance);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	const auto DrawSampleCapsule = [this, World, CapsuleHalfHeight, CapsuleRadius](const FVector& Location, const FColor& Color)
	{
		if (bDrawReachabilityDebug)
		{
			DrawDebugCapsule(World, Location, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity,
				Color, false, ReachabilityDebugDuration, 0, 1.0f);
		}
	};
	const auto DrawSweepResult = [this, World, &DrawSampleCapsule](const FVector& Start, const FVector& End,
		const FColor& Color, const FHitResult& Hit)
	{
		if (!bDrawReachabilityDebug)
		{
			return;
		}

		DrawDebugLine(World, Start, End, Color, false, ReachabilityDebugDuration, 0, 2.0f);
		DrawSampleCapsule(Start, Color);
		DrawSampleCapsule(End, Color);
		if (Hit.bBlockingHit)
		{
			DrawDebugPoint(World, Hit.ImpactPoint, 12.0f, Color, false, ReachabilityDebugDuration, 0);
		}
	};

	// CMC FindFloor처럼 캡슐을 아래로 쓸어 바닥을 찾는다. 중심이 턱 밖이어도 모서리에 걸친 위치를 바닥으로 인정한다
	const auto FindWalkableFloor = [&](const FVector& StartCenter, float SweepDistance,
		FVector& OutCapsuleCenter, float& OutFloorZ) -> bool
	{
		const FVector EndCenter = StartCenter - FVector::UpVector * SweepDistance;
		FHitResult FloorHit;
		const bool bHasFloor = World->SweepSingleByObjectType(
			FloorHit,
			StartCenter,
			EndCenter,
			FQuat::Identity,
			EnvironmentObjectQueryParams,
			CapsuleShape,
			FloorQueryParams);
		if (!bHasFloor || FloorHit.bStartPenetrating)
		{
			DrawSweepResult(StartCenter, EndCenter, FColor::Red, FloorHit);
			return false;
		}

		bool bIsWalkableFloor = MovementComponent->IsWalkable(FloorHit);
		if (!bIsWalkableFloor)
		{
			// 모서리 접촉은 법선이 기울어 나올 수 있으므로 접촉점 바로 바깥의 실제 면 법선으로 다시 판정한다
			const FVector OutwardDirection = (FloorHit.ImpactPoint - FloorHit.Location).GetSafeNormal2D();
			const FVector SurfaceProbeLocation = FloorHit.ImpactPoint + OutwardDirection * CapsuleClearance;
			FHitResult SurfaceHit;
			const bool bHasSurface = World->LineTraceSingleByObjectType(
				SurfaceHit,
				SurfaceProbeLocation + FVector::UpVector * CapsuleClearance,
				SurfaceProbeLocation - FVector::UpVector * CapsuleClearance,
				EnvironmentObjectQueryParams,
				FloorQueryParams);
			bIsWalkableFloor = bHasSurface && MovementComponent->IsWalkable(SurfaceHit);
		}

		DrawSweepResult(StartCenter, FloorHit.Location,
			bIsWalkableFloor ? FColor::Green : FColor::Orange, FloorHit);
		if (!bIsWalkableFloor)
		{
			return false;
		}

		OutCapsuleCenter = FloorHit.Location + FVector::UpVector * CapsuleClearance;
		OutFloorZ = FloorHit.ImpactPoint.Z;
		return true;
	};

	FVector PreviousCapsuleCenter = OwnerLocation;
	float PreviousFloorZ = OwnerLocation.Z - OwnerCapsule->GetScaledCapsuleHalfHeight();
	if (MovementComponent->CurrentFloor.IsWalkableFloor())
	{
		// 캡슐이 바닥에서 떠 있는 거리만큼 내려 실제 바닥 높이에서 시작한다
		PreviousFloorZ -= MovementComponent->CurrentFloor.GetDistanceToFloor();
	}
	const float MaximumStepHeight = MovementComponent->MaxStepHeight + CapsuleClearance;
	const float UpperHemisphereCenterOffset = CapsuleHalfHeight - CapsuleRadius;

	for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
		const FVector SampleLocation = FMath::Lerp(OwnerLocation, Destination, Alpha);
		// 직전 높이를 유지한 채 샘플 XY까지 수평으로 이동한다
		FVector MoveEnd(SampleLocation.X, SampleLocation.Y, PreviousCapsuleCenter.Z);
		float FloorSweepDistance = MaximumStepHeight + CapsuleClearance;

		FHitResult PathHit;
		const bool bPathBlocked = World->SweepSingleByObjectType(
			PathHit,
			PreviousCapsuleCenter,
			MoveEnd,
			FQuat::Identity,
			EnvironmentObjectQueryParams,
			CapsuleShape,
			MovementQueryParams);
		DrawSweepResult(PreviousCapsuleCenter, MoveEnd,
			bPathBlocked ? FColor::Yellow : FColor::Green, PathHit);
		if (bPathBlocked)
		{
			// CMC StepUp과 같이 발바닥보다 높고 위쪽 반구 중심보다 낮은 곳에 닿았을 때만 올라서기를 시도한다
			const float ImpactZ = PathHit.ImpactPoint.Z;
			// CMC StepUp처럼 바닥 접촉점이 아니라 캡슐 아래 끝과 비교한다. 모서리에 걸쳐 처진 경우에도 올라설 수 있다
			const float PreviousCapsuleBaseZ = PreviousCapsuleCenter.Z - CapsuleHalfHeight - CapsuleClearance;
			const bool bCanTryStepUp = !PathHit.bStartPenetrating
				&& ImpactZ > PreviousCapsuleBaseZ
				&& ImpactZ <= PreviousCapsuleCenter.Z + UpperHemisphereCenterOffset;
			if (!bCanTryStepUp)
			{
				return false;
			}

			// 위로: 머리 위가 막히면 닿은 높이까지만 올라간다
			const FVector StepUpTarget = PreviousCapsuleCenter + FVector::UpVector * MovementComponent->MaxStepHeight;
			FHitResult StepUpHit;
			World->SweepSingleByObjectType(
				StepUpHit,
				PreviousCapsuleCenter,
				StepUpTarget,
				FQuat::Identity,
				EnvironmentObjectQueryParams,
				CapsuleShape,
				MovementQueryParams);
			if (StepUpHit.bStartPenetrating)
			{
				return false;
			}
			const FVector RaisedCapsuleCenter = StepUpHit.bBlockingHit ? StepUpHit.Location : StepUpTarget;
			DrawSweepResult(PreviousCapsuleCenter, RaisedCapsuleCenter, FColor::Cyan, StepUpHit);

			// 앞으로: 올라간 높이에서도 막히면 넘을 수 없는 벽이다
			MoveEnd = FVector(SampleLocation.X, SampleLocation.Y, RaisedCapsuleCenter.Z);
			FHitResult StepForwardHit;
			const bool bStepForwardBlocked = World->SweepSingleByObjectType(
				StepForwardHit,
				RaisedCapsuleCenter,
				MoveEnd,
				FQuat::Identity,
				EnvironmentObjectQueryParams,
				CapsuleShape,
				MovementQueryParams);
			DrawSweepResult(RaisedCapsuleCenter, MoveEnd,
				bStepForwardBlocked ? FColor::Red : FColor::Cyan, StepForwardHit);
			if (bStepForwardBlocked)
			{
				return false;
			}

			// 아래로: 올라간 만큼 더 내려가며 바닥을 찾는다
			FloorSweepDistance += RaisedCapsuleCenter.Z - PreviousCapsuleCenter.Z;
		}

		FVector CapsuleCenter;
		float FloorZ = 0.0f;
		if (!FindWalkableFloor(MoveEnd, FloorSweepDistance, CapsuleCenter, FloorZ))
		{
			return false;
		}

		const float FloorHeightDelta = FloorZ - PreviousFloorZ;
		DrawSampleCapsule(CapsuleCenter,
			FMath::Abs(FloorHeightDelta) <= MaximumStepHeight ? FColor::Blue : FColor::Red);
		if (FMath::Abs(FloorHeightDelta) > MaximumStepHeight)
		{
			return false;
		}

		PreviousCapsuleCenter = CapsuleCenter;
		PreviousFloorZ = FloorZ;
	}

	const float TargetFeetZ = TargetLocation.Z - TargetCapsule->GetScaledCapsuleHalfHeight();
	if (FMath::Abs(TargetFeetZ - PreviousFloorZ) > MaximumStepHeight)
	{
		return false;
	}

	return true;
}

void UMyTargetLockComponent::FindTargetCandidates(TArray<AMyEnemyCharacter*>& OutCandidates,
	ETargetLockValidationMode ValidationMode, const AActor* ExcludedTarget) const
{
	OutCandidates.Reset();
	if (!IsValid(OwnerCharacter) || GetWorld() == nullptr || TargetLockMaxDistance <= 0.0f)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TargetLockCandidates), false, OwnerCharacter);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> OverlapResults;
	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		OwnerCharacter->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(TargetLockMaxDistance),
		QueryParams
	);

	TSet<AActor*> ProcessedActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* OverlappedActor = OverlapResult.GetActor();
		if (!IsValid(OverlappedActor) || OverlappedActor == ExcludedTarget || ProcessedActors.Contains(OverlappedActor))
		{
			continue;
		}
		ProcessedActors.Add(OverlappedActor);

		AMyEnemyCharacter* Candidate = Cast<AMyEnemyCharacter>(OverlappedActor);
		UE_LOG(LogTemp, Display, TEXT("Target Lock: overlap actor before validation=%s"), *GetNameSafe(OverlappedActor));
		if (IsValidTargetCandidate(Candidate, ValidationMode))
		{
			UE_LOG(LogTemp, Display, TEXT("Target Lock: overlap actor=%s"), *GetNameSafe(OverlappedActor));
			OutCandidates.Add(Candidate);
		}
	}
}

AMyEnemyCharacter* UMyTargetLockComponent::FindBestTarget(const AActor* ExcludedTarget) const
{
	TArray<AMyEnemyCharacter*> Candidates;
	FindTargetCandidates(Candidates, ETargetLockValidationMode::InitialAcquire, ExcludedTarget);

	const UCameraComponent* FollowCamera = OwnerCharacter->GetFollowCamera();
	const FVector ViewLocation = FollowCamera != nullptr ? FollowCamera->GetComponentLocation() : OwnerCharacter->GetActorLocation();
	const FVector ViewForward = (FollowCamera != nullptr
		? FollowCamera->GetForwardVector()
		: OwnerCharacter->GetActorForwardVector()).GetSafeNormal2D();
	AMyEnemyCharacter* BestTarget = nullptr;
	float BestYawDelta = MAX_flt;
	float BestDistanceSquared = MAX_flt;

	for (AMyEnemyCharacter* Candidate : Candidates)
	{
		const FVector ToTarget = GetTargetLockPoint(Candidate) - ViewLocation;
		const FVector DirectionToTarget = ToTarget.GetSafeNormal2D();
		if (DirectionToTarget.IsNearlyZero())
		{
			continue;
		}

		const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
			ViewForward.Rotation().Yaw,
			DirectionToTarget.Rotation().Yaw));
		const float DistanceSquared = ToTarget.SizeSquared();
		const bool bIsMoreCentered = YawDelta < BestYawDelta - UE_KINDA_SMALL_NUMBER;
		const bool bIsSameCenter = FMath::IsNearlyEqual(YawDelta, BestYawDelta);

		if (bIsMoreCentered || (bIsSameCenter && DistanceSquared < BestDistanceSquared))
		{
			BestYawDelta = YawDelta;
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

FVector UMyTargetLockComponent::GetTargetLockPoint(const AActor* Target) const
{
	return IsValid(Target)
		? Target->GetActorLocation() + FVector::UpVector * TargetLockPointHeightOffset
		: FVector::ZeroVector;
}

void UMyTargetLockComponent::UpdateTargetLockRotations()
{
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	AActor* LockedTarget = TargetLockContext.LockedTarget.Get();
	if (!IsValid(LockedTarget))
	{
		return;
	}

	FVector CharacterDirection = LockedTarget->GetActorLocation() - OwnerCharacter->GetActorLocation();
	CharacterDirection.Z = 0.0f;
	if (!CharacterDirection.IsNearlyZero())
	{
		const float TargetYaw = CharacterDirection.Rotation().Yaw;
		TargetLockContext.CharacterTargetRotation = FRotator(0.0f, TargetYaw, 0.0f);
	}

	const USpringArmComponent* CameraBoom = OwnerCharacter->GetCameraBoom();
	if (CameraBoom == nullptr)
	{
		return;
	}

	const FVector CameraOrigin = CameraBoom->GetComponentLocation() + CameraBoom->TargetOffset;
	const FVector CameraDirection = GetTargetLockPoint(LockedTarget) - CameraOrigin;
	if (!CameraDirection.IsNearlyZero())
	{
		FRotator CameraTargetRotation = CameraDirection.Rotation();
		CameraTargetRotation.Pitch = FMath::Clamp(
			CameraTargetRotation.Pitch + TargetLockCameraPitchOffset,
			TargetLockCameraMinPitch,
			TargetLockCameraMaxPitch
		);
		CameraTargetRotation.Roll = 0.0f;

		TargetLockContext.CameraTargetRotation = CameraTargetRotation;
	}
}

void UMyTargetLockComponent::UpdateTargetLockCamera(float DeltaSeconds)
{
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	AController* CharacterController = OwnerCharacter->GetController();
	if (!CharacterController)
	{
		return;
	}

	const FRotator NewControlRotation = FMath::RInterpTo(
		CharacterController->GetControlRotation(),
		TargetLockContext.CameraTargetRotation,
		DeltaSeconds,
		TargetLockCameraInterpSpeed
	);
	CharacterController->SetControlRotation(NewControlRotation);
}
