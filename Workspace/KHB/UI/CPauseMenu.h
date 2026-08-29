#pragma once

#include "CoreMinimal.h"
#include "CMenuBase.h"
#include "CPauseMenu.generated.h"

class UButton;

UCLASS()
class TEAMPROJECT_API UCPauseMenu : public UCMenuBase
{
	GENERATED_BODY()

protected:
	virtual bool Initialize() override;

private:
	UFUNCTION()
	void CancelPressed();

	UFUNCTION()
	void QuitPressed();

private:
	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

	UPROPERTY(meta = (BindWidget))
	UButton* QuitButton;
};
