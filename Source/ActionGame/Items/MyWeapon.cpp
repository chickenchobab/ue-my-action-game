// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/MyWeapon.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "Skills/SkillInstance_Attack.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Misc/MemStack.h"

static bool bShouldDrawSocketSphere = false;
static inline void DrawSocketSphere(UWorld* World, FVector Location, float Radius, FColor Color)
{
	if (bShouldDrawSocketSphere)
	{
		DrawDebugSphere(World, Location, Radius, 16, Color, true, 0.1f, 0, 2.0f);
	}
}

static FName GripSocketName = FName(TEXT("WeaponSocket"));

AMyWeapon::AMyWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.SetTickFunctionEnable(false);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetSimulatePhysics(false);

	DefaultAttachedSocket = GripSocketName;

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->SetupAttachment(WeaponMesh);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AMyWeapon::Equip(USceneComponent* NewParent, const FName& OverrideAttachedSocket)
{
	const FName& AttachedSocket = (OverrideAttachedSocket != NAME_None) ? OverrideAttachedSocket : DefaultAttachedSocket;

	const FAttachmentTransformRules AttachRules(
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		true
	);
	AttachToComponent(NewParent, AttachRules, AttachedSocket);

	if (USkeletalMeshComponent* MeshParent = Cast<USkeletalMeshComponent>(NewParent))
	{
		HitCheckDelegateHandle = MeshParent->OnTickPose.AddUObject(this, &ThisClass::HitCheck);
		if (AnimLayerClass)
		{
			MeshParent->LinkAnimClassLayers(AnimLayerClass);
		}
	}

	SetOwner(NewParent->GetOwner());

	PickupCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bIsEquipped = true;
}

void AMyWeapon::UnEquip()
{
	if (USkeletalMeshComponent* MeshParent = Cast<USkeletalMeshComponent>(RootComponent->GetAttachParent()))
	{
		MeshParent->OnTickPose.Remove(HitCheckDelegateHandle);
		MeshParent->UnlinkAnimClassLayers(AnimLayerClass);
	}
	
	HitCheckDelegateHandle.Reset();

	PrimaryActorTick.SetTickFunctionEnable(false);

	const FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	DetachFromActor(DetachRules);

	bIsAttacking = false;
	CurrentActiveSkill.Reset();
	HitCheckContext.Reset();

	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	bIsEquipped = false;
	SetOwner(nullptr);
}

void AMyWeapon::InitWeaponForAttack(USkillInstance_Attack* Skill, const UAnimMontage* SkillMontage)
{
	if (!IsValid(Skill) || !IsValid(SkillMontage))
	{
		CurrentActiveSkill.Reset();
		HitCheckContext.Reset();
		return;
	}

	CurrentActiveSkill = Skill;
	InitHitCheckContext(SkillMontage);
}

void AMyWeapon::OnAttackBegin()
{
	if (!bIsEquipped || !HitCheckContext.IsValid())
	{
		return;
	}

	bIsAttacking = true;
	HitCheckContext.OwnerMeshTransformLastFrame = HitCheckContext.OwnerMesh->GetComponentTransform();
}

void AMyWeapon::OnAttackEnd()
{
	bIsAttacking = false;

	// 인스턴스가 회수됐을 수 있으므로 유효성을 확인한다
	if (USkillInstance_Attack* ActiveSkill = CurrentActiveSkill.Get())
	{
		ActiveSkill->OnAttackEnd();
	}
	CurrentActiveSkill.Reset();
}

void AMyWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	WeaponSockets = WeaponMesh->GetAllSocketNames();
}

void AMyWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void AMyWeapon::HitCheck(USkinnedMeshComponent* MeshComp, float DeltaTime, bool bNeedsValidRootMotion)
{
	if (!bIsEquipped || !bIsAttacking || WeaponSockets.IsEmpty())
	{
		//UE_LOG(LogTemp, Display, TEXT("Tick return: Equip(%d) and Attacking(%d)"), bIsEquipped, bIsAttacking);
		return;
	}
	if (!HitCheckContext.IsValid())
	{
		//UE_LOG(LogTemp, Display, TEXT("Tick return: HitCheckContext invalid"));
		return;
	}

	for (int i = 0; i < 2; ++i)
	{
		DrawSocketSphere(GetWorld(), WeaponMesh->GetSocketLocation(WeaponSockets[i]), 5.0f, FColor::Red);
	}

	TArray<FSocketSamples> Samples;
	SampleSocketPositions(Samples);
	if (Samples.Num() < 2)
	{
		//UE_LOG(LogTemp, Display, TEXT("Tick return: Too few samples"));
		return;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bSkipNarrowPhase = true; // 엔진의 정밀 겹침 계산(narrow phase)은 생략한다.

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	int32 SubStepCount = Samples[0].Locations.Num() - 1;
	for (int32 Step = 0; Step < SubStepCount; ++Step)
	{
		FWeaponHitQuery Query;
		Query.Tip_Old = Samples[0].Locations[Step];
		Query.Tip_New = Samples[0].Locations[Step + 1];
		Query.Base_Old = Samples[1].Locations[Step];
		Query.Base_New = Samples[1].Locations[Step + 1];
		Query.Box = GetBoxFromHitQuery(Query); // Create an AABB that encloses all weapon sockets.

		TArray<FOverlapResult> Candidates;
		GetCandidatesByBoxOverlap(Candidates, Query.Box, QueryParams, ObjectQueryParams);

		for (const FOverlapResult& Candidate : Candidates)
		{
			AActor* CandidateActor = Candidate.GetActor();
			if (!IsValid(CandidateActor))
			{
				continue;
			}

			if (ACharacter* CandidateCharacter = Cast<ACharacter>(CandidateActor))
			{
				if (UCapsuleComponent* Capsule = CandidateCharacter->GetCapsuleComponent())
				{
					float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
					float Radius = Capsule->GetScaledCapsuleRadius();
					FVector CapsuleCenter = Capsule->GetComponentLocation();
					FVector UpVector = Capsule->GetUpVector();

					FVector CapsuleBase = CapsuleCenter - UpVector * (HalfHeight - Radius);
					FVector CapsuleTop = CapsuleCenter + UpVector * (HalfHeight - Radius);

					if (IntersectQuadWithCapsule(Query, CapsuleBase, CapsuleTop, Radius))
					{
						HitCheckContext.HitActorsThisFrame.AddUnique(CandidateActor);
					}
				}
			}
		}
	}

	if (!HitCheckContext.HitActorsThisFrame.IsEmpty())
	{
		TArray<AActor*> NewlyHitActors;
		for (AActor* HitActor : HitCheckContext.HitActorsThisFrame)
		{
			if (!HitCheckContext.SkillHandledActors.Contains(HitActor))
			{
				NewlyHitActors.Add(HitActor);
				HitCheckContext.SkillHandledActors.Add(HitActor);
			}
		}

		if (!NewlyHitActors.IsEmpty())
		{
			if (USkillInstance_Attack* ActiveSkill = CurrentActiveSkill.Get())
			{
				ActiveSkill->OnAttackHit(GetOwner(), NewlyHitActors);
			}
		}
		HitCheckContext.HitActorsThisFrame.Empty();
	}

	HitCheckContext.OwnerMeshTransformLastFrame = HitCheckContext.OwnerMesh->GetComponentTransform();
}

void AMyWeapon::SampleSocketPositions(TArray<FSocketSamples>& OutSamples)
{
	const FAnimMontageInstance* MontageInstance = HitCheckContext.MontageInstance;
	float PrevAnimTime = MontageInstance->GetPreviousPosition();
	float CurrAnimTime = MontageInstance->GetPosition();
	float DeltaMoved = MontageInstance->GetDeltaMoved();

	// 서브스텝 수를 애님 원본의 샘플링 프레임레이트로 정한다.
	// 게임 프레임 레이트가 떨어져도 애님 시간당 표본 밀도가 유지된다.
	// 원본 키 직접 접근(bForceUseRawData)은 에디터 전용이라 쓰지 않는다.
	
	float SamplingFrameRate = HitCheckContext.AnimSequence->GetSamplingFrameRate().AsDecimal();
	int32 AnimBasedSteps = FMath::Max(1, FMath::CeilToInt(DeltaMoved * SamplingFrameRate) * 3);
	int32 SubStepCount = FMath::Min(AnimBasedSteps, 16);

	OutSamples.SetNum(WeaponSockets.Num());
	for (int32 i = 0; i < WeaponSockets.Num(); ++i)
	{
		OutSamples[i].SocketName = WeaponSockets[i];
		OutSamples[i].Locations.Reserve(SubStepCount + 1);
	}

	// FMemStack에서 잡은 포즈/커브 메모리를 함수 종료 시 되감는다.
	FMemMark Mark(FMemStack::Get());

	for (int32 Step = 0; Step <= SubStepCount; ++Step)
	{
		float Alpha = (float)Step / (float)SubStepCount;
		// GetBonePose() does not account for the anim sequence's RateScale.
		float SampleTime = FMath::Lerp(PrevAnimTime, CurrAnimTime, Alpha) * HitCheckContext.AnimSequence->RateScale;

		FTransform SkelMeshToWorld;
		SkelMeshToWorld.Blend(HitCheckContext.OwnerMeshTransformLastFrame, HitCheckContext.OwnerMesh->GetComponentTransform(), Alpha);

		FCompactPose Pose;
		Pose.SetBoneContainer(&HitCheckContext.BoneContainer);
		FBlendedCurve Curve;
		UE::Anim::FStackAttributeContainer Attributes;
		FAnimationPoseData PoseData(Pose, Curve, Attributes);
		HitCheckContext.AnimSequence->GetBonePose(PoseData, FAnimExtractContext((double)SampleTime, true), false);

		FCSPose<FCompactPose> CSPose;
		CSPose.InitPose(Pose);
		FCompactPoseBoneIndex GripCompactIndex = HitCheckContext.BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(HitCheckContext.GripBoneIndex));
		const FTransform& GripBoneToSkelMesh = CSPose.GetComponentSpaceTransform(GripCompactIndex);

		for (int32 Socket = 0; Socket < WeaponSockets.Num(); ++Socket)
		{
			const FTransform SocketWorld = HitCheckContext.WeaponSocketToGripBone[Socket] * GripBoneToSkelMesh * SkelMeshToWorld;
			DrawSocketSphere(GetWorld(), SocketWorld.GetLocation(), 2.0f, FColor::Blue);
			OutSamples[Socket].Locations.Add(SocketWorld.GetLocation());
		}
	}
}

