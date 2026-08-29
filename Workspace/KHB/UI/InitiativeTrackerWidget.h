// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InitiativeTrackerWidget.generated.h"

class UTurnSlotWidget;
class ABaseCharacter;

UCLASS()
class TEAMPROJECT_API UInitiativeTrackerWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable)
	void UpdateTurnOrder(ABaseCharacter* CurrentTurn, const TArray<ABaseCharacter*>& TurnOrder);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnSlotWidget> TurnSlot_0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnSlotWidget> TurnSlot_1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnSlotWidget> TurnSlot_2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnSlotWidget> TurnSlot_3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnSlotWidget> TurnSlot_4;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnSlotWidget> TurnSlot_5;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnSlotWidget> TurnSlot_6;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnSlotWidget> TurnSlot_7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TArray<FVector2D> SlotSizes;

private:
	static constexpr int32 MaxSlots = 8;

	UTurnSlotWidget* GetSlotByIndex(int32 Index) const;
};
