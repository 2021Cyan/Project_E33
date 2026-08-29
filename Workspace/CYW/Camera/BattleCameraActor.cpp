#include "BattleCameraActor.h"
#include "Camera/CameraComponent.h"
#include "../../KHB/Character/MonsterCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"

ABattleCameraActor::ABattleCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Root);
}

void ABattleCameraActor::InitializeBasePosition()
{
    BaseLocation = GetActorLocation();
    BaseRotation = GetActorRotation();
    BaseFOV = Camera->FieldOfView;
    DefaultLocation = BaseLocation;
    DefaultRotation = BaseRotation;
    DefaultFOV = BaseFOV;
}

void ABattleCameraActor::BeginPlay()
{
    Super::BeginPlay();
    InitializeBasePosition();
    ResetSequenceStateToBase();
    ResetRuntimeEffects();
}

void ABattleCameraActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 기준/시퀀스 상태에 임시 효과 오프셋을 더해 최종 카메라 연출 구성
    if (!bPlayingSequence)
    {
        SequenceLocation = BaseLocation;
        SequenceRotation = BaseRotation;
    }

    if (!bPlayingFOVSequence) 
    {
        SequenceFOV = BaseFOV;
    }

    if (bPlayingSequence)
    {
        UpdateCameraSequence(DeltaTime);
    }

    if (bPlayingFOVSequence) 
    {
        UpdateFOVSequence(DeltaTime);
    }

    if (bEnemyTurnPullBackTracking)
    {
        UpdateEnemyTurnPullBack(DeltaTime);
    }

    UpdateAttackImpact(DeltaTime);

    if (bAllowHandheldShake)
    {
        UpdateHandheldShake(DeltaTime);
    }
    else
    { 
        ResetHandheldShakeOffset();
    }

    ApplyFinalCameraTransform();
}

void ABattleCameraActor::PlayCameraSequence(UBattleCameraSequenceData* Data)
{
    if (!Data)
    {
        return;
    }

    // 새 시퀀스는 현재 적용된 카메라 위치/회전을 기준으로 시작
    CurrentSequence = Data;
    BaseLocation = GetActorLocation();
    BaseRotation = GetActorRotation();
    BaseFOV = Camera->FieldOfView;

    ResetSequenceStateToBase();
    ResetSequencePlaybackState();

    bPlayingSequence = CurrentSequence->Cues.Num() > 0;
    bPlayingFOVSequence = CurrentSequence->FOVCues.Num() > 0;
    bAllowHandheldShake = CurrentSequence->bEnableHandheldShakeSequence;

    if (bPlayingSequence)
    {
        ApplyCue(0.0f);
    }

    if (bPlayingFOVSequence)
    {
        ApplyFOVCue(0.0f);
    }
}

void ABattleCameraActor::SetBaseCameraTransform(const FVector& NewLocation, const FRotator& NewRotation)
{
    // 외부에서 계산한 전투 카메라 위치를 기준 상태로 반영
    BaseLocation = NewLocation;
    BaseRotation = NewRotation;

    SequenceLocation = NewLocation;
    SequenceRotation = NewRotation;

    SetActorLocation(NewLocation);
    SetActorRotation(NewRotation);
}

void ABattleCameraActor::ReturnToDefaultPosition()
{
    // 전투 카메라에서 사용 중인 상태를 정리하고 시작 위치로 복귀
    StopEnemyTurnPullBackTracking();

    bPlayingSequence = false;
    bPlayingFOVSequence = false;
    CurrentSequence = nullptr;
    bAllowHandheldShake = true;

    BaseLocation = DefaultLocation;
    BaseRotation = DefaultRotation;
    BaseFOV = DefaultFOV;

    ResetSequenceStateToBase();
    ResetRuntimeEffects();
    ApplyFinalCameraTransform();
}

void ABattleCameraActor::StartEnemyTurnPullBackTracking(AMonsterCharacter* Monster, AActor* TargetActor, const FVector& LookTarget, bool bUseAreaSpineLookBlend)
{
    if (!Monster || !TargetActor)
    {
        StopEnemyTurnPullBackTracking();
        return;
    }

    EnemyTurnMonster = Monster;
    EnemyTurnTargetActor = TargetActor;
    EnemyTurnTrackingBaseLocation = BaseLocation;
    EnemyTurnTrackingLookTarget = LookTarget;
    bUseEnemyTurnAreaSpineLookBlend = bUseAreaSpineLookBlend;

    // 카메라가 위아래로 이동하지 않도록 수평 평면 기준으로만 뒤로 후퇴
    EnemyTurnPullBackDirection = BaseLocation - LookTarget;
    EnemyTurnPullBackDirection.Z = 0.0f;
    EnemyTurnPullBackDirection = EnemyTurnPullBackDirection.GetSafeNormal();

    if (EnemyTurnPullBackDirection.IsNearlyZero())
    {
        EnemyTurnPullBackDirection = -GetActorForwardVector();
        EnemyTurnPullBackDirection.Z = 0.0f;
        EnemyTurnPullBackDirection = EnemyTurnPullBackDirection.GetSafeNormal();
    }

    bEnemyTurnPullBackTracking = true;
}

