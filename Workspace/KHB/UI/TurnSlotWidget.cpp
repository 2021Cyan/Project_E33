// Fill out your copyright notice in the Description page of Project Settings.


#include "TurnSlotWidget.h"
#include "../Character/BaseCharacter.h"
#include "Components/Image.h"
#include "Components/Overlay.h"

void UTurnSlotWidget::InitSlot(ABaseCharacter* Character, int32 SlotIndex, bool bIsLastSlot)
{
	if (!Character)return;

	const bool bIsPlayer = Character->IsPlayerControlled();

	Image_Portrait->SetBrushFromTexture(Character->TurnPortraitTexture);

	const FLinearColor TeamColor = bIsPlayer ? PlayerColor : MonsterColor;
	Image_BackGround->SetColorAndOpacity(TeamColor);
	Image_Frame->SetColorAndOpacity(TeamColor);

	if (bIsLastSlot)
	{
        UMaterialInstanceDynamic* PortraitMID = UMaterialInstanceDynamic::Create(GradientMaterial, this);
        PortraitMID->SetScalarParameterValue(TEXT("FadeAlpha"), FadeAlpha);
        PortraitMID->SetTextureParameterValue(TEXT("Texture"), Character->TurnPortraitTexture);
        Image_Portrait->SetBrushFromMaterial(PortraitMID);

        if (UTexture* BackgroundTexture = Cast<UTexture>(Image_BackGround->GetBrush().GetResourceObject()))
        {
            UMaterialInstanceDynamic* BackgroundMID = UMaterialInstanceDynamic::Create(GradientMaterial, this);
            BackgroundMID->SetScalarParameterValue(TEXT("FadeAlpha"), FadeAlpha);
            BackgroundMID->SetTextureParameterValue(TEXT("Texture"), BackgroundTexture);
            Image_BackGround->SetBrushFromMaterial(BackgroundMID);
        }

        if (UTexture* FrameTexture = Cast<UTexture>(Image_Frame->GetBrush().GetResourceObject()))
        {
            UMaterialInstanceDynamic* FrameMID = UMaterialInstanceDynamic::Create(GradientMaterial, this);
            FrameMID->SetScalarParameterValue(TEXT("FadeAlpha"), FadeAlpha);
            FrameMID->SetTextureParameterValue(TEXT("Texture"), FrameTexture);
            Image_Frame->SetBrushFromMaterial(FrameMID);
        }
	}
	else if (SlotIndex == 6)
	{
		Overlay_TurnSlot->SetRenderOpacity(0.5f);
	}

	if (bIsPlayer)
		BP_PlayPlayerAnimation();
	else
		BP_PlayMonsterAnimation();
}

