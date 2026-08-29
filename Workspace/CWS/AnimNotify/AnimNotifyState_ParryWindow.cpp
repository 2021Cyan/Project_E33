#include "AnimNotifyState_ParryWindow.h"
#include "../CombatComponent.h"

void UAnimNotifyState_ParryWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (MeshComp && MeshComp->GetOwner())
    {
        UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>();
        if (CombatComp)
        {
            CombatComp->SetCombatState(ECombatState::Parrying);

            if (MeshComp->GetOwner()->HasAuthority())
            {

                CombatComp->Server_SetCombatState(ECombatState::Parrying);
            }
        }
    }
}

void UAnimNotifyState_ParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (MeshComp && MeshComp->GetOwner())
    {
        UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>();
        if (CombatComp)
        {
            // 서버가 스스로 자기 메모리에 "철수 방패 내렸음" 이라고 도장 찍음
            if (CombatComp->GetCurrentState() == ECombatState::Parrying)
            {
                CombatComp->SetCombatState(ECombatState::Idle);

                if (MeshComp->GetOwner()->HasAuthority())
                {
                    CombatComp->SetCombatState(ECombatState::Idle);
                    CombatComp->Server_SetCombatState(ECombatState::Idle);
                }
            }
        }
    }
}