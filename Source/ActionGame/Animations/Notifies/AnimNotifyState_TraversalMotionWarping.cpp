// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/Notifies/AnimNotifyState_TraversalMotionWarping.h"
#include "Characters/MyPlayerCharacter.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"

URootMotionModifier* UAnimNotifyState_TraversalMotionWarping::AddRootMotionModifier_Implementation(
	UMotionWarpingComponent* MotionWarpingComp,
	const UAnimSequenceBase* Animation,
	float StartTime,
	float EndTime) const
{
	URootMotionModifier* Modifier = Super::AddRootMotionModifier_Implementation(
		MotionWarpingComp,
		Animation,
		StartTime,
		EndTime);

	if (Modifier)
	{
		Modifier->OnDeactivateDelegate.BindDynamic(this, &ThisClass::OnTraversalRootMotionModifierDeactivate);
	}

	return Modifier;
}

void UAnimNotifyState_TraversalMotionWarping::OnTraversalRootMotionModifierDeactivate(
	UMotionWarpingComponent* MotionWarpingComp,
	URootMotionModifier* Modifier)
{
	Super::OnRootMotionModifierDeactivate(MotionWarpingComp, Modifier);

	if (MotionWarpingComp == nullptr || Modifier == nullptr || Modifier->PreviousPosition < Modifier->EndTime)
	{
		return;
	}

	const URootMotionModifier_Warp* WarpModifier = Cast<URootMotionModifier_Warp>(Modifier);
	if (WarpModifier == nullptr || WarpModifier->WarpTargetName.IsNone())
	{
		return;
	}

	if (AMyPlayerCharacter* PlayerCharacter = Cast<AMyPlayerCharacter>(MotionWarpingComp->GetOwner()))
	{
		PlayerCharacter->OnTraversalWarpEnded(WarpModifier->WarpTargetName);
	}
}
