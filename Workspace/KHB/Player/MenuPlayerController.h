#pragma once

#include "CoreMinimal.h"
#include "BasePlayerController.h"
#include "../UI/CMainMenu.h"
#include "MenuPlayerController.generated.h"

UCLASS()
class TEAMPROJECT_API AMenuPlayerController : public ABasePlayerController
{
	GENERATED_BODY()

public:
	AMenuPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	void UpdateServerList(TArray<FServerData> InServerDatas);
	void HideMainMenu();

private:
	UPROPERTY()
	UCMainMenu* MainMenu;

	TSubclassOf<UUserWidget> MainMenuWidgetClass;
};
