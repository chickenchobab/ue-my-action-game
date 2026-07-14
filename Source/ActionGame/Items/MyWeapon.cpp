// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/MyWeapon.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "Skills/MySkillData.h"

AMyWeapon::AMyWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.SetTickFunctionEnable(false);

	GripRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GripRoot"));
	SetRootComponent(GripRoot);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GripRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetSimulatePhysics(false);

	DefaultAttachedSocket = TEXT("WeaponSocket");

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

	SetOwner(NewParent->GetOwner());
	for (auto &KVP : SkillSet)
	{
		if (UMySkillData* SkillData = KVP.Value)
		{
			SkillData->InitWithAvatar(GetOwner());
		}
	}
	
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bIsEquipped = true;

	PrimaryActorTick.SetTickFunctionEnable(true);
}

void AMyWeapon::UnEquip()
{
	const FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	DetachFromActor(DetachRules);

	for (auto& KVP : SkillSet)
	{
		if (UMySkillData* SkillData = KVP.Value)
		{
			SkillData->InitWithAvatar(nullptr);
		}
	}

	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	PrimaryActorTick.SetTickFunctionEnable(false);

	bIsEquipped = false;
}

//void AMyWeapon::Register_OnAttackHit(FOnAttackHit::FDelegate&& Delegate)
//{
//	bIsAttacking = true;
//	OnAttackHit.Add(MoveTemp(Delegate));
//}

void AMyWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (WeaponMesh->DoesSocketExist(TEXT("GripSocket")))
	{
		const FTransform GripTransform = WeaponMesh->GetSocketTransform(TEXT("GripSocket"), RTS_Component);
		// In case the static mesh has been scaled, multiply the relative scale of the mesh
		WeaponMesh->SetRelativeLocation(-GripTransform.GetLocation() * WeaponMesh->GetRelativeScale3D());
	}
}

void AMyWeapon::BeginPlay()
{
	Super::BeginPlay();

	WeaponMeshSocketNames = WeaponMesh->GetAllSocketNames();
}

//void AMyWeapon::Tick(float DeltaSeconds)
//{
//	Super::Tick(DeltaSeconds);
//
//	if (!bIsEquipped || !bIsAttacking || WeaponMeshSocketNames.IsEmpty())
//	{
//		return;
//	}
//
//	FBox Box(ForceInit);
//	for (const FName& SocketName : WeaponMeshSocketNames)
//	{
//		const FVector& SocketLocation = WeaponMesh->GetSocketLocation(SocketName);
//		Box += SocketLocation;
//	}
//	Box = Box.ExpandBy(2.0f);
//
//	FVector Center = Box.GetCenter();
//	FVector Extent = Box.GetExtent();
//
//	FCollisionShape BoxShape = FCollisionShape::MakeBox(Extent);
//
//	FCollisionQueryParams QueryParams;
//	QueryParams.bTraceComplex = false;
//	QueryParams.AddIgnoredActor(GetOwner());
//
//	FCollisionObjectQueryParams ObjectParams;
//	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
//
//	TArray<FOverlapResult> Candidates;
//	GetWorld()->OverlapMultiByObjectType(
//		Candidates,
//		Center,
//		FQuat::Identity,
//		ObjectParams,
//		BoxShape,
//		QueryParams
//	);
//
//	FString DebugMsg;
//	for (auto& Result : Candidates)
//	{
//		DebugMsg += Result.GetActor()->GetName();
//		DebugMsg += TEXT(" ");
//	}
//	GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Red, *DebugMsg);
//}

