#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BasePlayerController.generated.h"

class UInputMappingContext;
class UBattleCameraSequenceData;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class ABattleManager;
class AMonsterCharacter;
class UQTEWidget;

UCLASS()
class TEAMPROJECT_API ABasePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
    void StopPawnMovement(APawn* PlayerPawn);
    virtual void BeginPlay() override;
    virtual void AcknowledgePossession(APawn* InPawn) override;

public:
    void SwitchInputMode(UInputMappingContext* NewIMC);
    void SwitchToExplorationInput();
    void SwitchToBattleInput();

    UFUNCTION(Client, Reliable)
    void Client_PrepareDungeonEnter();

    UFUNCTION(Client, Reliable)
    void Client_LoadStreamLevel(const FName& LevelName);

    UFUNCTION(Client, Reliable)
    void Client_UnLoadStreamLevel(const FName& LevelName);

    UFUNCTION(Client, Reliable)
    void Client_MoveToSpawnPoint(FVector InSpawnLocation, FRotator InSpawnRotation);

    UFUNCTION(Client, Reliable)
    void Client_PrepareDungeonExit();

    UFUNCTION(Client, Reliable)
    void Client_ReturnToOverworld(FVector InReturnLocation, FRotator InReturnRotation);

    UFUNCTION(Server, Reliable)
    void Server_NotifyLevelLoaded(const FName& LevelName);

    UFUNCTION(Server, Reliable)
    void Server_NotifyLevelUnloaded(const FName& LevelName);

    UFUNCTION(Server, Reliable)
    void Server_NotifySpawnComplete(const FName& LevelName);

    UFUNCTION(Server, Reliable)
    void Server_NotifyBattleStartMontageFinished();

    UFUNCTION()
    void OnLevelLoaded();

    UFUNCTION()
    void OnLevelUnloaded();

    UFUNCTION(Client, Reliable)
    void Client_ApplySharedBattleCamera(AActor* BattleCamera, float BlendTime);

    UFUNCTION(Client, Reliable)
    void Client_PlaySharedBattleCamera(AActor* BattleCamera, UBattleCameraSequenceData* SequenceData);

    UFUNCTION(Client, Reliable)
    void Client_MoveSharedBattleCameraEnemyTurn(AActor* BattleCamera, AMonsterCharacter* Monster, AActor* TargetActor, FVector CameraLocation, FRotator CameraRotation, FVector LookTarget, bool bUseAreaSpineLookBlend);

    UFUNCTION(Client, Reliable)
    void Client_ReturnSharedBattleCameraToBase(AActor* BattleCamera);

    UFUNCTION(Client, Reliable)
    void Client_PlaySharedBattleCameraImpact(AActor* BattleCamera);

    UFUNCTION(Client, Reliable)
    void Client_PlayCounterLevelSequence(ULevelSequence* LevelSequence, AActor* OriginActor, FVector OriginLocation, FRotator OriginRotation, bool bRestoreOriginAfterPlayback);

    UFUNCTION(Client, Reliable)
    void Client_StopCounterLevelSequence();

    UFUNCTION(Client, Reliable)
    void Client_ReturnToPersonalBattleCamera(float BlendTime);

    UFUNCTION(Client, Reliable)
    void Client_ReturnToExplorationCamera(float BlendTime);

    void SetPendingLevelName(FName LevelName) { PendingLevelName = LevelName; }
    FName GetPendingLevelName() const { return PendingLevelName; }

    UFUNCTION(Client, Reliable)
    void Client_ShowAttackQTE(float Duration, float SuccessStartTime, float SuccessEndTime, FVector2D ScreenPosition);

    // 다른 플레이어 턴 — UI 표시만, 판정 없음
    UFUNCTION(Client, Reliable)
    void Client_ShowQTESpectator(float Duration, FVector2D ScreenPosition);
    UFUNCTION(Client, Reliable)
    void Client_OnQTEResult(bool bSuccess);

    UFUNCTION(Server, Reliable)
    void Server_SubmitQTE(bool bSuccess);

    TObjectPtr<UQTEWidget> GetCurrentQTEWidget() const { return CurrentQTEWidget; }

    UFUNCTION(Client, Reliable)
    void Client_UpdateInitiativeTracker(ABaseCharacter* CurrentTurn, const TArray<ABaseCharacter*>& TurnOrder);

    UFUNCTION(Client, Reliable)
    void Client_HideInitiativeTracker();

    UPROPERTY(BlueprintReadWrite, Category = "HUD")
    TObjectPtr<UUserWidget> BattleHUDWidget;

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetBattleHUDWidget(UUserWidget* InBattleHUDWidget);
private:
    void ApplyGameInputMode();
    void StopCounterLevelSequenceLocal();

    FName PendingLevelName;
    FName PendingUnloadLevelName;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputMappingContext> IMC_Exploration;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputMappingContext> IMC_Battle;

    TObjectPtr<UInputMappingContext> CurrentIMC;

    UPROPERTY()
    bool bIsUsingSharedCamera = false;

    UPROPERTY(EditDefaultsOnly, Category = "Battle Camera|Counter")
    FName CounterLevelSequenceBindingName = TEXT("BP_LSActor");

    UPROPERTY(EditDefaultsOnly, Category = "QTE")
    TSubclassOf<UQTEWidget> QTEWidgetClass;

    UPROPERTY()
    TObjectPtr<UQTEWidget> CurrentQTEWidget;

    UPROPERTY()
    TObjectPtr<ALevelSequenceActor> ActiveCounterSequenceActor;

    UPROPERTY()
    TObjectPtr<ULevelSequencePlayer> ActiveCounterSequencePlayer;

    TWeakObjectPtr<AActor> ActiveCounterSequenceOriginActor;
    FVector ActiveCounterSequenceOriginalOriginLocation = FVector::ZeroVector;
    FRotator ActiveCounterSequenceOriginalOriginRotation = FRotator::ZeroRotator;
    bool bActiveCounterSequenceRestoresOrigin = false;
};