FBox AMyWeapon::GetBoxFromHitQuery(const FWeaponHitQuery& Query)
{
	FBox Box(ForceInit);
	Box += Query.Tip_Old;
	Box += Query.Tip_New;
	Box += Query.Base_Old;
	Box += Query.Base_New;

	Box = Box.ExpandBy(2.0f);
	return Box;
}

void AMyWeapon::GetCandidatesByBoxOverlap(TArray<FOverlapResult>& OutCandidates, const FBox& InBox, FCollisionQueryParams& QueryParams, FCollisionObjectQueryParams& ObjectQueryParams)
{
	FVector Center = InBox.GetCenter();
	FVector Extent = InBox.GetExtent();
	FCollisionShape BoxShape = FCollisionShape::MakeBox(Extent);

	QueryParams.ClearIgnoredSourceObjects();
	QueryParams.AddIgnoredActors(HitCheckContext.HitActorsThisFrame);
	QueryParams.AddIgnoredActor(GetOwner());

	GetWorld()->OverlapMultiByObjectType(
		OutCandidates,
		Center,
		FQuat::Identity,
		ObjectQueryParams,
		BoxShape,
		QueryParams
	);
}

bool AMyWeapon::IntersectQuadWithCapsule(const FWeaponHitQuery& Query, const FVector& CapsuleBase, const FVector& CapsuleTop, float Radius)
{
	if (IntersectTriangleWithCapsule(
		Query.Tip_New,
		Query.Tip_Old,
		Query.Base_New,
		CapsuleBase, CapsuleTop, Radius
	))
	{
		return true;
	}

	return IntersectTriangleWithCapsule(
		Query.Tip_Old,
		Query.Base_Old,
		Query.Base_New,
		CapsuleBase, CapsuleTop, Radius
	);
}

