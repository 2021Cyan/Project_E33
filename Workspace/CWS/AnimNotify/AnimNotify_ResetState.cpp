#include "AnimNotify_ResetState.h"
#include "../CombatComponent.h"

void UAnimNotify_ResetState::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (MeshComp && MeshComp->GetOwner())
    {
        UCombatComponent* CombatComp = MeshComp->GetOwner()->FindComponentByClass<UCombatComponent>();
        if (CombatComp)
        {
            // [서버용] 나중에 A님이 여기에 Server_RequestStateChange(ECombatState::Idle) 같은 걸 붙일 수 있습니다
            // [로컬용] 내 상태를 깔끔하게 Idle로 되돌림
            CombatComp->HandleStateChanged(ECombatState::Idle);
        }
    }
}