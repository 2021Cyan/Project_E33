// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TeamProjectGameMode.h"
#include "LobbyGameMode.generated.h"

UCLASS()
class TEAMPROJECT_API ALobbyGameMode : public ATeamProjectGameMode
{
	GENERATED_BODY()
	
public:
	ALobbyGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void CheckAllPlayersReady();

	void NotifyLobbyStateChanged();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FName FieldLevelPath = TEXT("/Game/Level/LV_Field");

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	bool bAutoTravelWhenAllReady = true;

private:
	void TravelToField();

	bool bTravelingToField = false;
};
