#include "AnimNotify_SpawnEruption.h"
#include "../CombatComponent.h"

void UAnimNotify_SpawnEruption::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (MeshComp && MeshComp->GetOwner())
    {
        UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>();
        if (CombatComp)
        {
            // �Ĺ� ������Ʈ���� "�ٴ� ����(Eruption) �Ͷ߷���" ����
            CombatComp->SpawnSkillEffect(EEffectSpawnType::Eruption);
        }
    }
}