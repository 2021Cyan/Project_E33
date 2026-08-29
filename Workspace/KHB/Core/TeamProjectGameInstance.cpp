#include "TeamProjectGameInstance.h"
#include "OnlineSessionSettings.h"
#include "../Player/MenuPlayerController.h"

#define SEARCH_PRESENCE TEXT("PRESENCESEARCH")

const static FName SESSION_NAME = TEXT("GameSession");
const static FName SESSION_SETTINGS_KEY = TEXT("TP");
const static FName SESSION_PROJECT_KEY = TEXT("TEAMPROJECT");
const static FString SESSION_PROJECT_VALUE = TEXT("TEAMPROJECT_SESSION");
const static TCHAR* FIELD_LISTEN_TRAVEL_URL = TEXT("/Game/Level/LV_Lobby?listen");

UTeamProjectGameInstance::UTeamProjectGameInstance()
{
}

void UTeamProjectGameInstance::Init()
{
	Super::Init();

	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (OSS)
	{
		UE_LOG(LogTemp, Warning, TEXT("OSS : %s is available."), *OSS->GetSubsystemName().ToString());

		SessionInterface = OSS->GetSessionInterface();

		if (SessionInterface.IsValid())
		{
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UTeamProjectGameInstance::OnCreateSessionComplete);
			SessionInterface->OnStartSessionCompleteDelegates.AddUObject(this, &UTeamProjectGameInstance::OnStartSessionComplete);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UTeamProjectGameInstance::OnDestroySessionComplete);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UTeamProjectGameInstance::OnFindSessionsComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UTeamProjectGameInstance::OnJoinSessionComplete);
		}

		FriendsInterface = OSS->GetFriendsInterface();
		if (FriendsInterface.IsValid())
		{
			FriendsInterface->ReadFriendsList(
				0,
				EFriendsLists::ToString(EFriendsLists::Default),
				FOnReadFriendsListComplete::CreateUObject(this, &UTeamProjectGameInstance::OnReadFriendsListComplete)
			);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Not found subsystem."));
	}

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UTeamProjectGameInstance::OnNetworkFailure);
	}
}

void UTeamProjectGameInstance::Host(FString ServerName)
{
	DesiredServerName = ServerName;
	bHostTravelPending = false;

	if (SessionInterface.IsValid())
	{
		auto AlreadyExistingSession = SessionInterface->GetNamedSession(SESSION_NAME);
		if (AlreadyExistingSession)
		{
			bCreateSessionAfterDestroy = true;
			bJoinSessionAfterDestroy = false;
			SessionInterface->DestroySession(SESSION_NAME);
		}
		else
		{
			CreateSession();
		}
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("Cannot host because SessionInterface is invalid."));
}

void UTeamProjectGameInstance::CreateSession()
{
	if (SessionInterface.IsValid())
	{
		FOnlineSessionSettings SessionSettings;

		IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
		if (OSS && OSS->GetSubsystemName() == FName(TEXT("NULL")))
		{
			SessionSettings.bIsLANMatch = true;
		}
		else
		{
			SessionSettings.bIsLANMatch = false;
		}
		SessionSettings.NumPublicConnections = 4;
		SessionSettings.bShouldAdvertise = true;
		SessionSettings.bUsesPresence = true;
		SessionSettings.bUseLobbiesIfAvailable = true;
		SessionSettings.bAllowJoinInProgress = true;
		SessionSettings.bAllowJoinViaPresence = true;
		SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
		SessionSettings.bAllowInvites = true;
		SessionSettings.Set(SESSION_SETTINGS_KEY, DesiredServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		SessionSettings.Set(SESSION_PROJECT_KEY, SESSION_PROJECT_VALUE, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		SessionInterface->CreateSession(0, SESSION_NAME, SessionSettings);
	}
}

void UTeamProjectGameInstance::StartSession()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->StartSession(SESSION_NAME);
	}
}

void UTeamProjectGameInstance::Join(uint32 Index)
{
	if (!SessionInterface.IsValid()) return;
	if (!SessionSearch.IsValid()) return;

	if (Index >= (uint32)SessionSearch->SearchResults.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid session index: %d"), Index);
		return;
	}

	auto AlreadyExistingSession = SessionInterface->GetNamedSession(SESSION_NAME);
	if (AlreadyExistingSession)
	{
		bCreateSessionAfterDestroy = false;
		bJoinSessionAfterDestroy = true;
		PendingJoinIndex = Index;
		SessionInterface->DestroySession(SESSION_NAME);
		return;
	}

	SessionInterface->JoinSession(0, SESSION_NAME, SessionSearch->SearchResults[Index]);
}

void UTeamProjectGameInstance::OpenMainMenuLevel()
{
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(SESSION_NAME))
	{
		bCreateSessionAfterDestroy = false;
		bJoinSessionAfterDestroy = false;
		bHostTravelPending = false;
		SessionInterface->DestroySession(SESSION_NAME);
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC) return;

	PC->ClientTravel("/Game/Level/LV_MainMenu", ETravelType::TRAVEL_Absolute);
}

void UTeamProjectGameInstance::RefreshServerList()
{
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	if (SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Start finding session."));

		IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
		if (OSS && OSS->GetSubsystemName() == FName(TEXT("NULL")))
		{
			SessionSearch->bIsLanQuery = true;
		}
		SessionSearch->MaxSearchResults = 100;
		SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
		SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	}
}

