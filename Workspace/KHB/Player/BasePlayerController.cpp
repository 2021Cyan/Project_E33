#include "BasePlayerController.h"
#include "../Core/LevelStreamingManager.h"
#include "../Core/BattleManager.h"
#include "../Character/BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "../../CYW/Camera/BattleCameraActor.h"
#include "../Character/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../UI/QTEWidget.h"
#include "../UI/InitiativeTrackerWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Camera/CameraComponent.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSequenceID.h"
#include "TimerManager.h"

 

void ABasePlayerController::StopPawnMovement(APawn* PlayerPawn)
{
    APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(PlayerPawn);
    if (!PlayerCharacter)
    {
        return;
    }

    if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    } 
}



void ABasePlayerController::BeginPlay()
{
    Super::BeginPlay();

    SwitchToExplorationInput();
}

void ABasePlayerController::AcknowledgePossession(APawn* InPawn)
{
    Super::AcknowledgePossession(InPawn);

    SwitchToExplorationInput();
}

void ABasePlayerController::SwitchInputMode(UInputMappingContext* NewIMC)
{
    if (!IsLocalController()) return;

    ApplyGameInputMode();

    if (!NewIMC)
    {
        UE_LOG(LogTemp, Error, TEXT("[InputFlow] New input mapping context is null. PC=%s"), *GetName());
        return;
    }

    if (CurrentIMC == NewIMC) return;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (CurrentIMC)
        {
            Subsystem->RemoveMappingContext(CurrentIMC);
        }

        Subsystem->AddMappingContext(NewIMC, 0);
        CurrentIMC = NewIMC;
    }
}

void ABasePlayerController::SwitchToExplorationInput()
{
    SwitchInputMode(IMC_Exploration);
}

void ABasePlayerController::SwitchToBattleInput()
{
    SwitchInputMode(IMC_Battle);
}

void ABasePlayerController::Client_ShowAttackQTE_Implementation(float Duration, float SuccessStartTime, float SuccessEndTime, FVector2D ScreenPosition)
{
    if (!IsLocalController())
    {
        return;
    }

    if (!QTEWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[QTE] QTEWidgetClass is null. PC=%s"), *GetName());
        return;
    }

    if (CurrentQTEWidget)
    {
        CurrentQTEWidget->RemoveFromParent();
        CurrentQTEWidget = nullptr;
    }

    CurrentQTEWidget = CreateWidget<UQTEWidget>(this, QTEWidgetClass);
    if (!CurrentQTEWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[QTE] Failed to create QTE widget. PC=%s"), *GetName());
        return;
    }

    if (BattleHUDWidget)
    {
        if (UCanvasPanel* QTEContainer = Cast<UCanvasPanel>(BattleHUDWidget->GetWidgetFromName(TEXT("QTE_Container"))))
        {
            UCanvasPanelSlot* Slot = QTEContainer->AddChildToCanvas(CurrentQTEWidget);
            if (Slot)
            {
                Slot->SetPosition(FVector2D(ScreenPosition.X * 1920.f, ScreenPosition.Y * 1080.f));
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
                Slot->SetAutoSize(true);
            }
        }
    }
    else
    {
        int32 ViewportWidth, ViewportHeight;
        GetViewportSize(ViewportWidth, ViewportHeight);
        FVector2D PixelPosition = FVector2D(ScreenPosition.X * ViewportWidth, ScreenPosition.Y * ViewportHeight);
        CurrentQTEWidget->AddToViewport();
        CurrentQTEWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
        CurrentQTEWidget->SetPositionInViewport(PixelPosition, false);
    }

    CurrentQTEWidget->StartQTE(Duration, SuccessStartTime, SuccessEndTime);

    UE_LOG(LogTemp, Warning, TEXT("[QTE] ShowAttackQTE: PC=%s Duration=%.2f Success=%.2f~%.2f"),
        *GetName(),
        Duration,
        SuccessStartTime,
        SuccessEndTime);
}

