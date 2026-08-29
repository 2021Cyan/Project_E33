#include "CMainMenu.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "../Core/TeamProjectGameInstance.h"
#include "CServerRow.h"

UCMainMenu::UCMainMenu()
{
	ConstructorHelpers::FClassFinder<UUserWidget> ServerRowClass_Asset(TEXT("/Game/ThirdPerson/Blueprints/Widgets/WBP_ServerRow"));
	if (ServerRowClass_Asset.Succeeded())
	{
		ServerRowClass = ServerRowClass_Asset.Class;
	}
}

bool UCMainMenu::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (!bSuccess) return false;

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UCMainMenu::OpenCreateRoomMenu);
	}

	if (!JoinButton) return false;
	JoinButton->OnClicked.AddDynamic(this, &UCMainMenu::OpenJoinMenu);

	if (!BackButton) return false;
	BackButton->OnClicked.AddDynamic(this, &UCMainMenu::OpenMainMenu);

	if (!QuitButton) return false;
	QuitButton->OnClicked.AddDynamic(this, &UCMainMenu::QuitGame);

	if (!CreateRoomButton) return false;
	CreateRoomButton->OnClicked.AddDynamic(this, &UCMainMenu::OpenCreateRoomMenu);

	if (!ConfirmJoinButton) return false;
	ConfirmJoinButton->OnClicked.AddDynamic(this, &UCMainMenu::JoinServer);

	if (!ListRefreshButton) return false;
	ListRefreshButton->OnClicked.AddDynamic(this, &UCMainMenu::RefreshServerList);

	if (CreateRoomMenu)
	{
		CreateRoomMenu->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!CancelCreateRoomButton) return false;
	CancelCreateRoomButton->OnClicked.AddDynamic(this, &UCMainMenu::CloseCreateRoomMenu);

	if (!ConfirmHostButton) return false;
	ConfirmHostButton->OnClicked.AddDynamic(this, &UCMainMenu::HostServer);

	if (Button_CloseHostDisconnectedPopup)
	{
		Button_CloseHostDisconnectedPopup->OnClicked.AddDynamic(this, &UCMainMenu::CloseHostDisconnectedPopup);
	}

	if (UTeamProjectGameInstance* GI = GetGameInstance<UTeamProjectGameInstance>())
	{
		if (GI->bShowHostDisconnectedPopup && SizeBox_HostDisconnectedPopup)
		{
			SizeBox_HostDisconnectedPopup->SetVisibility(ESlateVisibility::Visible);
			GI->bShowHostDisconnectedPopup = false;
		}
		else
		{
			if (SizeBox_HostDisconnectedPopup)
			{
				SizeBox_HostDisconnectedPopup->SetVisibility(ESlateVisibility::Hidden);
				GI->bShowHostDisconnectedPopup = false;
			}
		}
	}

	return true;
}

void UCMainMenu::HostServer()
{
	if (!ServerHostName) return;

	FString ServerName = ServerHostName->Text.ToString();
	ServerName.TrimStartAndEndInline();
	if (ServerName.IsEmpty()) return;

	OwningInstance->Host(ServerName);
}

void UCMainMenu::OpenMainMenu()
{
	if (bCreateRoomMenuOpen) return;

	if (!MenuSwitcher) return;
	if (!MainMenu) return;
	MenuSwitcher->SetActiveWidget(MainMenu);

	if (Image_MainMenuTitle)
	{
		Image_MainMenuTitle->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCMainMenu::OpenJoinMenu()
{
	if (!MenuSwitcher) return;
	if (!JoinMenu) return;
	MenuSwitcher->SetActiveWidget(JoinMenu);

	if (Image_MainMenuTitle)
	{
		Image_MainMenuTitle->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (OwningInstance)
	{
		OwningInstance->RefreshServerList();
	}
}

void UCMainMenu::RefreshServerList()
{
	if (OwningInstance)
	{
		OwningInstance->RefreshServerList();
	}
}

void UCMainMenu::OpenCreateRoomMenu()
{
	if (!CreateRoomMenu) return;
	if (ServerHostName)
	{
		ServerHostName->SetText(FText::GetEmpty());
	}

	bCreateRoomMenuOpen = true;
	CreateRoomMenu->SetVisibility(ESlateVisibility::Visible);
}

void UCMainMenu::CloseCreateRoomMenu()
{
	if (!CreateRoomMenu) return;
	if (ServerHostName)
	{
		ServerHostName->SetText(FText::GetEmpty());
	}

	bCreateRoomMenuOpen = false;
	CreateRoomMenu->SetVisibility(ESlateVisibility::Collapsed);
}

void UCMainMenu::JoinServer()
{
	if (bCreateRoomMenuOpen) return;

	if (SelectedIndex.IsSet() && OwningInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Selected index is %d."), SelectedIndex.GetValue());
		OwningInstance->Join(SelectedIndex.GetValue());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Selected index is not set."));
	}
}

void UCMainMenu::QuitGame()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	PC->ConsoleCommand("quit");
}

void UCMainMenu::CloseHostDisconnectedPopup()
{
	if (SizeBox_HostDisconnectedPopup)
	{
		SizeBox_HostDisconnectedPopup->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UCMainMenu::SetServerList(TArray<FServerData> InServerDatas)
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (!ServerList) return;
	SelectedIndex.Reset();
	ServerList->ClearChildren();

	uint32 i = 0;
	for (const FServerData& ServerData : InServerDatas)
	{
		UCServerRow* ServerRow = CreateWidget<UCServerRow>(World, ServerRowClass);
		if (!ServerRow) return;

		ServerRow->ServerName->SetText(FText::FromString(ServerData.Name));
		ServerRow->HostUser->SetText(FText::FromString(ServerData.HostUserName));

		FString FractionText = FString::Printf(TEXT("%d/%d"), ServerData.CurrentPlayers, ServerData.MaxPlayers);
		ServerRow->ConnectionFraction->SetText(FText::FromString(FractionText));

		ServerRow->Setup(this, i++);

		ServerList->AddChild(ServerRow);
	}
}

void UCMainMenu::SetSelectedIndex(uint32 InIndex)
{
	SelectedIndex = InIndex;

	for (int32 i = 0; i < ServerList->GetChildrenCount(); i++)
	{
		UCServerRow* ServerRow = Cast<UCServerRow>(ServerList->GetChildAt(i));
		if (ServerRow)
		{
			ServerRow->SetSelected(SelectedIndex.IsSet() && SelectedIndex.GetValue() == i);
		}
	}
}