void UTeamProjectGameInstance::OnCreateSessionComplete(FName InSessionName, bool InSuccess)
{
	if (!InSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Could not create session"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Session name is %s"), *InSessionName.ToString());

	AMenuPlayerController* PC = Cast<AMenuPlayerController>(GetFirstLocalPlayerController());
	if (PC) PC->HideMainMenu();

	UEngine* Engine = GetEngine();
	if (!Engine) return;

	Engine->AddOnScreenDebugMessage(0, 2, FColor::Green, TEXT("Host"));

	bHostTravelPending = true;
	StartSession();
}

void UTeamProjectGameInstance::OnStartSessionComplete(FName InSessionName, bool InSuccess)
{
	if (!bHostTravelPending)
	{
		return;
	}

	if (!InSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Could not start session: %s"), *InSessionName.ToString());
		bHostTravelPending = false;
		return;
	}

	TravelToFieldAsListenServer();
}

void UTeamProjectGameInstance::TravelToFieldAsListenServer()
{
	bHostTravelPending = false;

	UWorld* World = GetWorld();
	if (!World) return;

	UE_LOG(LogTemp, Warning, TEXT("Host travelling to field: %s"), FIELD_LISTEN_TRAVEL_URL);
	World->ServerTravel(FIELD_LISTEN_TRAVEL_URL);
}

void UTeamProjectGameInstance::OnDestroySessionComplete(FName InSessionName, bool InSuccess)
{
	if (!InSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Could not destroy session: %s"), *InSessionName.ToString());
		bCreateSessionAfterDestroy = false;
		bJoinSessionAfterDestroy = false;
		bHostTravelPending = false;
		return;
	}

	if (bCreateSessionAfterDestroy)
	{
		bCreateSessionAfterDestroy = false;
		CreateSession();
		return;
	}

	if (bJoinSessionAfterDestroy)
	{
		bJoinSessionAfterDestroy = false;
		Join(PendingJoinIndex);
	}
}

void UTeamProjectGameInstance::OnFindSessionsComplete(bool InSuccess)
{
	if (!InSuccess || !SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions failed or SessionSearch is invalid."));
		return;
	}

	TArray<FServerData> ServerNames;
	for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
	{
		FServerData ServerData;
		ServerData.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
		ServerData.CurrentPlayers = ServerData.MaxPlayers - SearchResult.Session.NumOpenPublicConnections;
		ServerData.HostUserName = SearchResult.Session.OwningUserName;
		FString ServerName;
		if (SearchResult.Session.SessionSettings.Get(SESSION_SETTINGS_KEY, ServerName))
		{
			ServerData.Name = ServerName;
		}
		ServerNames.Add(ServerData);
	}

	AMenuPlayerController* PC = Cast<AMenuPlayerController>(GetFirstLocalPlayerController());
	if (PC) PC->UpdateServerList(ServerNames);

	UE_LOG(LogTemp, Warning, TEXT("Finished finding session. Results: %d"), SessionSearch->SearchResults.Num());
}

void UTeamProjectGameInstance::OnJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type InResult)
{
	if (!SessionInterface.IsValid()) return;

	if (InResult != EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSession failed. Session: %s, Result: %d"), *InSessionName.ToString(), static_cast<int32>(InResult));
		OpenMainMenuLevel();
		return;
	}

	FString Address;
	if (!SessionInterface->GetResolvedConnectString(InSessionName, Address))
	{
		UE_LOG(LogTemp, Error, TEXT("Could not convert IP address"));
		OpenMainMenuLevel();
		return;
	}

	UEngine* Engine = GetEngine();
	if (!Engine) return;
	Engine->AddOnScreenDebugMessage(0, 5, FColor::Green, FString::Printf(TEXT("Joining to %s"), *Address));

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC) return;
	PC->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
}

void UTeamProjectGameInstance::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogTemp, Error, TEXT("NetworkFailure. Type: %d, Error: %s"), static_cast<int32>(FailureType), *ErrorString);

	bShowHostDisconnectedPopup = true;
	OpenMainMenuLevel();
}

void UTeamProjectGameInstance::OnReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	FriendIds.Empty();
	bFriendsListReady = bWasSuccessful;

	if (!bWasSuccessful || !FriendsInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read Steam friends list: %s"), *ErrorStr);
		return;
	}

	TArray<TSharedRef<FOnlineFriend>> Friends;
	if (!FriendsInterface->GetFriendsList(LocalUserNum, ListName, Friends))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetFriendsList failed."));
		return;
	}

	for (const TSharedRef<FOnlineFriend>& Friend : Friends)
	{
		FriendIds.Add(Friend->GetUserId()->ToString());
	}

	UE_LOG(LogTemp, Warning, TEXT("Steam friends loaded: %d"), FriendIds.Num());
}

bool UTeamProjectGameInstance::IsFriendSession(const FOnlineSessionSearchResult& SearchResult) const
{
	if (!bFriendsListReady) return false;
	if (!SearchResult.Session.OwningUserId.IsValid()) return false;

	const FString HostId = SearchResult.Session.OwningUserId->ToString();
	return FriendIds.Contains(HostId);
}

bool UTeamProjectGameInstance::IsProjectSession(const FOnlineSessionSearchResult& SearchResult) const
{
	FString ProjectValue;
	if (!SearchResult.Session.SessionSettings.Get(SESSION_PROJECT_KEY, ProjectValue)) return false;

	return ProjectValue == SESSION_PROJECT_VALUE;
}

bool UTeamProjectGameInstance::IsSteamLoggedIn() const
{
	IOnlineSubsystem* SteamOSS = IOnlineSubsystem::Get(FName(TEXT("STEAM")));
	if (!SteamOSS) return false;

	IOnlineIdentityPtr IdentityInterface = SteamOSS->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return false;

	return IdentityInterface->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}