void ABasePlayerController::Client_ShowQTESpectator_Implementation(float Duration, FVector2D ScreenPosition)
{
    if (!IsLocalController()) return;

    if (!QTEWidgetClass) return;

    if (CurrentQTEWidget)
    {
        CurrentQTEWidget->RemoveFromParent();
        CurrentQTEWidget = nullptr;
    }

    CurrentQTEWidget = CreateWidget<UQTEWidget>(this, QTEWidgetClass);
    if (!CurrentQTEWidget) return;

    if (BattleHUDWidget)
    {
        if (UCanvasPanel* QTEContainer = Cast<UCanvasPanel>(BattleHUDWidget->GetWidgetFromName(TEXT("QTE_Container"))))
        {
            UCanvasPanelSlot* Slot = QTEContainer->AddChildToCanvas(CurrentQTEWidget);
            if (Slot)
            {
                Slot->SetPosition(FVector2D(ScreenPosition.X * 1920.f, ScreenPosition.Y * 1080.f));
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
                Slot->SetAutoSize(true);
            }
        }
    }
    else
    {
        int32 ViewportWidth, ViewportHeight;
        GetViewportSize(ViewportWidth, ViewportHeight);
        FVector2D PixelPosition = FVector2D(ScreenPosition.X * ViewportWidth, ScreenPosition.Y * ViewportHeight);
        CurrentQTEWidget->AddToViewport();
        CurrentQTEWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
        CurrentQTEWidget->SetPositionInViewport(PixelPosition, false);
    }

    CurrentQTEWidget->BP_PlayQTEAnimation();
}

void ABasePlayerController::Server_SubmitQTE_Implementation(bool bSuccess)
{
    UE_LOG(LogTemp, Warning, TEXT("[QTE] Server_SubmitQTE: PC=%s Success=%s"),
        *GetName(), bSuccess ? TEXT("true") : TEXT("false"));

    // 로직: 스킬 사용자 CombatComponent에 결과 저장
    if (APlayerCharacter* PlayerCharacter = GetPawn<APlayerCharacter>())
    {
        PlayerCharacter->AddQTEResult(bSuccess);
    }

    // UI: 전체 플레이어에게 결과 전달 + 서버 타이머 취소
    if (ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass())))
    {
        if (ABattleManager* BM = LSM->GetBattleManagerForActor(GetPawn()))
        {
            BM->CancelQTETimer();
            BM->NotifyQTEResultToAllPlayers(bSuccess);
        }
    }
}

void ABasePlayerController::Client_OnQTEResult_Implementation(bool bSuccess)
{
    if (!IsLocalController()) return;

    if (CurrentQTEWidget)
    {
        CurrentQTEWidget->BP_OnQTEFinished(bSuccess);
    }
}

void ABasePlayerController::Client_UpdateInitiativeTracker_Implementation(ABaseCharacter* CurrentTurn, const TArray<ABaseCharacter*>& TurnOrder)
{
    if (!BattleHUDWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InitTracker] BattleHUDWidget null"));
        return;
    }
    UInitiativeTrackerWidget* Tracker = Cast<UInitiativeTrackerWidget>(
        BattleHUDWidget->GetWidgetFromName(TEXT("InitiativeTrackerWidget")));
    if (!Tracker)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InitTracker] Tracker widget not found"));
        return;
    }
    Tracker->SetVisibility(ESlateVisibility::Visible);
    Tracker->UpdateTurnOrder(CurrentTurn, TurnOrder);
}

void ABasePlayerController::Client_HideInitiativeTracker_Implementation()
{
    if (!BattleHUDWidget) return;
    UInitiativeTrackerWidget* Tracker = Cast<UInitiativeTrackerWidget>(
        BattleHUDWidget->GetWidgetFromName(TEXT("InitiativeTrackerWidget")));
    if (Tracker)
        Tracker->SetVisibility(ESlateVisibility::Hidden);
}

void ABasePlayerController::SetBattleHUDWidget(UUserWidget* InBattleHUDWidget)
{
    BattleHUDWidget = InBattleHUDWidget;
}

void ABasePlayerController::Server_NotifyBattleStartMontageFinished_Implementation()
{
    APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetPawn());
    if (!PlayerCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BattleStartMontageFinished Blocked: PawnInvalid PC=%s"), *GetName());
        return;
    }

    ABattleManager* BM = PlayerCharacter->GetBattleManager();
    if (!BM)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BattleStartMontageFinished Blocked: BMNull PC=%s Player=%s"),
            *GetName(),
            *PlayerCharacter->GetName());
        return;
    }

    BM->NotifyBattleStartMontageFinished(this);
}

