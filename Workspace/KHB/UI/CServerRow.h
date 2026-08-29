#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CServerRow.generated.h"

class UTextBlock;
class UCMainMenu;
class UButton;

UCLASS()
class TEAMPROJECT_API UCServerRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UCMainMenu* InParent, uint32 InIndex);
	void SetSelected(bool bNewSelected);

private:
	UFUNCTION()
	void OnClicked();

	UFUNCTION()
	void OnHovered();

	UFUNCTION()
	void OnUnhovered();

	UFUNCTION()
	void OnPressed();

	UFUNCTION()
	void OnReleased();

	void RefreshVisualState();
	void ApplyTextStyle(const FLinearColor& Color, bool bBold);
public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ServerName;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HostUser;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ConnectionFraction;

private:
	UPROPERTY(meta = (BindWidget))
	UButton* RowButton;

	UPROPERTY()
	UCMainMenu* Parent;

	uint32 SelfIndex = 0;

	bool bSelected = false;
	bool bHovered = false;
	bool bPressed = false;
};
