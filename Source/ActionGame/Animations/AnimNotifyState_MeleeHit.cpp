// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/AnimNotifyState_MeleeHit.h"
#include "Characters/MyCharacter.h"
#include "Combat/MyCombatComponent.h"
#include "Items/MyWeapon.h"

void UAnimNotifyState_MeleeHit::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	//Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (AMyCharacter* MeshOwnerCharacter = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		if (AMyWeapon* Weapon = MeshOwnerCharacter->GetCombatComponent()->GetCurrentWeapon())
		{
			Weapon->OnAttackBegin();
		}
	}
}

void UAnimNotifyState_MeleeHit::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	//Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (AMyCharacter* MeshOwnerCharacter = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		if (AMyWeapon* Weapon = MeshOwnerCharacter->GetCombatComponent()->GetCurrentWeapon())
		{
			Weapon->OnAttackEnd();
		}
	}
}
