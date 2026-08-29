// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyStatWidget.h"
#include "../../KHB/Character/MonsterCharacter.h"
#include "../../KHB/Component/StatComponent.h"

void UEnemyStatWidget::Init(AMonsterCharacter* InMonster)
{
    if (!InMonster)
    {
        return;
    }

    StatComponent = InMonster->FindComponentByClass<UStatComponent>();
    if (!StatComponent)
    {
        return;
    }

    StatComponent->OnStatChanged.RemoveDynamic(
        this,
        &UEnemyStatWidget::HandleStatChanged
    );

    StatComponent->OnStatChanged.AddDynamic(
        this,
        &UEnemyStatWidget::HandleStatChanged
    );

    RefreshInitial();
}

void UEnemyStatWidget::HandleStatChanged(ECharacterStatType StatType, float NewValue)
{
    BroadcastStatChanged(StatType, NewValue);
}

void UEnemyStatWidget::RefreshInitial()
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

void UEnemyStatWidget::BroadcastStatChanged(ECharacterStatType StatType, float NewValue)
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

void UEnemyStatWidget::NativeDestruct()
{
    if (StatComponent)
    {
        StatComponent->OnStatChanged.RemoveDynamic(
            this,
            &UEnemyStatWidget::HandleStatChanged
        );
    }

    Super::NativeDestruct();
}