void ABasePlayerController::ApplyGameInputMode()
{
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
    bShowMouseCursor = false;
}

void ABasePlayerController::Client_LoadStreamLevel_Implementation(const FName& LevelName)
{
    PendingLevelName = LevelName;
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 05 ClientLoadStreamLevel: Level=%s PC=%s"), *LevelName.ToString(), *GetName());

    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.ExecutionFunction = FName("OnLevelLoaded");
    LatentInfo.UUID = 1;
    LatentInfo.Linkage = 0;

    UGameplayStatics::LoadStreamLevel(this, LevelName, true, false, LatentInfo);
}

void ABasePlayerController::Client_PrepareDungeonEnter_Implementation()
{
    DisableInput(this);
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    StopPawnMovement(GetPawn());

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 04 ClientPrepareDungeonEnter: PC=%s Pawn=%s"),
        *GetName(),
        GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
}

void ABasePlayerController::OnLevelLoaded()
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 08 ClientLevelLoaded: Level=%s PC=%s"), *PendingLevelName.ToString(), *GetName());
    Server_NotifyLevelLoaded(PendingLevelName);
}

void ABasePlayerController::OnLevelUnloaded()
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 ClientLevelUnloaded: Level=%s PC=%s"), *PendingUnloadLevelName.ToString(), *GetName());
    Server_NotifyLevelUnloaded(PendingUnloadLevelName);
}

void ABasePlayerController::Client_ApplySharedBattleCamera_Implementation(AActor* BattleCamera, float BlendTime)
{
    if (!IsLocalController())
    {
        return;
    }

    if (!BattleCamera)
    {
        return;
    }

    SetViewTargetWithBlend(BattleCamera, BlendTime);
    bIsUsingSharedCamera = true;
}

void ABasePlayerController::Client_PlaySharedBattleCamera_Implementation(AActor* BattleCamera, UBattleCameraSequenceData* SequenceData)
{
    if (!IsLocalController())
    {
        return;
    }

    if (!BattleCamera)
    {
        return;
    }

    if (!bIsUsingSharedCamera) {
        return;
    }

    if (ABattleCameraActor* BattleCam = Cast<ABattleCameraActor>(BattleCamera))
    {
        if (SequenceData)
        {
            BattleCam->PlayCameraSequence(SequenceData);
        }
    }
}

void ABasePlayerController::Client_MoveSharedBattleCameraEnemyTurn_Implementation(AActor* BattleCamera, AMonsterCharacter* Monster, AActor* TargetActor, FVector CameraLocation, FRotator CameraRotation, FVector LookTarget, bool bUseAreaSpineLookBlend)
{
    if (!IsLocalController())
    {
        return;
    }

    ABattleCameraActor* BattleCam = Cast<ABattleCameraActor>(BattleCamera);
    if (!BattleCam || !Monster || !TargetActor)
    {
        return;
    }

    BattleCam->SetBaseCameraTransform(CameraLocation, CameraRotation);
    BattleCam->StartEnemyTurnPullBackTracking(Monster, TargetActor, LookTarget, bUseAreaSpineLookBlend);
}

void ABasePlayerController::Client_ReturnSharedBattleCameraToBase_Implementation(AActor* BattleCamera)
{
    if (!IsLocalController())
    {
        return;
    }

    if (ABattleCameraActor* BattleCam = Cast<ABattleCameraActor>(BattleCamera))
    {
        BattleCam->ReturnToDefaultPosition();
    }
}

void ABasePlayerController::Client_PlaySharedBattleCameraImpact_Implementation(AActor* BattleCamera)
{
    if (!IsLocalController())
    {
        return;
    }

    if (ABattleCameraActor* BattleCam = Cast<ABattleCameraActor>(BattleCamera))
    {
        BattleCam->PlayAttackImpact();
    }
}

