#include "LobbyPlayerSlotWidget.h"
#include "TeamProjectPlayerState.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void ULobbyPlayerSlotWidget::SetEmpty()
{
	if (Image_CharacterPortrait)
	{
		Image_CharacterPortrait->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Text_PlayerName)
	{
		Text_PlayerName->SetText(FText::FromString(TEXT("	")));
	}

	if (Text_CharacterName)
	{
		Text_CharacterName->SetText(FText::GetEmpty());
	}

	if (Text_ReadyState)
	{
		Text_ReadyState->SetText(FText::GetEmpty());
	}
}

void ULobbyPlayerSlotWidget::SetPlayerStateData(ATeamProjectPlayerState* TeamPS, const TArray<UTexture2D*>& CharacterPortraits, const TArray<FText>& CharacterNames)
{
	if (!TeamPS)
	{
		SetEmpty();
		return;
	}

	const int32 CharacterIndex = TeamPS->GetSelectedCharacterIndex();

	SetPlayerDisplayData(TeamPS->GetPlayerName(), CharacterIndex, TeamPS->IsReady(), CharacterPortraits, CharacterNames);
}

void ULobbyPlayerSlotWidget::SetPlayerDisplayData(const FString& PlayerName, int32 CharacterIndex, bool bReady, const TArray<UTexture2D*>& CharacterPortraits, const TArray<FText>& CharacterNames)
{
	if (Image_CharacterPortrait && CharacterPortraits.IsValidIndex(CharacterIndex))
	{
		Image_CharacterPortrait->SetBrushFromTexture(CharacterPortraits[CharacterIndex]);
		Image_CharacterPortrait->SetVisibility(ESlateVisibility::Visible);
	}

	if (Text_PlayerName)
	{
		Text_PlayerName->SetText(FText::FromString(PlayerName));
	}

	if (Text_CharacterName)
	{
		Text_CharacterName->SetText(CharacterNames.IsValidIndex(CharacterIndex) ? CharacterNames[CharacterIndex] : FText::FromString(TEXT("???")));
	}

	if (Text_ReadyState)
	{
		Text_ReadyState->SetText(bReady ? FText::FromString(TEXT("Ready")) : FText::FromString(TEXT("Not Ready")));
	}
}
