// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_StartAttackQTE.generated.h"

/**
 * 
 */
UCLASS()
class TEAMPROJECT_API UAnimNotify_StartAttackQTE : public UAnimNotify
{
	GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
    UPROPERTY(EditAnywhere, Category = "QTE")
    float Duration = 1.25f;

    UPROPERTY(EditAnywhere, Category = "QTE")
    float SuccessStartTime = 1.0f;

    UPROPERTY(EditAnywhere, Category = "QTE")
    float SuccessEndTime = 1.25f;

    UPROPERTY(EditAnywhere, Category = "QTE")
    FVector2D ScreenPosition = FVector2D(0.5f, 0.5f);
};
