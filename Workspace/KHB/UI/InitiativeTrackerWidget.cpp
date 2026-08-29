#include "InitiativeTrackerWidget.h"
#include "TurnSlotWidget.h"
#include "../Character/BaseCharacter.h"

void UInitiativeTrackerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Hidden);
}

UTurnSlotWidget* UInitiativeTrackerWidget::GetSlotByIndex(int32 Index) const
{
    switch (Index)
    {
    case 0: return TurnSlot_0;
    case 1: return TurnSlot_1;
    case 2: return TurnSlot_2;
    case 3: return TurnSlot_3;
    case 4: return TurnSlot_4;
    case 5: return TurnSlot_5;
    case 6: return TurnSlot_6;
    case 7: return TurnSlot_7;
    default: return nullptr;
    }
}

void UInitiativeTrackerWidget::UpdateTurnOrder(ABaseCharacter* CurrentTurn, const TArray<ABaseCharacter*>& TurnOrder)
{
    if (TurnOrder.IsEmpty()) return;

    const int32 Total = TurnOrder.Num();
    int32 StartIdx = TurnOrder.IndexOfByKey(CurrentTurn);
    if (StartIdx == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] InitiativeTracker: CurrentTurnNotFound Current=%s TurnOrder=%d"),
            CurrentTurn ? *CurrentTurn->GetName() : TEXT("None"),
            TurnOrder.Num());
        StartIdx = 0;
    }

    for (int32 i = 0; i < MaxSlots; i++)
    {
        UTurnSlotWidget* TurnSlot = GetSlotByIndex(i);
        if (!TurnSlot) continue;

        ABaseCharacter* Character = TurnOrder[(StartIdx + i) % Total];
        const bool bIsLastSlot = (i == MaxSlots - 1);
        TurnSlot->InitSlot(Character, i, bIsLastSlot);
    }
}
