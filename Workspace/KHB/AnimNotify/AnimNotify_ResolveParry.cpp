#include "AnimNotify_ResolveParry.h"
#include "../Character/BaseCharacter.h"
#include "../Core/BattleManager.h"

void UAnimNotify_ResolveParry::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner()) return;

    // 반격 판정은 서버 권위에서만 수행
    if (!MeshComp->GetOwner()->HasAuthority()) return;

    ABaseCharacter* Attacker = Cast<ABaseCharacter>(MeshComp->GetOwner());
    if (!Attacker) return;

    ABattleManager* BM = Attacker->GetBattleManager();
    if (BM)
    {
        BM->ResolveCounter();
    }
}
