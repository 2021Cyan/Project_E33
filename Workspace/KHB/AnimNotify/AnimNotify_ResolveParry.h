// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ResolveParry.generated.h"

/**
 * 공격 몽타주의 "마지막 타격" 지점에 배치.
 * 그 시점까지 기록된 패링 결과로 반격 발동 여부를 즉시 판정한다.
 * (공격 세션/턴 종료는 별도로 AttackSequenceEnd 노티파이가 담당)
 */
UCLASS()
class TEAMPROJECT_API UAnimNotify_ResolveParry : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