void ABattleCameraActor::StopEnemyTurnPullBackTracking()
{
    // 적 턴 추적에 사용한 참조와 보정 값을 모두 해제
    bEnemyTurnPullBackTracking = false;
    EnemyTurnMonster = nullptr;
    EnemyTurnTargetActor = nullptr;
    EnemyTurnTrackingBaseLocation = FVector::ZeroVector;
    EnemyTurnTrackingLookTarget = FVector::ZeroVector;
    EnemyTurnPullBackDirection = FVector::ZeroVector;
    bUseEnemyTurnAreaSpineLookBlend = false;
}

void ABattleCameraActor::PlayAttackImpact()
{
    // 이미 재생 중이어도 처음부터 다시 재생
    AttackImpactElapsed = 0.0f;
    bPlayingAttackImpact = true;
}

void ABattleCameraActor::UpdateCameraSequence(float DeltaTime)
{
    if (!CurrentSequence || !CurrentSequence->Cues.IsValidIndex(CurrentCueIndex))
    {
        bPlayingSequence = false;
        return;
    }

    const FBattleCameraCue& Cue = CurrentSequence->Cues[CurrentCueIndex];

    CurrentCueElapsed += DeltaTime;

    const float Duration = FMath::Max(Cue.Duration, 0.01f);
    const float RawAlpha = FMath::Clamp(CurrentCueElapsed / Duration, 0.0f, 1.0f);
    const float Alpha = ApplyEase(Cue.EaseType, RawAlpha);

    // 현재 큐의 진행률에 맞춰 위치/회전 오프셋을 적용
    ApplyCue(Alpha);

    if (RawAlpha >= 1.0f)
    {
        CurrentCueIndex++;
        CurrentCueElapsed = 0.0f;

        CueStartLocation = SequenceLocation;
        CueStartRotation = SequenceRotation;
        if (!CurrentSequence->Cues.IsValidIndex(CurrentCueIndex))
        {
            bPlayingSequence = false;
        }
    }
}

void ABattleCameraActor::UpdateFOVSequence(float DeltaTime)
{
    if (!CurrentSequence || !CurrentSequence->FOVCues.IsValidIndex(CurrentFOVCueIndex))
    {
        bPlayingFOVSequence = false;
        return;
    }

    const FBattleCameraFOVCue& Cue = CurrentSequence->FOVCues[CurrentFOVCueIndex];

    CurrentFOVCueElapsed += DeltaTime;

    const float Duration = FMath::Max(Cue.Duration, 0.01f);
    const float RawAlpha = FMath::Clamp(CurrentFOVCueElapsed / Duration, 0.0f, 1.0f);
    const float Alpha = ApplyEase(Cue.EaseType, RawAlpha);

    // FOV는 위치/회전 큐와 별도로 진행
    ApplyFOVCue(Alpha);

    if (RawAlpha >= 1.0f)
    {
        CurrentFOVCueIndex++;
        CurrentFOVCueElapsed = 0.0f;

        CueStartFOV = SequenceFOV;

        if (!CurrentSequence->FOVCues.IsValidIndex(CurrentFOVCueIndex))
        {
            bPlayingFOVSequence = false;
        }
    }
}

void ABattleCameraActor::ApplyCue(float Alpha)
{
    if (!CurrentSequence || !CurrentSequence->Cues.IsValidIndex(CurrentCueIndex))
    {
        return;
    }

    const FBattleCameraCue& Cue = CurrentSequence->Cues[CurrentCueIndex];

    const FVector TargetLocation = BaseLocation + Cue.LocationOffset;
    const FRotator TargetRotation = BaseRotation + Cue.RotationOffset;

    if (Cue.bUseControlOffset)
    {
        // ControlOffset을 사용하는 큐는 베지어 형태로 이동
        const FVector ControlLocation = BaseLocation + Cue.ControlOffset;

        const FVector A = FMath::Lerp(CueStartLocation, ControlLocation, Alpha);
        const FVector B = FMath::Lerp(ControlLocation, TargetLocation, Alpha);

        SequenceLocation = FMath::Lerp(A, B, Alpha);
    }
    else
    {
        SequenceLocation = FMath::Lerp(CueStartLocation, TargetLocation, Alpha);
    }

    SequenceRotation = FMath::Lerp(CueStartRotation, TargetRotation, Alpha);
}

