#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "Engine/TimerHandle.h"
#include "PlayerCharacter.generated.h"

class UInputAction;
class ULevelSequence;
struct FInputActionValue;
class USpringArmComponent;
class UCameraComponent;
class AMonsterCharacter;
  
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerTurnStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerTurnEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerEnterBattle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerExitBattle);

UCLASS()
class TEAMPROJECT_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
    APlayerCharacter();

protected:
    virtual void BeginPlay() override;

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    virtual void Tick(float DeltaTime) override;

    virtual void OnRep_ReplicatedBattleManager() override;

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void OnQTEInput(const FInputActionValue& Value);

    void UpdateHandheldShake(float DeltaTime);
    void ApplyCameraShake();
    void ResetCameraShake();

public:
    UFUNCTION(Client, Reliable)
    void Client_EnterBattleMode();

    UFUNCTION(Client, Reliable)
    void Client_ExitBattleMode();

    UFUNCTION(Client, Reliable)
    void Client_StartPlayerTurn();  

    UFUNCTION(Client, Reliable)
    void Client_StartDefenseTurn();

    UFUNCTION(Client, Reliable)
    void Client_EndPlayerTurn();

    void OnBattleEntryMontageFinished();
    void UpdateBattleInputState();

    void AddQTEResult(bool bSuccess);
    ULevelSequence* GetSingleCounterLevelSequence() const { return SingleCounterLevelSequence; }

    UFUNCTION(BlueprintCallable, Category = "Battle|UI")
    bool ShowBossHealthBarForCurrentBattle();

private:
    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> IA_Move;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> IA_Look;

    UPROPERTY(EditAnywhere, Category = "QTE")
    TObjectPtr<UInputAction> IA_QTE;

    UPROPERTY(EditAnywhere, Category = "Battle|Counter")
    TObjectPtr<ULevelSequence> SingleCounterLevelSequence;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    bool bUseHandHeldShake = false;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    float ShakeSpeed = 0.25f;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    float LocationShakeAmount = 6.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    float RotationShakeAmount = 2.0f;

    float ShakeTime = 0.0f;

    FVector DefaultCameraRelativeLocation = FVector::ZeroVector;
    FRotator DefaultCameraRelativeRotation = FRotator::ZeroRotator;

    FVector ShakeLocationOffset = FVector::ZeroVector;
    FRotator ShakeRotationOffset = FRotator::ZeroRotator;

    bool bPendingEnterBattleBroadcast = false;
    bool bEnterBattleBroadcasted = false;

    void TryBroadcastEnterBattle();

    // 보스 체력바: 전투 진입 직후 보스 몬스터가 클라에 복제되기 전이면 표시에 실패하므로,
    // 복제 도착까지 제한된 횟수만큼 재시도한다.
    void RetryShowBossHealthBar();
    FTimerHandle BossHealthBarRetryTimer;
    int32 BossHealthBarRetryCount = 0;
    static constexpr int32 MaxBossHealthBarRetries = 15;
public:
    UPROPERTY(BlueprintAssignable)
    FOnPlayerTurnStart OnPlayerTurnStart;

    UPROPERTY(BlueprintAssignable)
    FOnPlayerTurnEnd OnPlayerTurnEnd;

    UPROPERTY(BlueprintAssignable)
    FOnPlayerEnterBattle OnPlayerEnterBattle;

    UPROPERTY(BlueprintAssignable)
    FOnPlayerExitBattle OnPlayerExitBattle;

    UFUNCTION(BlueprintCallable, Category = "Camera|Shake")
    void SetHandheldShakeEnabled(bool bEnabled);

    UFUNCTION(BlueprintImplementableEvent, Category = "Battle")
    void ReceiveEnterBattleMode();

    UFUNCTION(BlueprintImplementableEvent, Category = "Battle")
    void ReceiveExitBattleMode();

    UFUNCTION(BlueprintImplementableEvent, Category = "Battle|UI")
    void ReceiveShowBossHealthBar(AMonsterCharacter* BossMonster);
};
