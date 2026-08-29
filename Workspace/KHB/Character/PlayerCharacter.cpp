#include "PlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "../Core/BattleManager.h"
#include "MonsterCharacter.h"
#include "../Player/BasePlayerController.h"
#include "../../CWS/CombatComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "../UI/QTEWidget.h"

APlayerCharacter::APlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 400.f;
    SpringArm->bUsePawnControlRotation = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
}

void APlayerCharacter::BeginPlay() 
{
    Super::BeginPlay();

    if (Camera) {
        DefaultCameraRelativeLocation = Camera->GetRelativeLocation();
        DefaultCameraRelativeRotation = Camera->GetRelativeRotation();
    }
}

void APlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateHandheldShake(DeltaTime);
    ApplyCameraShake();
}

void APlayerCharacter::OnRep_ReplicatedBattleManager()
{
    Super::OnRep_ReplicatedBattleManager();
    TryBroadcastEnterBattle();
}


void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
        EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
        EIC->BindAction(IA_QTE, ETriggerEvent::Started, this, &APlayerCharacter::OnQTEInput);
    }
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
    if (!Controller) return;

    FVector2D Input = Value.Get<FVector2D>();

    const FRotator Rotation = Controller->GetControlRotation();
    const FRotator YawRotation(0, Rotation.Yaw, 0);

    const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(ForwardDir, Input.Y);
    AddMovementInput(RightDir, Input.X);
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
    FVector2D Input = Value.Get<FVector2D>();
    AddControllerYawInput(Input.X);
    AddControllerPitchInput(Input.Y);
}

void APlayerCharacter::OnQTEInput(const FInputActionValue& Value)
{
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("QTE Input Received")));

    if (!IsMyTurn()) return;

    ABasePlayerController* PC = Cast<ABasePlayerController>(GetController());
    if (!PC) return;

    // 클라에서는 복제 BM이 없을 수 있어 BM 조회에 의존하지 않는다.
    // QTE 제출은 위젯 → 서버 RPC로 처리되므로 위젯만 있으면 충분.
    if(!PC->GetCurrentQTEWidget()) return;

    PC->GetCurrentQTEWidget()->SubmitQTE();
}

void APlayerCharacter::OnBattleEntryMontageFinished()
{
    if (ABasePlayerController* PC = Cast<ABasePlayerController>(GetController()))
    {
        PC->Server_NotifyBattleStartMontageFinished();
    }
}

void APlayerCharacter::UpdateBattleInputState()
{
    if (!IsLocallyControlled()) return;
    ABasePlayerController* PC = Cast<ABasePlayerController>(GetController());
    if (!PC) return;

    PC->EnableInput(PC);
    PC->SwitchToBattleInput();

    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);
}

void APlayerCharacter::AddQTEResult(bool bSuccess)
{
    if (UCombatComponent* Combat = FindComponentByClass<UCombatComponent>())
    {
        Combat->AddQTEResult(bSuccess);
    }
}

bool APlayerCharacter::ShowBossHealthBarForCurrentBattle()
{
    if (!IsLocallyControlled())
    {
        return false;
    }

    ABattleManager* BattleManager = GetBattleManager();
    if (BattleManager)
    {
        for (AMonsterCharacter* Monster : BattleManager->GetSpawnedMonsters())
        {
            if (Monster && Monster->IsBossEncounter())
            {
                // 보스가 클라에 복제 도착 — 재시도 중단 후 표시
                GetWorldTimerManager().ClearTimer(BossHealthBarRetryTimer);
                BossHealthBarRetryCount = 0;
                ReceiveShowBossHealthBar(Monster);
                return true;
            }
        }
    }

    // 전투 진입 직후엔 보스 몬스터(또는 BM의 SpawnedMonsters 참조)가 아직 클라에
    // 복제되지 않아 못 찾을 수 있다. 복제 도착을 기다리며 제한된 횟수만큼 재시도한다.
    if (BossHealthBarRetryCount < MaxBossHealthBarRetries)
    {
        ++BossHealthBarRetryCount;
        GetWorldTimerManager().SetTimer(
            BossHealthBarRetryTimer, this,
            &APlayerCharacter::RetryShowBossHealthBar, 0.2f, false);
    }
    else
    {
        // 보스 없는 전투이거나 복제 실패 — 폴링 종료
        BossHealthBarRetryCount = 0;
    }

    return false;
}

void APlayerCharacter::RetryShowBossHealthBar()
{
    ShowBossHealthBarForCurrentBattle();
}

void APlayerCharacter::TryBroadcastEnterBattle()
{
    ABattleManager* BattleManager = GetBattleManager();
    if (!bPendingEnterBattleBroadcast || bEnterBattleBroadcasted || !BattleManager || !IsLocallyControlled())
    {
        return;
    }

    bEnterBattleBroadcasted = true;
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] EnterBattleBroadcast: Player=%s BM=%s"),
        *GetName(),
        *BattleManager->GetName());

    OnPlayerEnterBattle.Broadcast();
}

