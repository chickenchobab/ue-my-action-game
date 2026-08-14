// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/Notifies/AnimNotifyState_MeleeHit.h"
#include "Characters/MyCharacter.h"
#include "Combat/MyCombatComponent.h"
#include "Items/MyWeapon.h"
#include "Attributes/MyGameplayTags.h"

void UAnimNotifyState_MeleeHit::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	//Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (AMyCharacter* MeshOwnerCharacter = Cast<AMyCharacter>(MeshComp->GetOwner()))
	{
		MeshOwnerCharacter->AddStatusTag(MyGameplayTags::Status_Channeling);
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
		MeshOwnerCharacter->RemoveStatusTag(MyGameplayTags::Status_Channeling);
		if (AMyWeapon* Weapon = MeshOwnerCharacter->GetCombatComponent()->GetCurrentWeapon())
		{
			Weapon->OnAttackEnd();
		}
	}
}
