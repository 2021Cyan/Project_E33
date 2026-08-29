// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "../Interfaces/CMenuInterface.h"
#include "TeamProjectGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class TEAMPROJECT_API UTeamProjectGameInstance : public UGameInstance, public ICMenuInterface
{
	GENERATED_BODY()
	
public:
	UTeamProjectGameInstance();

protected:
	virtual void Init() override;

public:
	UFUNCTION(Exec)
	void Host(FString ServerName) override;

	UFUNCTION(Exec)
	void Join(uint32 Index) override;

	void StartSession();

	void OpenMainMenuLevel() override;
	void RefreshServerList() override;

	bool bShowHostDisconnectedPopup = false;

private:
	void OnCreateSessionComplete(FName InSessionName, bool InSuccess);
	void OnStartSessionComplete(FName InSessionName, bool InSuccess);
	void OnDestroySessionComplete(FName InSessionName, bool InSuccess);
	void OnFindSessionsComplete(bool InSuccess);
	void OnJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type InResult);
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	void CreateSession();
	void TravelToFieldAsListenServer();

private:
	void OnReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);
	bool IsFriendSession(const FOnlineSessionSearchResult& SearchResult) const;
	bool IsProjectSession(const FOnlineSessionSearchResult& SearchResult) const;

	bool IsSteamLoggedIn() const;


private:
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	IOnlineFriendsPtr FriendsInterface;
	TSet<FString> FriendIds;
	bool bFriendsListReady = false;

	FString DesiredServerName;
	bool bCreateSessionAfterDestroy = false;
	bool bJoinSessionAfterDestroy = false;
	bool bHostTravelPending = false;
	uint32 PendingJoinIndex = 0;
};