void ABattleCameraActor::ApplyFOVCue(float Alpha)
{
    if (!CurrentSequence || !CurrentSequence->FOVCues.IsValidIndex(CurrentFOVCueIndex))
    {
        return;
    }

    const FBattleCameraFOVCue& Cue = CurrentSequence->FOVCues[CurrentFOVCueIndex];

    SequenceFOV = FMath::Lerp(CueStartFOV, Cue.FOV, Alpha);
}

float ABattleCameraActor::ApplyEase(EBattleCameraEaseType EaseType, float Alpha) const
{
    switch (EaseType)
    {
    case EBattleCameraEaseType::Linear:
        return Alpha;

    case EBattleCameraEaseType::EaseIn:
        return FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 3.0f);

    case EBattleCameraEaseType::EaseOut:
        return FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);

    case EBattleCameraEaseType::EaseInOut:
        return FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 3.0f);

    default:
        return Alpha;
    }
}

void ABattleCameraActor::UpdateHandheldShake(float DeltaTime)
{
    if (!bUseHandheldShake)
    {
        ResetHandheldShakeOffset();
        return;
    }

    ShakeTime += DeltaTime * ShakeSpeed;

    // Perlin noise로 매 프레임 부드러운 미세 흔들림 생성
    const float X = FMath::PerlinNoise1D(ShakeTime) * LocationShakeAmount;
    const float Y = FMath::PerlinNoise1D(ShakeTime + 17.0f) * LocationShakeAmount;
    const float Z = FMath::PerlinNoise1D(ShakeTime + 31.0f) * LocationShakeAmount;

    const float Pitch = FMath::PerlinNoise1D(ShakeTime + 43.0f) * RotationShakeAmount;
    const float Yaw = FMath::PerlinNoise1D(ShakeTime + 59.0f) * RotationShakeAmount;
    const float Roll = FMath::PerlinNoise1D(ShakeTime + 71.0f) * RotationShakeAmount;

    ShakeLocationOffset = FVector(X, Y, Z);
    ShakeRotationOffset = FRotator(Pitch, Yaw, Roll);
}

void ABattleCameraActor::UpdateEnemyTurnPullBack(float DeltaTime)
{
    if (!EnemyTurnMonster || EnemyTurnMonster->IsDead() || !EnemyTurnTargetActor)
    {
        StopEnemyTurnPullBackTracking();
        return;
    }

    FVector EnemyFocusLocation = EnemyTurnMonster->GetActorLocation();
    if (const USkeletalMeshComponent* MonsterMesh = EnemyTurnMonster->GetMesh())
    {
        if (MonsterMesh->DoesSocketExist(EnemyCenterOfMass))
        {
            EnemyFocusLocation = MonsterMesh->GetSocketLocation(EnemyCenterOfMass);
        }
    }

    const FVector TargetLocation = EnemyTurnTargetActor->GetActorLocation();

    if (bDrawEnemyTurnPullBackDebug)
    {
        DrawDebugSphere(GetWorld(), EnemyFocusLocation, 18.0f, 12, FColor::Cyan, false, 0.0f, 0, 2.0f);
        DrawDebugSphere(GetWorld(), TargetLocation, 18.0f, 12, FColor::Green, false, 0.0f, 0, 2.0f);
        DrawDebugLine(GetWorld(), EnemyFocusLocation, TargetLocation, FColor::Yellow, false, 0.0f, 0, 2.0f);
    }

    const float Distance = FVector::Dist2D(EnemyFocusLocation, TargetLocation);
    const float PullBackAlpha = 1.0f - FMath::Clamp(Distance / FMath::Max(EnemyTurnPullBackStartDistance, 1.0f), 0.0f, 1.0f);
    const float PullBackDistance = PullBackAlpha * EnemyTurnPullBackMaxDistance;

    // 공격자와 대상이 가까워 추가 카메라 범위가 필요할 때만 뒤로 후퇴
    const FVector DesiredLocation = EnemyTurnTrackingBaseLocation + EnemyTurnPullBackDirection * PullBackDistance;
    const FVector NewLocation = FMath::VInterpTo(BaseLocation, DesiredLocation, DeltaTime, EnemyTurnPullBackInterpSpeed);
    const float SpineLookBlend =
        bUseEnemyTurnAreaSpineLookBlend ? EnemyTurnAreaSpineLookBlend : EnemyTurnSpineLookBlend;
    const FVector DesiredLookTarget = FMath::Lerp(
        EnemyTurnTrackingLookTarget,
        EnemyFocusLocation,
        FMath::Clamp(SpineLookBlend, 0.0f, 1.0f)
    );
    const FRotator DesiredRotation = (DesiredLookTarget - NewLocation).Rotation();
    const FRotator NewRotation = FMath::RInterpTo(BaseRotation, DesiredRotation, DeltaTime, EnemyTurnPullBackInterpSpeed);

    BaseLocation = NewLocation;
    BaseRotation = NewRotation;

    if (!bPlayingSequence)
    {
        SequenceLocation = BaseLocation;
        SequenceRotation = BaseRotation;
    }
}

