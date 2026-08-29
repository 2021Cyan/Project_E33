#include "CPauseMenu.h"
#include "Components/Button.h"

bool UCPauseMenu::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (!bSuccess) return false;

	if (!CancelButton) return false;
	CancelButton->OnClicked.AddDynamic(this, &UCPauseMenu::CancelPressed);

	if (!QuitButton) return false;
	QuitButton->OnClicked.AddDynamic(this, &UCPauseMenu::QuitPressed);

	return true;
}

void UCPauseMenu::CancelPressed()
{
	Shutdown();
}

void UCPauseMenu::QuitPressed()
{
	if (!OwningInstance) return;

	Shutdown();
	OwningInstance->OpenMainMenuLevel();
}
