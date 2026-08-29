#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LobbyGameState.generated.h"

class ATeamProjectPlayerState;

UCLASS()
class TEAMPROJECT_API ALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<TObjectPtr<ATeamProjectPlayerState>>& GetPartyPlayers() const { return PartyPlayers; }

	void AddPartyPlayer(ATeamProjectPlayerState* PlayerState);
	void RemovePartyPlayer(ATeamProjectPlayerState* PlayerState);
	void RebuildPartyIndices();

protected:
	UFUNCTION()
	void OnRep_PartyPlayers();

private:
	UPROPERTY(ReplicatedUsing = OnRep_PartyPlayers)
	TArray<TObjectPtr<ATeamProjectPlayerState>> PartyPlayers;
};