void ABasePlayerController::Client_PlayCounterLevelSequence_Implementation(ULevelSequence* LevelSequence, AActor* OriginActor, FVector OriginLocation, FRotator OriginRotation, bool bRestoreOriginAfterPlayback)
{
    auto LogCounterLS = [this](const FString& Message)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CounterLS][Client:%s] %s"), *GetNameSafe(this), *Message);
    };

    LogCounterLS(FString::Printf(
        TEXT("RPC received: IsLocal=%s Sequence=%s Path=%s OriginActor=%s OriginLocation=%s OriginRotation=%s RestoreOrigin=%s"),
        IsLocalController() ? TEXT("true") : TEXT("false"),
        *GetNameSafe(LevelSequence),
        LevelSequence ? *LevelSequence->GetPathName() : TEXT("None"),
        *GetNameSafe(OriginActor),
        *OriginLocation.ToString(),
        *OriginRotation.ToString(),
        bRestoreOriginAfterPlayback ? TEXT("true") : TEXT("false")
    ));

    if (!IsLocalController())
    {
        LogCounterLS(TEXT("Blocked: controller is not local"));
        return;
    }

    if (!LevelSequence)
    {
        LogCounterLS(TEXT("Blocked: LevelSequence is NULL"));
        return;
    }

    StopCounterLevelSequenceLocal();

    if (!OriginActor)
    {
        LogCounterLS(TEXT("Blocked: OriginActor is NULL"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        LogCounterLS(TEXT("Blocked: World is NULL"));
        return;
    }

    LogCounterLS(FString::Printf(TEXT("World valid: %s"), *GetNameSafe(World)));

    const FVector OriginalOriginLocation = OriginActor->GetActorLocation();
    const FRotator OriginalOriginRotation = OriginActor->GetActorRotation();

    if (bRestoreOriginAfterPlayback)
    {
        OriginActor->SetActorLocationAndRotation(OriginLocation, OriginRotation);
    }
    else
    {
        OriginLocation = OriginActor->GetActorLocation();
        OriginRotation = OriginActor->GetActorRotation();
    }

    LogCounterLS(FString::Printf(
        TEXT("OriginActor ready: Name=%s Class=%s Location=%s Rotation=%s OriginalLocation=%s OriginalRotation=%s"),
        *GetNameSafe(OriginActor),
        *GetNameSafe(OriginActor->GetClass()),
        *OriginActor->GetActorLocation().ToString(),
        *OriginActor->GetActorRotation().ToString(),
        *OriginalOriginLocation.ToString(),
        *OriginalOriginRotation.ToString()
    ));

    FActorSpawnParameters SequenceSpawnParams;
    SequenceSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SequenceSpawnParams.ObjectFlags |= RF_Transient;

    ALevelSequenceActor* SequenceActor = World->SpawnActor<ALevelSequenceActor>(
        ALevelSequenceActor::StaticClass(),
        OriginLocation,
        OriginRotation,
        SequenceSpawnParams
    );

    if (!SequenceActor)
    {
        LogCounterLS(TEXT("Blocked: failed to spawn LevelSequenceActor"));
        if (bRestoreOriginAfterPlayback)
        {
            OriginActor->SetActorLocationAndRotation(OriginalOriginLocation, OriginalOriginRotation);
        }
        return;
    }

    LogCounterLS(FString::Printf(
        TEXT("LevelSequenceActor spawned: Name=%s Location=%s Rotation=%s"),
        *GetNameSafe(SequenceActor),
        *SequenceActor->GetActorLocation().ToString(),
        *SequenceActor->GetActorRotation().ToString()
    ));

    FMovieSceneSequencePlaybackSettings PlaybackSettings;
    PlaybackSettings.bAutoPlay = false;

    SequenceActor->PlaybackSettings = PlaybackSettings;
    ULevelSequencePlayer* InitialSequencePlayer = SequenceActor->GetSequencePlayer();
    LogCounterLS(FString::Printf(
        TEXT("Initial SequencePlayer before SetSequence: %s"),
        InitialSequencePlayer ? TEXT("Valid") : TEXT("NULL")
    ));

    if (InitialSequencePlayer)
    {
        InitialSequencePlayer->SetPlaybackSettings(PlaybackSettings);
    }

    UDefaultLevelSequenceInstanceData* InstanceData = NewObject<UDefaultLevelSequenceInstanceData>(SequenceActor);
    if (InstanceData)
    {
        InstanceData->TransformOriginActor = OriginActor;
        SequenceActor->DefaultInstanceData = InstanceData;
        SequenceActor->bOverrideInstanceData = true;
        LogCounterLS(FString::Printf(
            TEXT("InstanceData set: InstanceData=%s TransformOriginActor=%s Override=%s"),
            *GetNameSafe(InstanceData),
            *GetNameSafe(InstanceData->TransformOriginActor.Get()),
            SequenceActor->bOverrideInstanceData ? TEXT("true") : TEXT("false")
        ));
    }
    else
    {
        LogCounterLS(TEXT("Warning: failed to create UDefaultLevelSequenceInstanceData"));
    }

    SequenceActor->SetActorLocationAndRotation(OriginLocation, OriginRotation);
    LogCounterLS(TEXT("Calling SequenceActor->SetSequence"));
    SequenceActor->SetSequence(LevelSequence);
    LogCounterLS(FString::Printf(
        TEXT("After SetSequence: SequenceActorSequence=%s"),
        *GetNameSafe(SequenceActor->GetSequence())
    ));

    if (!CounterLevelSequenceBindingName.IsNone())
    {
        UMovieScene* MovieScene = LevelSequence->GetMovieScene();
        if (MovieScene)
        {
            const FString TargetBindingName = CounterLevelSequenceBindingName.ToString();
            bool bBindingOverridden = false;

            for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
            {
                if (!Binding.GetName().Equals(TargetBindingName, ESearchCase::IgnoreCase))
                {
                    continue;
                }

                const FMovieSceneObjectBindingID BindingID(
                    UE::MovieScene::FFixedObjectBindingID(Binding.GetObjectGuid(), MovieSceneSequenceID::Root)
                );

                TArray<AActor*> BindingActors;
                BindingActors.Add(OriginActor);
                SequenceActor->SetBinding(BindingID, BindingActors, false);

                bBindingOverridden = true;
                LogCounterLS(FString::Printf(
                    TEXT("Binding override set: BindingName=%s Actor=%s"),
                    *TargetBindingName,
                    *GetNameSafe(OriginActor)
                ));
            }

            if (!bBindingOverridden)
            {
                LogCounterLS(FString::Printf(
                    TEXT("Binding override skipped: BindingName not found BindingName=%s"),
                    *TargetBindingName
                ));
            }
        }
        else
        {
            LogCounterLS(TEXT("Binding override skipped: MovieScene is NULL"));
        }
    }

    ULevelSequencePlayer* SequencePlayer = SequenceActor->GetSequencePlayer();
    if (!SequencePlayer)
    {
        LogCounterLS(TEXT("Blocked: SequencePlayer is NULL after SetSequence"));
        SequenceActor->Destroy();
        if (bRestoreOriginAfterPlayback)
        {
            OriginActor->SetActorLocationAndRotation(OriginalOriginLocation, OriginalOriginRotation);
        }
        return;
    }

    const FQualifiedFrameTime StartTime = SequencePlayer->GetCurrentTime();
    const FQualifiedFrameTime DurationTime = SequencePlayer->GetDuration();
    const float DurationSeconds = DurationTime.Rate.AsDecimal() > 0.0
        ? static_cast<float>(DurationTime.Time.AsDecimal() / DurationTime.Rate.AsDecimal())
        : 0.0f;

    UCameraComponent* ActiveCameraBefore = SequencePlayer->GetActiveCameraComponent();
    const FString ActiveCameraBeforeName = ActiveCameraBefore ? ActiveCameraBefore->GetName() : TEXT("None");

    LogCounterLS(FString::Printf(
        TEXT("SequencePlayer ready: Player=%s StartFrame=%.3f DurationFrame=%.3f Rate=%.3f DurationSeconds=%.3f IsPlayingBefore=%s ActiveCameraBefore=%s"),
        *GetNameSafe(SequencePlayer),
        StartTime.Time.AsDecimal(),
        DurationTime.Time.AsDecimal(),
        DurationTime.Rate.AsDecimal(),
        DurationSeconds,
        SequencePlayer->IsPlaying() ? TEXT("true") : TEXT("false"),
        *ActiveCameraBeforeName
    ));

    LogCounterLS(TEXT("Calling SequencePlayer->Play"));
    SequencePlayer->Play();
    ActiveCounterSequenceActor = SequenceActor;
    ActiveCounterSequencePlayer = SequencePlayer;
    ActiveCounterSequenceOriginActor = OriginActor;
    ActiveCounterSequenceOriginalOriginLocation = OriginalOriginLocation;
    ActiveCounterSequenceOriginalOriginRotation = OriginalOriginRotation;
    bActiveCounterSequenceRestoresOrigin = bRestoreOriginAfterPlayback;

    UCameraComponent* ActiveCameraAfter = SequencePlayer->GetActiveCameraComponent();
    const FString ActiveCameraAfterName = ActiveCameraAfter ? ActiveCameraAfter->GetName() : TEXT("None");

    LogCounterLS(FString::Printf(
        TEXT("After Play: IsPlaying=%s ActiveCamera=%s"),
        SequencePlayer->IsPlaying() ? TEXT("true") : TEXT("false"),
        *ActiveCameraAfterName
    ));

    const float CleanupDelay = FMath::Max(DurationSeconds + 1.0f, 2.0f);
    SequenceActor->SetLifeSpan(CleanupDelay);
    if (bRestoreOriginAfterPlayback)
    {
        const float RestoreDelay = FMath::Max(DurationSeconds, 0.0f);
        FTimerHandle RestoreOriginTimerHandle;
        TWeakObjectPtr<AActor> WeakOriginActor = OriginActor;
        World->GetTimerManager().SetTimer(
            RestoreOriginTimerHandle,
            [WeakOriginActor, OriginalOriginLocation, OriginalOriginRotation]()
            {
                if (AActor* PinnedOriginActor = WeakOriginActor.Get())
                {
                    PinnedOriginActor->SetActorLocationAndRotation(OriginalOriginLocation, OriginalOriginRotation);
                }
            },
            RestoreDelay,
            false
        );
    }

    LogCounterLS(FString::Printf(
        TEXT("Cleanup scheduled: CleanupDelay=%.3f RestoreOrigin=%s SequenceActorLifeSpan=%.3f"),
        CleanupDelay,
        bRestoreOriginAfterPlayback ? TEXT("true") : TEXT("false"),
        SequenceActor->GetLifeSpan()
    ));
}

void ABasePlayerController::Client_StopCounterLevelSequence_Implementation()
{
    if (!IsLocalController())
    {
        return;
    }

    StopCounterLevelSequenceLocal();
}

void ABasePlayerController::StopCounterLevelSequenceLocal()
{
    if (IsValid(ActiveCounterSequencePlayer))
    {
        ActiveCounterSequencePlayer->Stop();
    }

    if (bActiveCounterSequenceRestoresOrigin)
    {
        if (AActor* OriginActor = ActiveCounterSequenceOriginActor.Get())
        {
            OriginActor->SetActorLocationAndRotation(
                ActiveCounterSequenceOriginalOriginLocation,
                ActiveCounterSequenceOriginalOriginRotation
            );
        }
    }

    if (IsValid(ActiveCounterSequenceActor) && !ActiveCounterSequenceActor->IsActorBeingDestroyed())
    {
        ActiveCounterSequenceActor->Destroy();
    }

    ActiveCounterSequenceActor = nullptr;
    ActiveCounterSequencePlayer = nullptr;
    ActiveCounterSequenceOriginActor.Reset();
    ActiveCounterSequenceOriginalOriginLocation = FVector::ZeroVector;
    ActiveCounterSequenceOriginalOriginRotation = FRotator::ZeroRotator;
    bActiveCounterSequenceRestoresOrigin = false;
}

void ABasePlayerController::Client_ReturnToPersonalBattleCamera_Implementation(float BlendTime)
{
    if (!IsLocalController())
    {
        return;
    }

    StopCounterLevelSequenceLocal();

    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn))
    {
        return;
    }

    SetViewTargetWithBlend(ControlledPawn, BlendTime);
    bIsUsingSharedCamera = false;
}

