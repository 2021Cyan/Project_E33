#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnCasting.generated.h"

UCLASS(meta = (DisplayName = "Spawn Casting Effect (¸¶¹ýÁø)"))
class TEAMPROJECT_API UAnimNotify_SpawnCasting : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};