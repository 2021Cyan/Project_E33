// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyPlayerSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class ATeamProjectPlayerState;

UCLASS()
class TEAMPROJECT_API ULobbyPlayerSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetEmpty();
	void SetPlayerStateData(ATeamProjectPlayerState* TeamPS, const TArray<UTexture2D*>& CharacterPortraits, const TArray<FText>& CharacterNames);
	void SetPlayerDisplayData(const FString& PlayerName, int32 CharacterIndex, bool bReady, const TArray<UTexture2D*>& CharacterPortraits, const TArray<FText>& CharacterNames);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_CharacterPortrait;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_CharacterName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ReadyState;
};
