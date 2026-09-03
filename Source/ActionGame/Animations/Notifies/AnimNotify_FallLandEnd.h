// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_FallLandEnd.generated.h"

// modified: 착지 애니메이션 종료 시 애님 인스턴스의 전이 플래그를 설정한다
UCLASS()
class ACTIONGAME_API UAnimNotify_FallLandEnd : public UAnimNotify
{
	GENERATED_BODY()

public:

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
