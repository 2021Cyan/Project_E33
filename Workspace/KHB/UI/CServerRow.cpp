#include "CServerRow.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "CMainMenu.h"

void UCServerRow::Setup(UCMainMenu* InParent, uint32 InIndex)
{
	Parent = InParent;
	SelfIndex = InIndex;

	if (RowButton)
	{
		RowButton->OnClicked.AddDynamic(this, &UCServerRow::OnClicked);
		RowButton->OnHovered.AddDynamic(this, &UCServerRow::OnHovered);
		RowButton->OnUnhovered.AddDynamic(this, &UCServerRow::OnUnhovered);
		RowButton->OnPressed.AddDynamic(this, &UCServerRow::OnPressed);
		RowButton->OnReleased.AddDynamic(this, &UCServerRow::OnReleased);
	}

	RefreshVisualState();
}

void UCServerRow::SetSelected(bool bNewSelected)
{
	bSelected = bNewSelected;
	RefreshVisualState();
}

void UCServerRow::OnClicked()
{
	if (Parent)
	{
		Parent->SetSelectedIndex(SelfIndex);
	}
}

void UCServerRow::OnHovered()
{
	bHovered = true;
	RefreshVisualState();
}

void UCServerRow::OnUnhovered()
{
	bHovered = false;
	bPressed = false;
	RefreshVisualState();
}

void UCServerRow::OnPressed()
{
	bPressed = true;
	RefreshVisualState();
}

void UCServerRow::OnReleased()
{
	bPressed = false;
	RefreshVisualState();
}

void UCServerRow::RefreshVisualState()
{
	if (bPressed)
	{
		ApplyTextStyle(FLinearColor(0.85f, 0.65f, 0.35f, 1.f), true);
		return;
	}

	if (bSelected)
	{
		ApplyTextStyle(FLinearColor(1.f, 0.82f, 0.45f, 1.f), true);
		return;
	}

	if (bHovered)
	{
		ApplyTextStyle(FLinearColor(0.9f, 0.9f, 0.9f, 1.f), true);
		return;
	}

	ApplyTextStyle(FLinearColor(0.65f, 0.65f, 0.65f, 1.f), false);
}

void UCServerRow::ApplyTextStyle(const FLinearColor& Color, bool bBold)
{
	TArray<UTextBlock*> Texts = {
		ServerName,
		HostUser,
		ConnectionFraction
	};

	for (UTextBlock* Text : Texts)
	{
		if (!Text)
		{
			continue;
		}

		Text->SetColorAndOpacity(FSlateColor(Color));

		FSlateFontInfo FontInfo = Text->GetFont();
		FontInfo.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
		Text->SetFont(FontInfo);
	}
}