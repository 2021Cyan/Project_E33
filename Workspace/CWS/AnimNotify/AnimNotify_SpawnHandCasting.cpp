#include "AnimNotify_SpawnHandCasting.h"
#include "../CombatComponent.h"

void UUAnimNotify_SpawnHandCasting::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (MeshComp && MeshComp->GetOwner())
    {
        UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>();
        if (CombatComp)
        {
            // �Ĺ� ������Ʈ���� �� ��(��)���� ����Ʈ �Ͷ߸���� ����
            CombatComp->SpawnSkillEffect(EEffectSpawnType::HandCasting);
        }
    }
}