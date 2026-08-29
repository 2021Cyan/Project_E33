#include "PlayerStatSlotWidget.h"
#include "../../KHB/Character/PlayerCharacter.h"
#include "../../KHB/Component/StatComponent.h"

void UPlayerStatSlotWidget::InitSlot(APlayerCharacter* InPlayer)
{
    UnbindStatComponent();
    PlayerChar = InPlayer;
    StatComponent = nullptr;

    if (!PlayerChar) {
        return;
    }

    StatComponent = PlayerChar->GetStatComponent();
    if (!StatComponent)
    {
        return;
    }

    StatComponent->OnStatChanged.AddDynamic(
        this,
        &UPlayerStatSlotWidget::HandleStatChanged
    );
    RefreshInitial();
    BP_OnPortraitChanged(PlayerChar->PortraitTexture);
}

void UPlayerStatSlotWidget::UnbindStatComponent()
{
    if (StatComponent) {
        StatComponent->OnStatChanged.RemoveDynamic(
            this,
            &UPlayerStatSlotWidget::HandleStatChanged
        );
    }
}

void UPlayerStatSlotWidget::HandleStatChanged(ECharacterStatType StatType, float NewValue)
{
    BroadcastStatChanged(StatType, NewValue);
}

void UPlayerStatSlotWidget::RefreshInitial()
{
    if (!StatComponent)
    {
        return;
    }

    const float HP = StatComponent->GetHP();
    const float MaxHP = StatComponent->GetMaxHP();
    const float AP = StatComponent->GetAP();
    const float MaxAP = StatComponent->GetMaxAP();

    const float HPPercent = MaxHP > 0.f ? HP / MaxHP : 0.f;
    const float APPercent = MaxAP > 0.f ? AP / MaxAP : 0.f;

    BP_OnStatInitialized(
        HP,
        MaxHP,
        HPPercent,
        AP,
        MaxAP,
        APPercent
    );
}

void UPlayerStatSlotWidget::BroadcastStatChanged(ECharacterStatType StatType, float NewValue)
{
    if (!StatComponent)
    {
        return;
    }

    const float HP = StatComponent->GetHP();
    const float MaxHP = StatComponent->GetMaxHP();
    const float AP = StatComponent->GetAP();
    const float MaxAP = StatComponent->GetMaxAP();

    const float HPPercent = MaxHP > 0.f ? HP / MaxHP : 0.f;
    const float APPercent = MaxAP > 0.f ? AP / MaxAP : 0.f;

    BP_OnStatChanged(
        StatType,
        NewValue,
        HP,
        MaxHP,
        HPPercent,
        AP,
        MaxAP,
        APPercent
    );
}

void UPlayerStatSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();
    InitSlot(PlayerChar);
}

void UPlayerStatSlotWidget::NativeDestruct()
{
    UnbindStatComponent();
    StatComponent = nullptr;
    Super::NativeDestruct();
}