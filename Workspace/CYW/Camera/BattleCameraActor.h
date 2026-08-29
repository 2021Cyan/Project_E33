#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleCameraSequenceData.h"
#include "BattleCameraActor.generated.h"

class UCameraComponent;
class USceneComponent;
class AMonsterCharacter;

UCLASS()
class TEAMPROJECT_API ABattleCameraActor : public AActor
{
    GENERATED_BODY()

public:
    ABattleCameraActor();
    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    // 현재 전투 상황에 맞는 기준 카메라 위치/회전을 설정
    void SetBaseCameraTransform(const FVector& NewLocation, const FRotator& NewRotation);

    // 저장해둔 기본 카메라 위치로 복귀
    void ReturnToDefaultPosition();

    // 적 턴 동안 공격자와 대상을 함께 담도록 카메라를 보정
    void StartEnemyTurnPullBackTracking(
        AMonsterCharacter* Monster, 
        AActor* TargetActor, 
        const FVector& LookTarget, 
        bool bUseAreaSpineLookBlend = false
    );
    void StopEnemyTurnPullBackTracking();

    // 기본 위치를 재사용해야 할 때(광역 공격시) 위치 반환
    FVector GetDefaultCameraLocation() const { return DefaultLocation; }

    // 공격 적중 순간에 짧은 흔들림/줌 효과 재생
    void PlayAttackImpact();

    // 카메라 시퀀스 데이터 에셋 재생
    void PlayCameraSequence(UBattleCameraSequenceData* Data);

private:
    // BeginPlay 시점의 기본 카메라 상태를 저장
    void InitializeBasePosition();

    // 시퀀스는 위치/회전 트랙과 FOV 트랙을 별도로 갱신
    void UpdateCameraSequence(float DeltaTime);
    void UpdateFOVSequence(float DeltaTime);

    // 현재 큐의 보간 결과를 시퀀스에 반영
    void ApplyCue(float Alpha);
    void ApplyFOVCue(float Alpha);
    float ApplyEase(EBattleCameraEaseType EaseType, float Alpha) const;

    // 추가 카메라 효과 갱신
    void UpdateHandheldShake(float DeltaTime);
    void UpdateEnemyTurnPullBack(float DeltaTime);
    void UpdateAttackImpact(float DeltaTime);

    // 복귀와 재생 준비 과정에서 사용되는 통일된 초기화 과정
    void ResetHandheldShakeOffset();
    void ResetAttackImpactOffset();
    void ResetSequencePlaybackState();
    void ResetSequenceStateToBase();
    void ResetRuntimeEffects();

    // 모든 카메라 레이어를 합산해 실제 Actor/CameraComponent에 적용
    void ApplyFinalCameraTransform();

private:
    // 컴포넌트
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> Camera;

    // 기본 카메라 상태
    FVector BaseLocation;
    FRotator BaseRotation;
    float BaseFOV = 90.0f;

    FVector DefaultLocation;
    FRotator DefaultRotation;
    float DefaultFOV = 90.0f;

    // 현재 시퀀스 재생 상태
    FVector SequenceLocation;
    FRotator SequenceRotation;
    float SequenceFOV = 90.0f;

    FVector CueStartLocation;
    FRotator CueStartRotation;
    float CueStartFOV = 90.0f;

    UPROPERTY()
    TObjectPtr<UBattleCameraSequenceData> CurrentSequence;

    int32 CurrentCueIndex = 0;
    float CurrentCueElapsed = 0.0f;

    int32 CurrentFOVCueIndex = 0;
    float CurrentFOVCueElapsed = 0.0f;

    bool bPlayingSequence = false;
    bool bPlayingFOVSequence = false;
    bool bAllowHandheldShake = true;

    // 시퀀스 결과 위에 더해지는 런타임 효과 오프셋
    FVector ShakeLocationOffset = FVector::ZeroVector;
    FRotator ShakeRotationOffset = FRotator::ZeroRotator;
    float ShakeTime = 0.0f;

    FVector AttackImpactLocationOffset = FVector::ZeroVector;
    FRotator AttackImpactRotationOffset = FRotator::ZeroRotator;
    float AttackImpactFOVOffset = 0.0f;

    // 자연스러운 흔들림 설정
    UPROPERTY(EditAnywhere, Category = "Battle Camera Shake")
    bool bUseHandheldShake = true;

    UPROPERTY(EditAnywhere, Category = "Battle Camera Shake")
    float ShakeSpeed = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera Shake")
    float LocationShakeAmount = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera Shake")
    float RotationShakeAmount = 1.0f;

    // 적 턴 추적 설정
    UPROPERTY(EditAnywhere, Category = "Battle Camera|Enemy Turn")
    float EnemyTurnPullBackStartDistance = 450.0f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Enemy Turn")
    float EnemyTurnPullBackMaxDistance = 260.0f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Enemy Turn")
    float EnemyTurnPullBackInterpSpeed = 4.0f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Enemy Turn")
    FName EnemyCenterOfMass = TEXT("spine_01");

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Enemy Turn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EnemyTurnSpineLookBlend = 0.70f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Enemy Turn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EnemyTurnAreaSpineLookBlend = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Enemy Turn|Debug")
    bool bDrawEnemyTurnPullBackDebug = false;

    // 적 공격 적중 시 짧게 재생되는 임팩트(카메라 흔들림) 효과
    UPROPERTY(EditAnywhere, Category = "Battle Camera|Attack Impact")
    float AttackImpactDuration = 0.25f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Attack Impact")
    float AttackImpactZoomAmount = 8.0f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Attack Impact")
    float AttackImpactLocationShakeAmount = 14.0f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Attack Impact")
    float AttackImpactRotationShakeAmount = 2.5f;

    UPROPERTY(EditAnywhere, Category = "Battle Camera|Attack Impact")
    float AttackImpactShakeSpeed = 42.0f;

    float AttackImpactElapsed = 0.0f;
    bool bPlayingAttackImpact = false;

    // 적 턴 추적 상태
    UPROPERTY()
    TObjectPtr<AMonsterCharacter> EnemyTurnMonster;

    UPROPERTY()
    TObjectPtr<AActor> EnemyTurnTargetActor;

    FVector EnemyTurnTrackingBaseLocation = FVector::ZeroVector;
    FVector EnemyTurnTrackingLookTarget = FVector::ZeroVector;
    FVector EnemyTurnPullBackDirection = FVector::ZeroVector;
    bool bUseEnemyTurnAreaSpineLookBlend = false;
    bool bEnemyTurnPullBackTracking = false;
};