bool AMyWeapon::IntersectTriangleWithCapsule(const FVector& V0, const FVector& V1, const FVector& V2, const FVector& CapsuleBase, const FVector& CapsuleTop, float R)
{
	float R2 = R * R;
	FVector CapsuleAxis = CapsuleTop - CapsuleBase; // Base -> Top 방향 벡터

	//  1. 삼각형 vs 위아래 구(Sphere) 판정
	//
	//  캡슐의 양 끝 반구 중심(CapsuleBase, CapsuleTop)과
	//  삼각형 각 점 사이의 최단거리가 R 미만이면 교차.

	auto DistSqPointToTriangle = [](
		const FVector& P,
		const FVector& A, const FVector& B, const FVector& C) -> float
		{
			// 구 중심 P를 Triangle 평면에 투영
			FVector AB = B - A;
			FVector AC = C - A;
			FVector Normal = FVector::CrossProduct(AB, AC); // 정규화 불필요

			FVector AP = P - A;
			float   T = FVector::DotProduct(AP, Normal) /
				FVector::DotProduct(Normal, Normal);
			FVector Projected = P - Normal * T; // 평면 위 투영점

			// Barycentric 좌표로 투영점이 Triangle 내부인지 확인
			// u, v, w 모두 0~1이고 합이 1이면 내부
			FVector v0 = C - A, v1 = B - A, v2 = Projected - A;
			float   d00 = v0 | v0, d01 = v0 | v1, d11 = v1 | v1;
			float   d20 = v2 | v0, d21 = v2 | v1;
			float   Denom = d00 * d11 - d01 * d01;

			if (FMath::Abs(Denom) < SMALL_NUMBER)
			{
				// 삼각형이 퇴화(면적 0) -> 세 꼭짓점 중 최근접점
				float DA = (P - A).SizeSquared();
				float DB = (P - B).SizeSquared();
				float DC = (P - C).SizeSquared();
				return FMath::Min3(DA, DB, DC);
			}

			float V = (d11 * d20 - d01 * d21) / Denom;
			float W = (d00 * d21 - d01 * d20) / Denom;
			float U = 1.0f - V - W;

			if (U >= 0.0f && V >= 0.0f && W >= 0.0f)
			{
				// 투영점이 Triangle 내부 -> 평면까지의 수직거리
				return (P - Projected).SizeSquared();
			}

			// 투영점이 Triangle 외부 -> 세 엣지 중 최근접점
			auto DistSqToSegment = [](
				const FVector& P,
				const FVector& A, const FVector& B) -> float
				{
					FVector AB = B - A;
					float   Len = AB.SizeSquared();
					if (Len < SMALL_NUMBER) return (P - A).SizeSquared();
					float   T = FMath::Clamp((P - A | AB) / Len, 0.0f, 1.0f);
					return (P - (A + AB * T)).SizeSquared();
				};

			return FMath::Min3(
				DistSqToSegment(P, A, B),
				DistSqToSegment(P, B, C),
				DistSqToSegment(P, C, A));
		};

	// CapsuleBase(아래 구), CapsuleTop(위 구) 각각 판정
	if (DistSqPointToTriangle(CapsuleBase, V0, V1, V2) < R2) return true;
	if (DistSqPointToTriangle(CapsuleTop, V0, V1, V2) < R2) return true;

	// 2. 삼각형의 각 꼭짓점 vs 캡슐의 원기둥 판정
	//
	//  꼭짓점을 CapsuleAxis에 정사영했을 때 [0, 1] 범위 안에 있고
	//  (= 캡슐 높이 구간 안에 있고)
	//  캡슐 축 선분까지의 거리가 R 미만이면 교차.

	float AxisLenSq = CapsuleAxis.SizeSquared();

	for (const FVector& Vertex : { V0, V1, V2 })
	{
		FVector ToVertex = Vertex - CapsuleBase;

		// 캡슐 축 선분에 정사영, [0,1] 이면 원기둥 구간 내
		float T = (ToVertex | CapsuleAxis) / AxisLenSq;
		if (T < 0.0f || T > 1.0f) continue;

		// 축 선분까지의 수직거리
		FVector Closest = CapsuleBase + CapsuleAxis * T;
		float   DistSq = (Vertex - Closest).SizeSquared();

		if (DistSq < R2) return true;
	}

	return false;
}

