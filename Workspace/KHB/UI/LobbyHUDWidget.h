// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyHUDWidget.generated.h"

class UButton;
class UTexture2D;
class ULobbyPlayerSlotWidget;

UCLASS()
class TEAMPROJECT_API ULobbyHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void RefreshLobbySlots();

protected:
	virtual void NativeConstruct() override;

private:
	void ApplyLocalSelectedCharacter(int32 NewIndex);
	void ApplyLocalReady(bool bNewReady);

	UFUNCTION()
	void OnClickedLune();

	UFUNCTION()
	void OnClickedMaelle();

	UFUNCTION()
	void OnClickedVerso();

	UFUNCTION()
	void OnClickedReady();

	UFUNCTION()
	void OnClickedExit();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULobbyPlayerSlotWidget> Slot_Player0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULobbyPlayerSlotWidget> Slot_Player1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULobbyPlayerSlotWidget> Slot_Player2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULobbyPlayerSlotWidget> Slot_Player3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Lune;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Maelle;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Verso;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Ready;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Exit;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TArray<UTexture2D*> CharacterPortraits;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TArray<FText> CharacterNames;

	bool bHasLocalSelectedCharacterPreview = false;
	int32 LocalSelectedCharacterPreview = INDEX_NONE;

	bool bHasLocalReadyPreview = false;
	bool bLocalReadyPreview = false;
};
