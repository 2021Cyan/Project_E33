#pragma once

#include "CoreMinimal.h"
#include "CMenuBase.h"
#include "CMainMenu.generated.h"

class UButton;
class UWidgetSwitcher;
class UWidget;
class UPanelWidget;
class UEditableTextBox;
class USizeBox;

USTRUCT()
struct FServerData
{
	GENERATED_BODY()

public:
	FString Name;
	uint16 CurrentPlayers;
	uint16 MaxPlayers;
	FString HostUserName;
};

UCLASS()
class TEAMPROJECT_API UCMainMenu : public UCMenuBase
{
	GENERATED_BODY()

public:
	UCMainMenu();

protected:
	virtual bool Initialize() override;

private:
	UFUNCTION()
	void HostServer();

	UFUNCTION()
	void OpenMainMenu();

	UFUNCTION()
	void OpenJoinMenu();
	
	UFUNCTION()
	void RefreshServerList();

	UFUNCTION()
	void OpenCreateRoomMenu();

	UFUNCTION()
	void CloseCreateRoomMenu();

	UFUNCTION()
	void JoinServer();

	UFUNCTION()
	void QuitGame();

	UFUNCTION()
	void CloseHostDisconnectedPopup();

public:
	void SetServerList(TArray<FServerData> InServerDatas);
	void SetSelectedIndex(uint32 InIndex);

private:
	UPROPERTY(meta = (BindWidget))
	UWidgetSwitcher* MenuSwitcher;

	UPROPERTY(meta = (BindWidget))
	UWidget* MainMenu;

	UPROPERTY(meta = (BindWidget))
	UWidget* JoinMenu;

	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* Image_MainMenuTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* HostButton;

	UPROPERTY(meta = (BindWidget))
	UButton* QuitButton;

	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;

	UPROPERTY(meta = (BindWidget))
	UButton* BackButton;

	UPROPERTY(meta = (BindWidget))
	UButton* ListRefreshButton;

	UPROPERTY(meta = (BindWidget))
	UPanelWidget* ServerList;

	TSubclassOf<UUserWidget> ServerRowClass;

	UPROPERTY(meta = (BindWidget))
	UButton* CreateRoomButton;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmJoinButton;

	UPROPERTY(meta = (BindWidget))
	UWidget* CreateRoomMenu;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* ServerHostName;

	UPROPERTY(meta = (BindWidget))
	UButton* CancelCreateRoomButton;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmHostButton;

	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* SizeBox_HostDisconnectedPopup;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Button_CloseHostDisconnectedPopup;

private:
	TOptional<uint32> SelectedIndex;
	bool bCreateRoomMenuOpen = false;
};