void ABasePlayerController::Client_ReturnToExplorationCamera_Implementation(float BlendTime)
{
    if (!IsLocalController())
    {
        return;
    }

    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn))
    {
        return;
    }

    SetViewTargetWithBlend(ControlledPawn, BlendTime);
    bIsUsingSharedCamera = false;
}


void ABasePlayerController::Server_NotifyLevelLoaded_Implementation(const FName& LevelName)
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 08 ServerNotifyLevelLoaded: Level=%s PC=%s"), *LevelName.ToString(), *GetName());

    ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass()));
    if (LSM)
        LSM->NotifyPlayerLoaded(this, LevelName);
    else
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 08 ServerNotifyLevelLoaded Blocked: LSMNull Level=%s PC=%s"), *LevelName.ToString(), *GetName());
}

void ABasePlayerController::Server_NotifyLevelUnloaded_Implementation(const FName& LevelName)
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 ServerNotifyLevelUnloaded: Level=%s PC=%s"), *LevelName.ToString(), *GetName());

    ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass()));
    if (LSM)
        LSM->NotifyPlayerUnloaded(this, LevelName);
    else
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 ServerNotifyLevelUnloaded Blocked: LSMNull Level=%s PC=%s"), *LevelName.ToString(), *GetName());
}

void ABasePlayerController::Client_MoveToSpawnPoint_Implementation(FVector InSpawnLocation, FRotator InSpawnRotation)
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn) return;

    StopPawnMovement(ControlledPawn);
    ControlledPawn->SetActorLocationAndRotation(InSpawnLocation, InSpawnRotation,
        false, nullptr, ETeleportType::TeleportPhysics);
    StopPawnMovement(ControlledPawn);

    SetControlRotation(InSpawnRotation);

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 09 ClientMoveConfirmed: PC=%s Pawn=%s Location=%s Rotation=%s"),
        *GetName(),
        ControlledPawn ? *ControlledPawn->GetName() : TEXT("None"),
        *InSpawnLocation.ToString(),
        *InSpawnRotation.ToString());

    Server_NotifySpawnComplete(PendingLevelName);
}

