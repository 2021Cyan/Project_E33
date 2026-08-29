#include "MenuPlayerController.h"
#include "../Core/TeamProjectGameInstance.h"

AMenuPlayerController::AMenuPlayerController()
{
	ConstructorHelpers::FClassFinder<UUserWidget> MainMenuWidgetClass_Asset(TEXT("/Game/ThirdPerson/Blueprints/Widgets/WBP_MainMenu"));
	if (MainMenuWidgetClass_Asset.Succeeded())
	{
		MainMenuWidgetClass = MainMenuWidgetClass_Asset.Class;
	}
}

void AMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) return;
	if (!ensure(MainMenuWidgetClass)) return;

	UTeamProjectGameInstance* GI = Cast<UTeamProjectGameInstance>(GetGameInstance());
	if (!GI) return;

	MainMenu = CreateWidget<UCMainMenu>(this, MainMenuWidgetClass);
	if (!MainMenu) return;

	MainMenu->SetOwningInstance(GI);
	MainMenu->Startup();
}

void AMenuPlayerController::UpdateServerList(TArray<FServerData> InServerDatas)
{
	if (!MainMenu) return;
	MainMenu->SetServerList(InServerDatas);
}

void AMenuPlayerController::HideMainMenu()
{
	if (!MainMenu) return;
	MainMenu->Shutdown();
}