void ABattleCameraActor::UpdateAttackImpact(float DeltaTime)
{
    if (!bPlayingAttackImpact)
    {
        ResetAttackImpactOffset();
        return;
    }

    AttackImpactElapsed += DeltaTime;

    const float Duration = FMath::Max(AttackImpactDuration, 0.01f);
    const float RawAlpha = FMath::Clamp(AttackImpactElapsed / Duration, 0.0f, 1.0f);
    const float Decay = FMath::InterpEaseOut(1.0f, 0.0f, RawAlpha, 2.0f);
    const float ShakeTimeValue = AttackImpactElapsed * AttackImpactShakeSpeed;

    // 짧게 강하게 흔들린 뒤 빠르게 감쇠
    AttackImpactLocationOffset = FVector(
        FMath::PerlinNoise1D(ShakeTimeValue) * AttackImpactLocationShakeAmount * Decay,
        FMath::PerlinNoise1D(ShakeTimeValue + 17.0f) * AttackImpactLocationShakeAmount * Decay,
        FMath::PerlinNoise1D(ShakeTimeValue + 31.0f) * AttackImpactLocationShakeAmount * Decay
    );

    AttackImpactRotationOffset = FRotator(
        FMath::PerlinNoise1D(ShakeTimeValue + 43.0f) * AttackImpactRotationShakeAmount * Decay,
        FMath::PerlinNoise1D(ShakeTimeValue + 59.0f) * AttackImpactRotationShakeAmount * Decay,
        FMath::PerlinNoise1D(ShakeTimeValue + 71.0f) * AttackImpactRotationShakeAmount * Decay
    );

    AttackImpactFOVOffset = -AttackImpactZoomAmount * Decay;

    if (RawAlpha >= 1.0f)
    {
        bPlayingAttackImpact = false;
        ResetAttackImpactOffset();
    }
}

void ABattleCameraActor::ApplyFinalCameraTransform()
{
    // 최종 적용 순서 : 시퀀스/기준 위치/회전 -> 흔들림 효과 -> 공격 임팩트
    SetActorLocation(SequenceLocation + ShakeLocationOffset + AttackImpactLocationOffset);
    SetActorRotation(SequenceRotation + ShakeRotationOffset + AttackImpactRotationOffset);

    if (Camera)
    {
        Camera->SetFieldOfView(FMath::Max(5.0f, SequenceFOV + AttackImpactFOVOffset));
    }
}

void ABattleCameraActor::ResetHandheldShakeOffset()
{
    ShakeLocationOffset = FVector::ZeroVector;
    ShakeRotationOffset = FRotator::ZeroRotator;
}

void ABattleCameraActor::ResetAttackImpactOffset()
{
    AttackImpactLocationOffset = FVector::ZeroVector;
    AttackImpactRotationOffset = FRotator::ZeroRotator;
    AttackImpactFOVOffset = 0.0f;
}

void ABattleCameraActor::ResetSequencePlaybackState()
{
    CurrentCueIndex = 0;
    CurrentCueElapsed = 0.0f;

    CurrentFOVCueIndex = 0;
    CurrentFOVCueElapsed = 0.0f;
}

void ABattleCameraActor::ResetSequenceStateToBase()
{
    SequenceLocation = BaseLocation;
    SequenceRotation = BaseRotation;
    SequenceFOV = BaseFOV;

    CueStartLocation = BaseLocation;
    CueStartRotation = BaseRotation;
    CueStartFOV = BaseFOV;
}

void ABattleCameraActor::ResetRuntimeEffects()
{
    ResetHandheldShakeOffset();
    ResetAttackImpactOffset();

    bPlayingAttackImpact = false;
    AttackImpactElapsed = 0.0f;
}

