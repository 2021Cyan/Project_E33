// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../KHB/Component/StatComponent.h"
#include "EnemyStatWidget.generated.h"

class AMonsterCharacter;
/**
 * 
 */
UCLASS()
class TEAMPROJECT_API UEnemyStatWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "Player")
	TObjectPtr<AMonsterCharacter> MonsterChar;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void Init(AMonsterCharacter* InMonster);
protected:
    UFUNCTION()
    void HandleStatChanged(ECharacterStatType StatType, float NewValue);

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void BP_OnStatInitialized(
        float HP,
        float MaxHP,
        float HPPercent,
        float AP,
        float MaxAP,
        float APPercent
    );

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void BP_OnStatChanged(
        ECharacterStatType StatType,
        float NewValue,
        float HP,
        float MaxHP,
        float HPPercent,
        float AP,
        float MaxAP,
        float APPercent
    );
    virtual void NativeDestruct() override;
private:
    void RefreshInitial();
    void BroadcastStatChanged(ECharacterStatType StatType, float NewValue);

private:
    UPROPERTY()
    UStatComponent* StatComponent;
};
