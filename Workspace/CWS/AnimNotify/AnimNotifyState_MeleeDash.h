#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_MeleeDash.generated.h"

UCLASS(meta = (DisplayName = "Melee Dash (근접 돌진)"))
class TEAMPROJECT_API UAnimNotifyState_MeleeDash : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    // 에디터에서 "복귀(Dash Out)" 용도로 쓸 것인지 체크하는 변수
    UPROPERTY(EditAnywhere, Category = "Dash Settings")
    bool bIsDashOut = false;

    // 적과 얼마나 떨어져서 멈출 것인지 조절하는 변수 1.5m
    UPROPERTY(EditAnywhere, Category = "Dash Settings")
    float StopDistance = 150.0f;

    UPROPERTY(EditAnywhere, Category = "Dash Settings")
    float DashSpeed = 1500.0f;

    virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
};