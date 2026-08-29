#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnEruption.generated.h"

UCLASS(meta = (DisplayName = "Spawn Eruption Effect (¹Ù´Ú Æø¹ß)"))
class TEAMPROJECT_API UAnimNotify_SpawnEruption : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};