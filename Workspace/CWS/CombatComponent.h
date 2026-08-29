#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "CombatComponent.generated.h"

class UStatComponent;
class ABattleManager;
class ABaseCharacter;

UENUM(BlueprintType)
enum class EEffectSpawnType : uint8
{
    HandCasting UMETA(DisplayName = "Hand Casting"), 
    Casting      UMETA(DisplayName = "Casting"),       
    Eruption    UMETA(DisplayName = "Eruption"),   
    Projectile  UMETA(DisplayName = "Projectile"), 
    Impact      UMETA(DisplayName = "Impact")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnTargetChanged,
    AActor*, NewTarget
);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_API UCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // =======================================================================
    // [Data & Animation & VFX] 전투 에셋 세팅
    // =======================================================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Data")
    UDataTable* SkillDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
    UAnimMontage* DefaultHitMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
    UAnimMontage* ParryMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
    UAnimMontage* StunMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
    UAnimMontage* BattleStartMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
    UAnimMontage* DashOutMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|VFX")
    UNiagaraSystem* ParrySparkEffect;

    // =======================================================================
    // [State & Replication] 전투 상태 및 동기화
    // =======================================================================
    UFUNCTION(BlueprintCallable, Category = "Combat|State")
    ECombatState GetCurrentState() const { return CurrentState; }

    void SetCombatState(ECombatState NewState) { CurrentState = NewState; }

    UPROPERTY(ReplicatedUsing = OnRep_CurrentState)
    ECombatState CurrentState;

    UPROPERTY(Replicated)
    FName CurrentActionID;

    UFUNCTION()
    void OnRep_CurrentState();

    UFUNCTION(BlueprintCallable, Category = "Combat|UI")
    void SetPendingAction(FName ActionID);

    UFUNCTION(BlueprintCallable, Category = "Combat|UI")
    void ResetPendingAction();

    UFUNCTION(BlueprintCallable, Category = "Combat|UI")
    void ConfirmPendingAction();

    UFUNCTION(BlueprintCallable, Category = "Combat|UI")
    FName GetPendingAction() const { return PendingActionID; }

    UPROPERTY(BlueprintAssignable, Category = "Combat|UI")
    FOnTargetChanged OnTargetChanged;

    void SelectTarget(AActor* TargetActor);
    // =======================================================================
    // [Action & RPC] 전투 행동 및 서버 통신
    // =======================================================================
    UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
    void AutoSelectFirstTarget();

    UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
    void CycleTarget(bool bNext);

    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void ExecuteAction(FName ActionID);

    UFUNCTION(Server, Reliable)
    void Server_ExecuteAction(ECombatState IntendedState, FName ActionID, AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Combat|State")
    void SaveOriginalPosition();

    void StartAttackSession();

    UFUNCTION(BlueprintCallable, Category = "Combat|Action")
    void ApplySkillEffect();

    UFUNCTION(Server, Reliable)
    void Server_ApplyDamage(AActor* TargetActor, float Damage);

    UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
    TArray<ABaseCharacter*> GetAliveEnemyTargets();

    UFUNCTION(BlueprintCallable, Category = "Combat|State")
    FVector GetOriginalLocation() const { return OriginalLocation; }

    UFUNCTION(BlueprintCallable, Category = "Combat|State")
    AActor* GetCurrentTarget() const { return CurrentTarget; }

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_StartTracking(AActor* TargetToTrack);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_StopTracking();


    // =======================================================================
    // [Reaction & Visual] 판정 및 연출
    // =======================================================================
    UFUNCTION(BlueprintCallable, Category = "Combat|Reaction")
    void HandleStateChanged(ECombatState NewState, FName ActionID = NAME_None);

    UFUNCTION(Server, Reliable)
    void Server_SetCombatState(ECombatState NewState);

    void SpawnSkillEffect(EEffectSpawnType EffectType);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SpawnSkillEffect(EEffectSpawnType EffectType, const TArray<AActor*>& TargetsToHit);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnReceivedHit(EParryResult Result);

    void PerformCounterAttack(ABaseCharacter* TargetMonster, float CounterDamage, bool bGroup);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayCounter(FName CounterSkillID, AActor* TargetMonster);

    UPROPERTY()
    TArray<class UNiagaraComponent*> FlyingProjectiles;

    UFUNCTION(BlueprintCallable, Category = "Combat|Animtion")
    void PlayMontageWithSync(UAnimMontage* MontageToPlay);

    void AddQTEResult(bool bSuccess) { QTEResults.Add(bSuccess); }
    const TArray<bool>& GetQTEResults() const { return QTEResults; }

    FSkillData* GetCurrentSkillData() const { return CurrentSkillData; }

    void StartTrackingActor(AActor* TargetToTrack);

    void StopTrackingActor();

    // 전투 종료/언로드 직전 반복 타이머를 강제 정리
    void StopBattleTimers();


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data")
    FName SingleCounterSkillID = TEXT("Player_Counter_Single");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data")
    FName GroupCounterSkillID = TEXT("Player_Counter_Group");

private:
    void ApplyDamageInternal(AActor* TargetActor, float SkillDamage, int32 DamageMultiplier = 1);

private:

    UPROPERTY(Replicated)
    FVector OriginalLocation;

    UPROPERTY(Replicated)
    FRotator OriginalRotation;

    UPROPERTY()
    FName PendingActionID = NAME_None;
private:
    FSkillData* CurrentSkillData;
    UPROPERTY()
    AActor* CurrentTarget;

    TArray<bool> QTEResults;

    FTimerHandle RotationTimerHandle;
    void InterpolateRotation();

    UPROPERTY()
    AActor* ActorToTrack;

    FTimerHandle TrackingTimerHandle;
    void TrackActorRoutine();
};
