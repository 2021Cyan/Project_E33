#include "UAnimNotify_ApplySkill.h"
#include "../CombatComponent.h" // 우리가 만든 컴포넌트 헤더

void UUAnimNotify_ApplySkill::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    // 부모 클래스의 기본 기능 실행
    Super::Notify(MeshComp, Animation, EventReference);

    // MeshComp(캐릭터 뼈대)와 그 주인이 존재하는지 안전 검사
    if (MeshComp && MeshComp->GetOwner())
    {
        // 주인의 몸통에서 UCombatComponent를 찾아냄
        UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>();

        if (CombatComp)
        {
            // 컴포넌트를 찾았으면 데미지/이펙트 트리거 함수를 실행
            CombatComp->ApplySkillEffect();
        }
    }
}