void AMyWeapon::InitHitCheckContext(const UAnimMontage* Montage)
{
	HitCheckContext.Reset();

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	check(OwnerCharacter);
	UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	HitCheckContext.MontageInstance = AnimInstance->GetActiveInstanceForMontage(Montage);
	if (HitCheckContext.MontageInstance == nullptr)
	{
		return;
	}

	HitCheckContext.Montage = Montage;

	HitCheckContext.AnimSequence = Cast<UAnimSequence>(Montage->GetFirstAnimReference());
	if (HitCheckContext.AnimSequence == nullptr)
	{
		return;
	}

	HitCheckContext.OwnerMesh = OwnerCharacter->GetMesh();
	if (HitCheckContext.OwnerMesh == nullptr)
	{
		return;
	}

	const USkeletalMeshSocket* GripSocket = HitCheckContext.OwnerMesh->GetSocketByName(GripSocketName);
	if (GripSocket == nullptr)
	{
		return;
	}
	HitCheckContext.GripBoneName = GripSocket->BoneName;
	UE_LOG(LogTemp, Display, TEXT("GripBoneName: %s"), *GripSocket->BoneName.ToString());
	HitCheckContext.GripBoneIndex = HitCheckContext.OwnerMesh->GetBoneIndex(GripSocket->BoneName);

	BuildBoneContainer(HitCheckContext.OwnerMesh->GetSkeletalMeshAsset());

	const FTransform& GripSocketToBone = GripSocket->GetSocketLocalTransform();

	TArray<FTransform>& WeaponSocketToGripBone = HitCheckContext.WeaponSocketToGripBone;
	WeaponSocketToGripBone.SetNum(WeaponSockets.Num());
	for (int32 Socket = 0; Socket < WeaponSockets.Num(); ++Socket)
	{
		const FTransform& WeaponSocketToWeaponMesh = WeaponMesh->GetSocketTransform(WeaponSockets[Socket], RTS_Component);
		WeaponSocketToGripBone[Socket] = WeaponSocketToWeaponMesh * GripSocketToBone;
	}
}

void AMyWeapon::BuildBoneContainer(USkeletalMesh* SkelMesh)
{
	const FReferenceSkeleton& RefSkel = SkelMesh->GetRefSkeleton();

	TArray<FBoneIndexType> RequiredBones;

	int32 Current = HitCheckContext.GripBoneIndex;
	while (Current != INDEX_NONE)
	{
		RequiredBones.AddUnique(Current);
		Current = RefSkel.GetParentIndex(Current);
	}

	RequiredBones.Sort();

	HitCheckContext.BoneContainer.InitializeTo(
		RequiredBones,
		UE::Anim::FCurveFilterSettings(),
		*SkelMesh
	);
}

bool AMyWeapon::FHitCheckContext::IsValid() const
{
	if (!AnimSequence || !MontageInstance || !Montage || MontageInstance->Montage != Montage)
	{
		return false;
	}
	if (!OwnerMesh || GripBoneName == NAME_None || GripBoneIndex == INDEX_NONE)
	{
		return false;
	}
	if (WeaponSocketToGripBone.IsEmpty())
	{
		return false;
	}
	return true;
}

void AMyWeapon::FHitCheckContext::Reset()
{
	AnimSequence = nullptr;
	MontageInstance = nullptr;
	Montage = nullptr;

	OwnerMesh = nullptr;
	GripBoneName = NAME_None;
	GripBoneIndex = INDEX_NONE;
	WeaponSocketToGripBone.Reset();
	BoneContainer.Reset();

	HitActorsThisFrame.Empty();
	SkillHandledActors.Reset();
}
