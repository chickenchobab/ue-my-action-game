#pragma once

#include "Components/ActorComponent.h"
#include "Combat/MyTargetLockTypes.h"
#include "MyTargetLockComponent.generated.h"

class AMyEnemyCharacter;
class AMyPlayerCharacter;

enum class ETargetLockValidationMode : uint8
{
	InitialAcquire,
	TargetSwitch,
	LockedTargetMaintenance
};

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class ACTIONGAME_API UMyTargetLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMyTargetLockComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	FORCEINLINE const FTargetLockContext& GetTargetLockContext() const { return TargetLockContext; }
	FORCEINLINE bool IsTargetLocked() const { return TargetLockContext.bIsTargetLocked; }

	void ToggleTargetLock();
	void HandleTargetSwitchInput(const FVector2D& LookInput);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Target Lock", meta = (ClampMin = "0.0"))
	float TargetLockMaxDistance = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float InitialTargetLockMaxYawAngle = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock", meta = (ClampMin = "0.0"))
	float TargetLockPointHeightOffset = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock", meta = (ClampMin = "0.0"))
	float LockedTargetLineOfSightLossDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Reachability", meta = (ClampMin = "1.0"))
	float GroundPathSampleSpacing = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Reachability", meta = (ClampMin = "0.0"))
	float LockedTargetReachabilityCheckInterval = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Debug")
	bool bDrawReachabilityDebug = true;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Debug", meta = (ClampMin = "0.0"))
	float ReachabilityDebugDuration = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Camera", meta = (ClampMin = "0.0"))
	float TargetLockCameraInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Camera", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float TargetLockCameraPitchOffset = -70.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Camera", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float TargetLockCameraMinPitch = -60.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Camera", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float TargetLockCameraMaxPitch = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Input", meta = (ClampMin = "0.0"))
	float TargetSwitchInputThreshold = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Input", meta = (ClampMin = "0.0"))
	float TargetSwitchMouseInputEpsilon = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Target Lock|Input", meta = (ClampMin = "0.0"))
	float TargetSwitchMouseIdleDuration = 0.1f;

private:
	bool TryAcquireTarget();
	bool TrySwitchTarget(float Direction);
	void SetLockedTarget(AActor* NewTarget);
	void ReleaseTargetLock();
	void UpdateTargetSwitchInputState();
	void ResetTargetSwitchInputState();
	bool ValidateLockedTarget() const;
	bool IsValidTargetCandidate(const AMyEnemyCharacter* Candidate, ETargetLockValidationMode ValidationMode) const;
	bool HasLineOfSightToTarget(const AActor* Target) const;
	bool IsTargetWithinInitialYawAngle(const AActor* Target) const;
	bool IsGroundWarpReachable(const AMyEnemyCharacter* Target) const;
	void FindTargetCandidates(TArray<AMyEnemyCharacter*>& OutCandidates, ETargetLockValidationMode ValidationMode,
		const AActor* ExcludedTarget = nullptr) const;
	AMyEnemyCharacter* FindBestTarget(const AActor* ExcludedTarget = nullptr) const;
	FVector GetTargetLockPoint(const AActor* Target) const;
	void UpdateTargetLockRotations();
	void UpdateTargetLockCamera(float DeltaSeconds);

private:
	UPROPERTY(Transient)
	TObjectPtr<AMyPlayerCharacter> OwnerCharacter;

	UPROPERTY(Transient)
	FTargetLockContext TargetLockContext;

	float TargetSwitchInputAccumulator = 0.0f;
	float LastMeaningfulMouseInputTime = -1000000.0f;
	float LockedTargetReachabilityTimeRemaining = 0.0f;
	float LockedTargetLineOfSightLostTime = 0.0f;
	bool bTargetSwitchInputReady = true;
};
