// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/Notifies/AnimNotify_FallLandEnd.h"
#include "Animations/MyAnimInstance.h"

void UAnimNotify_FallLandEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp)
	{
		return;
	}

	if (UMyAnimInstance* AnimInstance = Cast<UMyAnimInstance>(MeshComp->GetAnimInstance()))
	{
		AnimInstance->NotifyFallLandEnd();
	}
}