void APlayerCharacter::Client_StartPlayerTurn_Implementation()
{
    bClientTurnActive = true;        // 클라 턴 판정용 플래그 (서버 결정 반영)
    UpdateBattleInputState();
    OnPlayerTurnStart.Broadcast();
}

void APlayerCharacter::Client_StartDefenseTurn_Implementation()
{
    bClientTurnActive = false;
    UpdateBattleInputState();
}

void APlayerCharacter::Client_EndPlayerTurn_Implementation()
{
    bClientTurnActive = false;       // 턴 종료 — 클라 입력 게이트 닫기
    UpdateBattleInputState();
    OnPlayerTurnEnd.Broadcast();
}

void APlayerCharacter::Client_EnterBattleMode_Implementation()
{
    bClientTurnActive = false;
    bPendingEnterBattleBroadcast = true;
    bEnterBattleBroadcasted = false;
    BossHealthBarRetryCount = 0;       // 새 전투 진입 — 보스바 재시도 카운트 초기화
    if (ABasePlayerController* PC = Cast<ABasePlayerController>(GetController()))
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        PC->DisableInput(PC);
    }
    ReceiveEnterBattleMode();
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] ClientEnterBattleMode: Player=%s"), *GetName());
    SetHandheldShakeEnabled(true);
    TryBroadcastEnterBattle();
}

void APlayerCharacter::Client_ExitBattleMode_Implementation()
{
    bClientTurnActive = false;       // 전투 종료 시 안전 초기화
    bPendingEnterBattleBroadcast = false;
    bEnterBattleBroadcasted = false;
    GetWorldTimerManager().ClearTimer(BossHealthBarRetryTimer);  // 보스바 재시도 중단
    BossHealthBarRetryCount = 0;
    if (ABasePlayerController* PC = Cast<ABasePlayerController>(GetController()))
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        PC->DisableInput(PC);
        ReceiveExitBattleMode();
        SetHandheldShakeEnabled(true);

		// 배틀 모드 종료 시 모든 플레이어의 상태를 Idle로 강제 전환 (예외 상황 방지)
        if (UCombatComponent* Combat = FindComponentByClass<UCombatComponent>())
        {
            Combat->SetCombatState(ECombatState::Idle);
            Combat->HandleStateChanged(ECombatState::Idle);
        }

    }
    UE_LOG(LogTemp, Warning, TEXT("[Input] 기본(탐험) IMC로 전환 완료!"));

    OnPlayerExitBattle.Broadcast();
}

void APlayerCharacter::UpdateHandheldShake(float DeltaTime)
{
    if (!bUseHandHeldShake)
    {
        ShakeLocationOffset = FVector::ZeroVector;
        ShakeRotationOffset = FRotator::ZeroRotator;
        return;
    }

    ShakeTime += DeltaTime * ShakeSpeed;

    const float X = FMath::PerlinNoise1D(ShakeTime) * LocationShakeAmount;
    const float Y = FMath::PerlinNoise1D(ShakeTime + 17.0f) * LocationShakeAmount;
    const float Z = FMath::PerlinNoise1D(ShakeTime + 31.0f) * LocationShakeAmount;

    const float Pitch = FMath::PerlinNoise1D(ShakeTime + 43.0f) * RotationShakeAmount;
    const float Yaw = FMath::PerlinNoise1D(ShakeTime + 59.0f) * RotationShakeAmount;
    const float Roll = FMath::PerlinNoise1D(ShakeTime + 71.0f) * RotationShakeAmount;

    ShakeLocationOffset = FVector(X, Y, Z);
    ShakeRotationOffset = FRotator(Pitch, Yaw, Roll);
}

void APlayerCharacter::ApplyCameraShake()
{
    if (!Camera)
    {
        return;
    }

    Camera->SetRelativeLocation(DefaultCameraRelativeLocation + ShakeLocationOffset);
    Camera->SetRelativeRotation(DefaultCameraRelativeRotation + ShakeRotationOffset);
}

void APlayerCharacter::ResetCameraShake()
{
    ShakeLocationOffset = FVector::ZeroVector;
    ShakeRotationOffset = FRotator::ZeroRotator;

    if (Camera)
    {
        Camera->SetRelativeLocation(DefaultCameraRelativeLocation);
        Camera->SetRelativeRotation(DefaultCameraRelativeRotation);
    }
}

void APlayerCharacter::SetHandheldShakeEnabled(bool bEnabled)
{
    bUseHandHeldShake = bEnabled;

    if (!bUseHandHeldShake)
    {
        ResetCameraShake();
    }
}
