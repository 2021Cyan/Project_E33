#include "LobbyPlayerController.h"
#include "../Core/LobbyGameMode.h"
#include "TeamProjectPlayerState.h"
#include "../UI/LobbyHUDWidget.h"

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) return;

	if (LobbyHUDWidgetClass)
	{
		LobbyHUDWidget = CreateWidget<ULobbyHUDWidget>(this, LobbyHUDWidgetClass);
		if (LobbyHUDWidget)
		{
			LobbyHUDWidget->AddToViewport(10);

			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(LobbyHUDWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

			SetInputMode(InputMode);
			bShowMouseCursor = true;

			RefreshLobbyUI();
		}
	}
}

ALobbyPlayerController::ALobbyPlayerController()
{
}

void ALobbyPlayerController::RefreshLobbyUI()
{
	if (LobbyHUDWidget)
	{
		LobbyHUDWidget->RefreshLobbySlots();
	}
}

void ALobbyPlayerController::Client_RefreshLobbyUI_Implementation()
{
	RefreshLobbyUI();
}

void ALobbyPlayerController::Server_SetSelectedCharacterIndex_Implementation(int32 NewIndex)
{
    ATeamProjectPlayerState* TeamPS = GetPlayerState<ATeamProjectPlayerState>();
    if (!TeamPS)
    {
        return;
    }

    TeamPS->SetSelectedCharacterIndex(NewIndex);

	if (ALobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
	{
		LobbyGM->NotifyLobbyStateChanged();
	}
}

void ALobbyPlayerController::Server_SetReady_Implementation(bool bNewReady)
{
    ATeamProjectPlayerState* TeamPS = GetPlayerState<ATeamProjectPlayerState>();
    if (!TeamPS)
    {
        return;
	}

	TeamPS->SetReady(bNewReady);

	if (UWorld* World = GetWorld())
	{
		if (ALobbyGameMode* LobbyGM = World->GetAuthGameMode<ALobbyGameMode>())
		{
			LobbyGM->NotifyLobbyStateChanged();
			LobbyGM->CheckAllPlayersReady();
		}
	}
}
