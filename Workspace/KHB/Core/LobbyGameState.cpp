#include "LobbyGameState.h"
#include "TeamProjectPlayerState.h"
#include "../Player/LobbyPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyGameState, PartyPlayers);
}

void ALobbyGameState::AddPartyPlayer(ATeamProjectPlayerState* PlayerState)
{
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	PartyPlayers.AddUnique(PlayerState);
	RebuildPartyIndices();
}

void ALobbyGameState::RemovePartyPlayer(ATeamProjectPlayerState* PlayerState)
{
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	PartyPlayers.Remove(PlayerState);
	RebuildPartyIndices();
}

void ALobbyGameState::RebuildPartyIndices()
{
	if (!HasAuthority())
	{
		return;
	}

	PartyPlayers.RemoveAll([](const TObjectPtr<ATeamProjectPlayerState>& PlayerState)
	{
		return PlayerState == nullptr;
	});

	for (int32 Index = 0; Index < PartyPlayers.Num(); ++Index)
	{
		if (ATeamProjectPlayerState* PlayerState = PartyPlayers[Index])
		{
			PlayerState->SetPartyIndex(Index);
		}
	}
}

void ALobbyGameState::OnRep_PartyPlayers()
{
	if (!GetWorld())
	{
		return;
	}

	if (ALobbyPlayerController* LobbyPC = Cast<ALobbyPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		LobbyPC->RefreshLobbyUI();
	}
}
