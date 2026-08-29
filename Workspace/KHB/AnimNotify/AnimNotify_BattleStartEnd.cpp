#include "AnimNotify_BattleStartEnd.h"
#include "../Character/PlayerCharacter.h"
#include "../../CWS/CombatComponent.h"
#include "../Player/BasePlayerController.h"

void UAnimNotify_BattleStartEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	APlayerCharacter* Player = Cast<APlayerCharacter>(MeshComp->GetOwner());
	if (!Player || !Player->IsLocallyControlled()) return;
	Player->OnBattleEntryMontageFinished();
}
