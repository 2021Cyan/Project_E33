#include "LobbyHUDWidget.h"
#include "LobbyPlayerSlotWidget.h"
#include "../Core/LobbyGameState.h"
#include "TeamProjectPlayerState.h"
#include "../Player/LobbyPlayerController.h"
#include "Components/Button.h"
#include "../Core/TeamProjectGameInstance.h"

void ULobbyHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Lune)
	{
		Btn_Lune->OnClicked.AddDynamic(this, &ULobbyHUDWidget::OnClickedLune);
	}

	if (Btn_Maelle)
	{
		Btn_Maelle->OnClicked.AddDynamic(this, &ULobbyHUDWidget::OnClickedMaelle);
	}

	if (Btn_Verso)
	{
		Btn_Verso->OnClicked.AddDynamic(this, &ULobbyHUDWidget::OnClickedVerso);
	}

	if (Btn_Ready)
	{
		Btn_Ready->OnClicked.AddDynamic(this, &ULobbyHUDWidget::OnClickedReady);
	}

	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddDynamic(this, &ULobbyHUDWidget::OnClickedExit);
	}

	RefreshLobbySlots();
}

void ULobbyHUDWidget::RefreshLobbySlots()
{
	TArray<ULobbyPlayerSlotWidget*> Slots = {Slot_Player0,Slot_Player1,Slot_Player2,Slot_Player3};

	for (ULobbyPlayerSlotWidget* PlayerSlot : Slots)
	{
		if (PlayerSlot)
		{
			PlayerSlot->SetEmpty();
		}
	}

	ALobbyGameState* LobbyGS = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	if (!LobbyGS) return;

	const TArray<TObjectPtr<ATeamProjectPlayerState>>& PartyPlayers = LobbyGS->GetPartyPlayers();
	ATeamProjectPlayerState* LocalPS = nullptr;
	if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		LocalPS = PC->GetPlayerState<ATeamProjectPlayerState>();
	}

	if (LocalPS)
	{
		if (bHasLocalSelectedCharacterPreview && LocalPS->GetSelectedCharacterIndex() == LocalSelectedCharacterPreview)
		{
			bHasLocalSelectedCharacterPreview = false;
		}

		if (bHasLocalReadyPreview && LocalPS->IsReady() == bLocalReadyPreview)
		{
			bHasLocalReadyPreview = false;
		}
	}

	for (int32 i = 0; i < PartyPlayers.Num() && i < Slots.Num(); ++i)
	{
		if (Slots[i])
		{
			ATeamProjectPlayerState* TeamPS = PartyPlayers[i];
			if (TeamPS && TeamPS == LocalPS && (bHasLocalSelectedCharacterPreview || bHasLocalReadyPreview))
			{
				const int32 CharacterIndex = bHasLocalSelectedCharacterPreview ? LocalSelectedCharacterPreview : TeamPS->GetSelectedCharacterIndex();
				const bool bReady = bHasLocalReadyPreview ? bLocalReadyPreview : TeamPS->IsReady();
				Slots[i]->SetPlayerDisplayData(TeamPS->GetPlayerName(), CharacterIndex, bReady, CharacterPortraits, CharacterNames);
			}
			else
			{
				Slots[i]->SetPlayerStateData(TeamPS, CharacterPortraits, CharacterNames);
			}
		}
	}
}

void ULobbyHUDWidget::ApplyLocalSelectedCharacter(int32 NewIndex)
{
	bHasLocalSelectedCharacterPreview = true;
	LocalSelectedCharacterPreview = NewIndex;
	RefreshLobbySlots();
}

void ULobbyHUDWidget::ApplyLocalReady(bool bNewReady)
{
	bHasLocalReadyPreview = true;
	bLocalReadyPreview = bNewReady;
	RefreshLobbySlots();
}

void ULobbyHUDWidget::OnClickedLune()
{
	ApplyLocalSelectedCharacter(0);

	if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		PC->Server_SetSelectedCharacterIndex(0);
	}
}

void ULobbyHUDWidget::OnClickedMaelle()
{
	ApplyLocalSelectedCharacter(1);

	if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		PC->Server_SetSelectedCharacterIndex(1);
	}
}

void ULobbyHUDWidget::OnClickedVerso()
{
	ApplyLocalSelectedCharacter(2);

	if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		PC->Server_SetSelectedCharacterIndex(2);
	}
}

void ULobbyHUDWidget::OnClickedReady()
{
	if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		ATeamProjectPlayerState* TeamPS = PC->GetPlayerState<ATeamProjectPlayerState>();
		if (!TeamPS)
		{
			return;
		}

		const bool bCurrentReady = bHasLocalReadyPreview ? bLocalReadyPreview : TeamPS->IsReady();
		const bool bNextReady = !bCurrentReady;

		ApplyLocalReady(bNextReady);
		PC->Server_SetReady(bNextReady);
	}
}

void ULobbyHUDWidget::OnClickedExit()
{
	if (UTeamProjectGameInstance* GI = GetGameInstance<UTeamProjectGameInstance>())
	{
		GI->OpenMainMenuLevel();
	}
}