void ABasePlayerController::Client_PrepareDungeonExit_Implementation()
{
    DisableInput(this);
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    StopPawnMovement(GetPawn());

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 30 ClientPrepareDungeonExit: PC=%s Pawn=%s"),
        *GetName(),
        GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
}

void ABasePlayerController::Client_ReturnToOverworld_Implementation(FVector InReturnLocation, FRotator InReturnRotation)
{
    APawn* ControlledPawn = GetPawn();
    if (ControlledPawn)
    {
        StopPawnMovement(ControlledPawn);
        ControlledPawn->SetActorLocationAndRotation(InReturnLocation, InReturnRotation,
            false, nullptr, ETeleportType::TeleportPhysics);
        StopPawnMovement(ControlledPawn);
    }
    SetControlRotation(InReturnRotation);
    ResetIgnoreMoveInput();   // IgnoreMoveInput = 0 으로 강제
    ResetIgnoreLookInput();   // IgnoreLookInput = 0 으로 강제
    EnableInput(this);
    SwitchToExplorationInput();
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 30 ClientReturnToOverworld: PC=%s Pawn=%s Location=%s Rotation=%s"),
        *GetName(),
        ControlledPawn ? *ControlledPawn->GetName() : TEXT("None"),
        *InReturnLocation.ToString(),
        *InReturnRotation.ToString());
}

void ABasePlayerController::Server_NotifySpawnComplete_Implementation(const FName& LevelName)
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 10 ServerNotifySpawnComplete: Level=%s PC=%s"), *LevelName.ToString(), *GetName());

    ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass()));
    if (LSM)
        LSM->NotifyPlayerSpawnComplete(this, LevelName);
    else
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 10 ServerNotifySpawnComplete Blocked: LSMNull Level=%s PC=%s"), *LevelName.ToString(), *GetName());
}

void ABasePlayerController::Client_UnLoadStreamLevel_Implementation(const FName& LevelName)
{
    PendingUnloadLevelName = LevelName;
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 ClientUnloadStreamLevel: Level=%s PC=%s"), *LevelName.ToString(), *GetName());

    // 입장의 OnLevelLoaded 핸드셰이크와 대칭: 언로드 완료를 콜백으로 받아 서버에 보고한다.
    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.ExecutionFunction = FName("OnLevelUnloaded");
    LatentInfo.UUID = 2;
    LatentInfo.Linkage = 0;

    UGameplayStatics::UnloadStreamLevel(this, LevelName, LatentInfo, false);
}
