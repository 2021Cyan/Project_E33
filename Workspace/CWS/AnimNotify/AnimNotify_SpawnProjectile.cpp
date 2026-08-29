#include "AnimNotify_SpawnProjectile.h"
#include "../CombatComponent.h"

void UUAnimNotify_SpawnProjectile::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (MeshComp && MeshComp->GetOwner())
    {
        UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>();
        if (CombatComp)
        {
            // �Ĺ� ������Ʈ���� ����ü�� ���� ���� (����ź ��ǥ �ڵ� ���۵�)
            CombatComp->SpawnSkillEffect(EEffectSpawnType::Projectile);
        }
    }
}