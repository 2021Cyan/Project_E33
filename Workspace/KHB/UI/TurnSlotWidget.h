// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnSlotWidget.generated.h"

class ABaseCharacter;
class UImage;
class UOverlay;

UCLASS()
class TEAMPROJECT_API UTurnSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitSlot(ABaseCharacter* Character, int32 SlotIndex, bool bIsLastSlot);

protected:
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UOverlay> Overlay_TurnSlot;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UImage> Image_Portrait;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UImage> Image_Frame;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UImage> Image_BackGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FLinearColor PlayerColor = FLinearColor(0.f, 0.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FLinearColor MonsterColor = FLinearColor(1.f, 0.f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TObjectPtr<UMaterialInterface> GradientMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	float FadeAlpha = 0.35f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Slot")
	void BP_PlayPlayerAnimation();

	UFUNCTION(BlueprintImplementableEvent, Category = "Slot")
	void BP_PlayMonsterAnimation();

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GradientMID;
};
