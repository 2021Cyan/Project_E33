#include "CMenuBase.h"

void UCMenuBase::SetOwningInstance(ICMenuInterface* InInstance)
{
	OwningInstance = InInstance;
}

void UCMenuBase::Startup()
{
	AddToViewport(10);
	bIsFocusable = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}

void UCMenuBase::Shutdown()
{
	RemoveFromViewport();
	bIsFocusable = false;

	FInputModeGameOnly InputMode;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = false;
}
