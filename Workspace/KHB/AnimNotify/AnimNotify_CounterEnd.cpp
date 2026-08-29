#include "AnimNotify_CounterEnd.h"
#include "../Character/PlayerCharacter.h"
#include "../Core/BattleManager.h"

void UAnimNotify_CounterEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	if (!MeshComp->GetOwner()->HasAuthority())
	{
		return;
	}

	APlayerCharacter* CounterPlayer = Cast<APlayerCharacter>(MeshComp->GetOwner());
	if (!CounterPlayer)
	{
		return;
	}

	ABattleManager* BM = CounterPlayer->GetBattleManager();
	if (BM)
	{
		BM->NotifyCounterFinished(CounterPlayer);
	}
}
