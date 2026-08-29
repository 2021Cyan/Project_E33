#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "UAnimNotify_ApplySkill.generated.h"

UCLASS()
class TEAMPROJECT_API UUAnimNotify_ApplySkill : public UAnimNotify
{
	GENERATED_BODY()

public:
	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};