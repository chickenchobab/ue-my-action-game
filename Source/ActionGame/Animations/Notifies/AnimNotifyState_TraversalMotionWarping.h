// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AnimNotifyState_MotionWarping.h"
#include "AnimNotifyState_TraversalMotionWarping.generated.h"

class UMotionWarpingComponent;
class URootMotionModifier;

UCLASS(meta = (DisplayName = "Traversal Motion Warping"))
class ACTIONGAME_API UAnimNotifyState_TraversalMotionWarping : public UAnimNotifyState_MotionWarping
{
	GENERATED_BODY()

public:

	virtual URootMotionModifier* AddRootMotionModifier_Implementation(
		UMotionWarpingComponent* MotionWarpingComp,
		const UAnimSequenceBase* Animation,
		float StartTime,
		float EndTime) const override;

private:

	UFUNCTION()
	void OnTraversalRootMotionModifierDeactivate(
		UMotionWarpingComponent* MotionWarpingComp,
		URootMotionModifier* Modifier);
};
