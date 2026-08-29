#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../KHB/Component/StatComponent.h"
#include "PlayerStatSlotWidget.generated.h"

class APlayerCharacter;
class UTexture2D;

UCLASS()
class TEAMPROJECT_API UPlayerStatSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "Player")
    TObjectPtr<APlayerCharacter> PlayerChar;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void InitSlot(APlayerCharacter* InPlayer);

protected:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

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

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void BP_OnPortraitChanged(UTexture2D* PortraitTexture);

private:
    void RefreshInitial();
    void UnbindStatComponent();
    void BroadcastStatChanged(ECharacterStatType StatType, float NewValue);

private:
    UPROPERTY()
    TObjectPtr<UStatComponent> StatComponent;
};