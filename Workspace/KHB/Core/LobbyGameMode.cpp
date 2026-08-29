#include "LobbyGameMode.h"
#include "LobbyGameState.h"
#include "TeamProjectPlayerState.h"
#include "../Player/LobbyPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

ALobbyGameMode::ALobbyGameMode()
{
	bUseSeamlessTravel = true;
	PlayerControllerClass = ALobbyPlayerController::StaticClass();
	PlayerStateClass = ATeamProjectPlayerState::StaticClass();
	GameStateClass = ALobbyGameState::StaticClass();
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (ALobbyGameState* LobbyGS = GetGameState<ALobbyGameState>())
	{
		if (ATeamProjectPlayerState* TeamPS = NewPlayer ? NewPlayer->GetPlayerState<ATeamProjectPlayerState>() : nullptr)
		{
			LobbyGS->AddPartyPlayer(TeamPS);
		}
	}
	
	NotifyLobbyStateChanged();
}

void ALobbyGameMode::Logout(AController* Exiting)
{
	if (ALobbyGameState* LobbyGS = GetGameState<ALobbyGameState>())
	{
		if (ATeamProjectPlayerState* TeamPS = Exiting ? Exiting->GetPlayerState<ATeamProjectPlayerState>() : nullptr)
		{
			LobbyGS->RemovePartyPlayer(TeamPS);
		}
	}

	Super::Logout(Exiting);

	NotifyLobbyStateChanged();
}

void ALobbyGameMode::CheckAllPlayersReady()
{
	if (!HasAuthority() || bTravelingToField) return;

	AGameStateBase* GS = GameState;
	if (!GS || GS->PlayerArray.IsEmpty()) return;

	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		const ATeamProjectPlayerState* TeamPS = Cast<ATeamProjectPlayerState>(PlayerState);
		if (!TeamPS || !TeamPS->IsReady()) return;
	}

	if (bAutoTravelWhenAllReady)
	{
		bTravelingToField = true;
		TravelToField();
	}
}

void ALobbyGameMode::NotifyLobbyStateChanged()
{
	if (!HasAuthority()) return;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALobbyPlayerController* LobbyPC = Cast<ALobbyPlayerController>(It->Get()))
		{
			LobbyPC->Client_RefreshLobbyUI();
		}
	}
}

void ALobbyGameMode::TravelToField()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	World->ServerTravel(FieldLevelPath.ToString() + TEXT("?listen"));
}
