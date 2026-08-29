#include "AnimNotify_SpawnCasting.h"
#include "../CombatComponent.h"

void UAnimNotify_SpawnCasting::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (MeshComp && MeshComp->GetOwner())
    {
        UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>();
        if (CombatComp)
        {
            // �Ĺ� ������Ʈ���� "������(Casting) ��ƶ�" ����
            CombatComp->SpawnSkillEffect(EEffectSpawnType::Casting);
        }
    }
}