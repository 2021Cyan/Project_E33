#include "AnimNotify_StartAttackQTE.h"
#include "../Character/BaseCharacter.h"
#include "../Core/BattleManager.h"

void UAnimNotify_StartAttackQTE::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner()) return;

    if (!MeshComp->GetOwner()->HasAuthority()) return;

    ABaseCharacter* Attacker = Cast<ABaseCharacter>(MeshComp->GetOwner());
    if (!Attacker) return;

    ABattleManager* BM = Attacker->GetBattleManager();
    if (!BM) return;

    BM->ShowAttackQTEToPlayers(Duration, SuccessStartTime, SuccessEndTime, ScreenPosition);
}
