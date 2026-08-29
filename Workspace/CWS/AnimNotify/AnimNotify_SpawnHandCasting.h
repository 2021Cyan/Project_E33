#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnHandCasting.generated.h"

UCLASS(meta = (DisplayName = "Spawn Hand Casting (º’ ¿Ã∆Â∆Æ)"))
class TEAMPROJECT_API UUAnimNotify_SpawnHandCasting : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};