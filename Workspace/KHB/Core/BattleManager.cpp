#include "BattleManager.h"
#include "Net/UnrealNetwork.h"
#include "TurnManager.h"
#include "LevelStreamingManager.h"
#include "DungeonSpawnPoint.h"
#include "../Character/PlayerCharacter.h"
#include "../Character/MonsterCharacter.h"
#include "../Player/BasePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "../../CWS/CombatTypes.h"
#include "../../CWS/CombatComponent.h"
#include "../../CYW/Camera/BattleCinematicManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"

namespace
{
    void StopPawnMovement(APawn* Pawn)
    {
        ACharacter* Character = Cast<ACharacter>(Pawn);
        if (!Character)
        {
            return;
        }

        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
    }
}

ABattleManager::ABattleManager()
{
    bReplicates = true;
    // 매니저 액터는 모든 참여자에게 항상 복제되어야 한다.
    // (레벨 배치 액터라 기본 거리 컬링이 걸리면, 멀리 있는 원격 참여자에게는
    //  BM 채널이 안 열려 SpawnedMonsters/RegisteredPlayerControllers 등 복제 배열이
    //  초기값(빈 배열=0) 그대로 남는다. 포인터는 이름으로 resolve돼도 데이터가 0이 됨.)
    bAlwaysRelevant = true;
    BattleState = EBattleState::Idle;
}

void ABattleManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABattleManager, BattleState);
    DOREPLIFETIME(ABattleManager, RegisteredPlayerControllers);
    DOREPLIFETIME(ABattleManager, CurrentTurnCharacter);
    DOREPLIFETIME(ABattleManager, SpawnedMonsters);
    DOREPLIFETIME(ABattleManager, BattlePlayerCharacters);
    DOREPLIFETIME(ABattleManager, bBattleStartMontagesFinished);
    DOREPLIFETIME(ABattleManager, bBattleStartCinematicFinished);
    DOREPLIFETIME(ABattleManager, bFirstTurnStarted);
}

void ABattleManager::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 06 BM BeginPlay: BM=%s Level=%s"), *GetName(), *DungeonLevelName.ToString());

    // 스폰포인트 캐싱 및 인덱스 순서 정렬
    CacheSpawnPoints();


    // LSM이 들고 있던 던전 입장 대상 플레이어 컨트롤러 목록을 BattleManager가 받아서 서버 기준 참가자 원본으로 등록
    // 서버에서 전투 로직 및 클라이언트 RPC 호출 목적으로 서버 측에 저장 및 관리

    ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass()));
    if (LSM)
    {
        TArray<ABasePlayerController*> Players = LSM->GetDungeonPlayers(DungeonLevelName);
        for (ABasePlayerController* PC : Players)
            RegisterPlayer(PC);

        SetMonsterSpawnSettings(LSM->GetBattleMonsterSpawnSettings(DungeonLevelName));
        SetEncounterSourceMonster(LSM->GetPendingEncounterMonster(DungeonLevelName));
        LSM->RegisterBattleManager(DungeonLevelName, this);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 06 BM BeginPlay Blocked: LSMNull Level=%s"), *DungeonLevelName.ToString());
    }
}

void ABattleManager::StartBattle()
{
    if (!HasAuthority()) return;
    if (BattleState != EBattleState::Idle)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 12 StartBattle Blocked: StateNotIdle"));
        return;
    }

    // 등록된 PC 목록을 기반으로 실제 전투에 사용할 PlayerCharacter Pawn 목록을 확정
    // 클라이언트에서 타켓팅, UI 등 로컬 환경에서의 진행을 위해 필요
    // 클라이언트는 본인 이외 PC를 인지 불가 그래서 PlayerController가 아니라 APlayerCharacter 배열이 필요함
    RefreshBattlePlayerCharactersFromControllers();

    SpawnBattleMonsters();
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 12 StartBattle: Players=%d Monsters=%d"), RegisteredPlayerControllers.Num(), SpawnedMonsters.Num());


    // 플레이어 + 몬스터 Pawn를 하나로 묶는 Array 생성하여 턴매니저에 저장
    // 턴계산을 위해 저장
    // Array 사용 이유: 순서 보장, 정렬 쉬움, 인덱스 기반 순서 조회 쉬움
    TArray<ABaseCharacter*> Participants = BuildParticipants();

    InitializeTurnManager(Participants);
    ResetBattleStartReadiness();

    BattleState = EBattleState::InBattle;

    InitializeParticipantsForBattle(Participants);
    StartBattleCinematicOrFirstTurn();
}

void ABattleManager::InitializeTurnManager(const TArray<ABaseCharacter*>& Participants)
{
    TurnManager = NewObject<UTurnManager>(this);
    TurnManager->InitTurn(Participants);
}

void ABattleManager::InitializeParticipantsForBattle(const TArray<ABaseCharacter*>& Participants)
{
    AssignBattleManagerToParticipants(Participants);
    NotifyPlayersEnterBattle();

    for (AMonsterCharacter* Monster : SpawnedMonsters)
    {
        if (Monster)
        {
            Monster->SetInBattle(true);
        }
    }
}

void ABattleManager::StartBattleCinematicOrFirstTurn()
{
    if (BattleCinematicManager)
    {
        BattleCinematicManager->OnBattleStarted(
            RegisteredPlayerControllers,
            FOnBattleCinematicFinished::CreateUObject(this, &ABattleManager::NotifyBattleStartCinematicFinished)
        );
        return;
    }

    NotifyBattleStartCinematicFinished();
}

void ABattleManager::ResetBattleStartReadiness()
{
    bBattleStartMontagesFinished = false;
    bBattleStartCinematicFinished = false;
    bFirstTurnStarted = false;
    BattleStartMontageFinishedPlayers.Empty();
}

void ABattleManager::NotifyBattleStartCinematicFinished()
{
    if (!HasAuthority())
    {
        return;
    }

    bBattleStartCinematicFinished = true;
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BattleStartCinematicFinished: Level=%s"), *DungeonLevelName.ToString());
    TryStartFirstTurn();
}

void ABattleManager::NotifyBattleStartMontageFinished(ABasePlayerController* PlayerController)
{
    if (!HasAuthority())
    {
        return;
    }

    MarkBattleStartMontageFinished(PlayerController);
}

void ABattleManager::MarkBattleStartMontageFinished(ABasePlayerController* PlayerController)
{
    if (!HasAuthority() || !PlayerController)
    {
        return;
    }

    BattleStartMontageFinishedPlayers.Add(PlayerController);

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BattleStartMontageFinished: PC=%s Ready=%d/%d"),
        *PlayerController->GetName(),
        BattleStartMontageFinishedPlayers.Num(),
        RegisteredPlayerControllers.Num());

    bool bAllPlayersFinished = true;
    for (ABasePlayerController* RegisteredPC : RegisteredPlayerControllers)
    {
        if (!RegisteredPC)
        {
            continue;
        }

        if (!BattleStartMontageFinishedPlayers.Contains(RegisteredPC))
        {
            bAllPlayersFinished = false;
            break;
        }
    }

    if (bAllPlayersFinished)
    {
        bBattleStartMontagesFinished = true;
        TryStartFirstTurn();
    }
}

void ABattleManager::TryStartFirstTurn()
{
    if (!HasAuthority() || bFirstTurnStarted)
    {
        return;
    }

    if (!bBattleStartMontagesFinished || !bBattleStartCinematicFinished)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] TryStartFirstTurn Waiting: Montage=%s Cinematic=%s"),
            bBattleStartMontagesFinished ? TEXT("true") : TEXT("false"),
            bBattleStartCinematicFinished ? TEXT("true") : TEXT("false"));
        return;
    }

    bFirstTurnStarted = true;
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] TryStartFirstTurn: StartTurn"));
    StartTurn();
}

void ABattleManager::SendSpawnPoint(ABasePlayerController* PC)
{
    if (!HasAuthority() || !PC) return;

    int32 Index = RegisteredPlayerControllers.IndexOfByKey(PC);
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 09 SendSpawnPoint: PC=%s Index=%d SpawnPoints=%d"), *PC->GetName(), Index, SpawnPoints.Num());

    if (!SpawnPoints.IsValidIndex(Index))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 09 SendSpawnPoint Blocked: InvalidIndex PC=%s Index=%d SpawnPoints=%d"), *PC->GetName(), Index, SpawnPoints.Num());
        return;
    }

    APawn* Pawn = PC->GetPawn();
    if (!Pawn) return;

    FVector TargetLocation = SpawnPoints[Index]->GetActorLocation();
    float HalfHeight = Pawn->GetSimpleCollisionHalfHeight();
    TargetLocation.Z += HalfHeight;
    FRotator TargetRotation = SpawnPoints[Index]->GetActorRotation();

    StopPawnMovement(Pawn);
    Pawn->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
    StopPawnMovement(Pawn);

    PC->SetControlRotation(TargetRotation);
    PC->Client_MoveToSpawnPoint(TargetLocation, TargetRotation);
}

void ABattleManager::CacheSpawnPoints()
{
    TArray<AActor*> SpawnActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADungeonSpawnPoint::StaticClass(), SpawnActors);

    SpawnPoints.Empty();
    EnemySpawnPoints.Empty();

    for (AActor* Actor : SpawnActors)
    {
        ADungeonSpawnPoint* SpawnPoint = Cast<ADungeonSpawnPoint>(Actor);
        if (!SpawnPoint)
        {
            continue;
        }

        if (SpawnPoint->LevelName != DungeonLevelName)
        {
            continue;
        }

        if (SpawnPoint->Team == EBattleSpawnTeam::Player)
        {
            SpawnPoints.Add(SpawnPoint);
        }
        else if (SpawnPoint->Team == EBattleSpawnTeam::Enemy)
        {
            EnemySpawnPoints.Add(SpawnPoint);
        }
    }

    SpawnPoints.Sort([](const ADungeonSpawnPoint& A, const ADungeonSpawnPoint& B)
        {
            return A.Index < B.Index;
        });

    EnemySpawnPoints.Sort([](const ADungeonSpawnPoint& A, const ADungeonSpawnPoint& B)
        {
            return A.Index < B.Index;
        });

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] CacheSpawnPoints: Player=%d Enemy=%d"),
        SpawnPoints.Num(),
        EnemySpawnPoints.Num());
}

void ABattleManager::SetMonsterSpawnSettings(const FBattleMonsterSpawnSettings& InMonsterSpawnSettings)
{
    MonsterSpawnSettings = InMonsterSpawnSettings;
}

void ABattleManager::SetEncounterSourceMonster(AMonsterCharacter* InEncounterSourceMonster)
{
    EncounterSourceMonster = InEncounterSourceMonster;
}

void ABattleManager::EndBattle(bool bPlayersWon)
{
    if (!HasAuthority()) return;
    if (BattleState == EBattleState::End) return;
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 29 EndBattle: Level=%s PlayersWon=%s EncounterMonster=%s"),
        *DungeonLevelName.ToString(),
        bPlayersWon ? TEXT("true") : TEXT("false"),
        EncounterSourceMonster ? *EncounterSourceMonster->GetName() : TEXT("None"));

    BattleState = EBattleState::End;

    if (BattleCinematicManager)
    {
        BattleCinematicManager->OnBattleEnded(RegisteredPlayerControllers);
    }

    for (APlayerCharacter* PlayerChar : GetBattlePlayerCharacters())
    {
        if (PlayerChar)
        {
            PlayerChar->Client_ExitBattleMode();
            PlayerChar->SetInBattle(false);

			// 배틀 종료 시 모든 플레이어의 상태를 Idle로 강제 전환 (예외 상황 방지)
            if (UCombatComponent* Combat = PlayerChar->FindComponentByClass<UCombatComponent>())
            {
                Combat->SetCombatState(ECombatState::Idle);
                Combat->HandleStateChanged(ECombatState::Idle);
            }
        }
    }

    for (AMonsterCharacter* Monster : SpawnedMonsters)
        if (Monster) Monster->SetInBattle(false);

    for (ABasePlayerController* PC : RegisteredPlayerControllers)
        if (PC) PC->Client_HideInitiativeTracker();

    // 언로드 중 notify/콜백 발동 방지 — 리스트 비우기 전에 몽타주·반복 타이머 모두 정리
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(QTETimerHandle);

    for (APlayerCharacter* PlayerChar : GetBattlePlayerCharacters())
        if (PlayerChar)
        {
            PlayerChar->StopAnimMontage();
            if (UCombatComponent* Combat = PlayerChar->FindComponentByClass<UCombatComponent>())
                Combat->StopBattleTimers();
        }

    for (AMonsterCharacter* Monster : SpawnedMonsters)
        if (Monster)
        {
            Monster->StopAnimMontage();
            if (UCombatComponent* Combat = Monster->FindComponentByClass<UCombatComponent>())
                Combat->StopBattleTimers();
        }

    if (bPlayersWon && EncounterSourceMonster && !EncounterSourceMonster->IsActorBeingDestroyed())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] DestroyEncounterSourceMonster: Monster=%s"), *EncounterSourceMonster->GetName());
        EncounterSourceMonster->Destroy();
    }

    // TODO: 보상 처리
    // 플레이어를 오버월드로 복귀시키고 던전 레벨 지연 언로드를 예약한다.
    // 레벨/BM은 곧바로 내리지 않고 유지 → 다른 몹과 곧바로 재진입하면 재사용.
    ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass()));
    if (LSM)
        LSM->Server_RequestExitDungeon(DungeonLevelName);

    // BM을 Idle로 리셋해 다음 전투 재사용을 준비 (스폰 몹 파괴 + 리스트/세션/타이머 정리).
    ResetBattle();
}

void ABattleManager::ResetBattle()
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] ResetBattle: Level=%s"), *DungeonLevelName.ToString());

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(QTETimerHandle);
        World->GetTimerManager().ClearTimer(EndBattleTimerHandle);
    }

    // 이전 전투에서 스폰된 몬스터 액터를 실제로 파괴 (배열만 비우면 레벨에 남아 누적됨)
    for (AMonsterCharacter* Monster : SpawnedMonsters)
    {
        if (Monster && !Monster->IsActorBeingDestroyed())
        {
            Monster->Destroy();
        }
    }

    // 플레이어는 오버월드로 복귀하므로 복제 BM 포인터를 정리한다(상주 BM 재사용 시 stale 방지).
    for (ABasePlayerController* PC : RegisteredPlayerControllers)
    {
        if (!PC) continue;
        if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PC->GetPawn()))
            PlayerChar->SetBattleManager(nullptr);
    }

    SpawnedMonsters.Empty();
    RegisteredPlayerControllers.Empty();
    BattlePlayerCharacters.Empty();

    TurnManager = nullptr;
    CurrentTurnCharacter = nullptr;
    ResetBattleStartReadiness();

    // 공격 세션 상태 정리
    SessionTargets.Reset();
    SessionParryCounts.Empty();
    SessionRequiredParryCount = 1;
    SessionAttacker = nullptr;
    bCounterResolved = false;
    bAttackSessionEnded = false;
    PendingCounterPlayers.Reset();

    bEndBattlePending = false;
    EncounterSourceMonster = nullptr;

    BattleState = EBattleState::Idle;
}

void ABattleManager::ReinitForReuse(const TArray<ABasePlayerController*>& InPlayers, const FBattleMonsterSpawnSettings& InMonsterSpawnSettings, AMonsterCharacter* InEncounterSourceMonster)
{
    if (!HasAuthority()) return;

    // 레벨 재사용 시 BeginPlay가 다시 돌지 않으므로, BeginPlay가 하던 초기화를 여기서 수행.
    ResetBattle();
    CacheSpawnPoints();

    for (ABasePlayerController* PC : InPlayers)
    {
        RegisterPlayer(PC);
    }

    SetMonsterSpawnSettings(InMonsterSpawnSettings);
    SetEncounterSourceMonster(InEncounterSourceMonster);

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] ReinitForReuse: Level=%s Players=%d"), *DungeonLevelName.ToString(), RegisteredPlayerControllers.Num());
}

void ABattleManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // BM 액터가 (레벨 언로드 등으로) 파괴될 때 LSM 등록에서 자신을 제거 → 죽은 포인터 잔존 방지.
    if (HasAuthority())
    {
        if (ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass())))
        {
            LSM->UnregisterBattleManager(DungeonLevelName, this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ABattleManager::StartTurn()
{
    if (!HasAuthority() || BattleState != EBattleState::InBattle)
    {
        return;
    }

    // 종료 예약 중이면 다음 턴 시작 금지 (막타 후 딜레이 동안 유령 턴 방지)
    if (bEndBattlePending) return;

    if (!TurnManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 14 StartTurn Blocked: TurnManagerNull"));
        return;
    }

    CurrentTurnCharacter = TurnManager->GetCurrentTurnCharacter();
    if (!CurrentTurnCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 14 StartTurn Blocked: CurrentTurnCharacterNull"));
        return;
    }

    // 턴 순서 관리 UI 업데이트
    for (ABasePlayerController* PC : RegisteredPlayerControllers)
        if (PC) PC->Client_UpdateInitiativeTracker(CurrentTurnCharacter, TurnManager->GetTurnOrder());
    
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 14 StartTurn: Character=%s Cycle=%d"), *CurrentTurnCharacter->GetName(), TurnManager->GetCurrentCycle());


    StartCurrentTurnCharacter();
}

void ABattleManager::StartCurrentTurnCharacter()
{
    if (APlayerCharacter* Player = Cast<APlayerCharacter>(CurrentTurnCharacter))
    {
        StartPlayerTurn(Player);
        return;
    }

    if (AMonsterCharacter* Monster = Cast<AMonsterCharacter>(CurrentTurnCharacter))
    {
        StartMonsterTurn(Monster);
    }
}

void ABattleManager::StartPlayerTurn(APlayerCharacter* Player)
{
    if (!Player)
    {
        return;
    }

    Player->Client_StartPlayerTurn();

    if (BattleCinematicManager)
    {
        BattleCinematicManager->OnPlayerTurnStarted(Player);
    }
}

void ABattleManager::StartMonsterTurn(AMonsterCharacter* Monster)
{
    if (!Monster)
    {
        return;
    }

    for (APlayerCharacter* Player : GetBattlePlayerCharacters())
    {
        if (Player && !Player->IsDead())
        {
            Player->Client_StartDefenseTurn();
        }
    }

    Monster->StartAITurn();

    if (BattleCinematicManager)
    {
        BattleCinematicManager->OnEnemyTurnStarted(Monster, RegisteredPlayerControllers);
    }
}

void ABattleManager::Server_OnTurnEnd_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 20 ServerTurnEnd"));
    OnTurnEnd();
}

void ABattleManager::OnTurnEnd()
{
    if (!HasAuthority()) return;

    // 종료 예약 중이면 턴 진행 중단 (전투 끝났는데 다음 턴 도는 것 방지)
    if (bEndBattlePending) return;

    if (!TurnManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 21 OnTurnEnd Blocked: TurnManagerNull"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 21 OnTurnEnd"));

    ABaseCharacter* TurnCharacter = TurnManager->GetCurrentTurnCharacter();

    if (AMonsterCharacter* CurrentTurnMonster = Cast<AMonsterCharacter>(TurnCharacter)) {
        if (BattleCinematicManager)
        {
            BattleCinematicManager->MoveCameraToBase(RegisteredPlayerControllers);
        }
    }

    if (APlayerCharacter* CurrentTurnPlayer = Cast<APlayerCharacter>(TurnCharacter)) {
        CurrentTurnPlayer->Client_EndPlayerTurn();
    }
    
    // 다음 참가자로 이동
    TurnManager->NextTurn();

    // 다음 참가자 턴 시작
    StartTurn();
}

void ABattleManager::BeginAttackSession(const TArray<APlayerCharacter*>& Targets, ESkillTargetType AttackType, int32 RequiredParries)
{
    if (!HasAuthority()) return;

    SessionAttackType = AttackType;
    SessionTargets = Targets;       // 타겟 명단 확정

    //ParryFailedTargets.Reset();     // 패링 추적 초기화 (낙관적: 아무도 실패 안 한 상태) -> 안씀
   
    // [신규] 세션의 요구 패링 횟수 세팅 및 플레이어별 카운트 맵 초기화
    SessionRequiredParryCount = FMath::Max(1, RequiredParries);
    SessionParryCounts.Empty();
    SessionAttacker = CurrentTurnCharacter; // 이번 세션 공격자 확정
    bCounterResolved = false;       // 반격 미처리 상태로 시작
    bAttackSessionEnded = false;
    PendingCounterPlayers.Reset();

    if (APlayerCharacter* CurrentTurnPlayer = Cast<APlayerCharacter>(SessionAttacker))
    {
        if (BattleCinematicManager)
        {
            BattleCinematicManager->OnPlayerActionExecuted(CurrentTurnPlayer);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BeginAttackSession: Attacker=%s Targets=%d"),
        CurrentTurnCharacter ? *CurrentTurnCharacter->GetName() : TEXT("None"), SessionTargets.Num());
}

// 2. [신규] 패링 성공 횟수 기록용 함수 (적당한 곳에 추가)
void ABattleManager::NotifyParrySuccess(APlayerCharacter* Target)
{
    if (!HasAuthority() || !Target) return;

    // 패링을 성공할 때마다 타겟의 성공 횟수를 1씩 올려서 기록해둡니다.
    int32& Count = SessionParryCounts.FindOrAdd(Target, 0);
    Count++;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 패링 누적 성공: %s (현재 %d회 / 목표 %d회)"), *Target->GetName(), Count, SessionRequiredParryCount);
}

// 이젠 패링 성공 횟수로 기록함 
//void ABattleManager::NotifyParryFailed(APlayerCharacter* Target)
//{
//    if (!HasAuthority() || !Target) return;
//
//    ParryFailedTargets.AddUnique(Target);   // 같은 타겟 중복 기록 방지
//}

bool ABattleManager::ResolveCounter()
{
    // 이미 카운터가 나갔으면 무시
    if (!HasAuthority() || bCounterResolved) return false;

    AMonsterCharacter* AttackingMonster = Cast<AMonsterCharacter>(SessionAttacker);
    if (!AttackingMonster || SessionTargets.IsEmpty()) return false;

    bool bShouldCounter = false;

    // 1. 성공 횟수 검사
    if (SessionAttackType == ESkillTargetType::AllEnemies)
    {
        bShouldCounter = true;
        for (APlayerCharacter* Target : SessionTargets)
        {
            if (Target->IsDead()) continue;
            if (SessionParryCounts.FindRef(Target) < SessionRequiredParryCount)
            {
                bShouldCounter = false;
                break;
            }
        }
    }
    else
    {
        APlayerCharacter* SingleTarget = SessionTargets[0];
        if (SessionParryCounts.FindRef(SingleTarget) >= SessionRequiredParryCount)
        {
            bShouldCounter = true;
        }
    }

    // 2. 검사 결과에 따른 처리
    if (bShouldCounter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] ResolveCounter: 총 패링 요구 횟수(%d회) 전원 달성 → 카운터 발동!"), SessionRequiredParryCount);
        TriggerCounterAttack(AttackingMonster);

        bCounterResolved = true;
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] ResolveCounter: 요구 횟수를 채우지 못해 카운터가 발동하지 않습니다."));
    }

    return false;
}

void ABattleManager::EndAttackSession()
{
    if (!HasAuthority()) return;
    if (bAttackSessionEnded) return;

    if (!PendingCounterPlayers.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] EndAttackSession Deferred: PendingCounters=%d"), PendingCounterPlayers.Num());
        return;
    }

    // 마지막 타격 노티파이(ResolveParry)가 누락된 경우를 대비한 안전망.
    // 이미 ResolveCounter가 호출됐다면 bCounterResolved 가드로 다시 실행되지 않는다.
    if (ResolveCounter())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] EndAttackSession Deferred: CounterStarted"));
        return;
    }

    FinishAttackSessionAndAdvanceTurn();
}

void ABattleManager::CleanupAttackSession()
{
    SessionTargets.Reset();

    /* ParryFailedTargets.Reset(); -> 안씀*/

    PendingCounterPlayers.Reset();
    SessionParryCounts.Empty();
    SessionRequiredParryCount = 1;

    if (SessionAttacker)
    {
        if (UCombatComponent* Combat = SessionAttacker->FindComponentByClass<UCombatComponent>())
        {
            Combat->Server_SetCombatState(ECombatState::Idle);
            Combat->HandleStateChanged(ECombatState::Idle);
        }
    }

    bCounterResolved = false;
    SessionAttacker = nullptr;
}

void ABattleManager::FinishAttackSessionAndAdvanceTurn()
{
    if (!HasAuthority()) return;
    if (bAttackSessionEnded) return;

    bAttackSessionEnded = true;

    CleanupAttackSession();

    if (BattleState == EBattleState::InBattle)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] EndAttackSession -> OnTurnEnd"));
        OnTurnEnd();
    }
}

void ABattleManager::TriggerCounterAttack(ABaseCharacter* TargetMonster)
{
    if (!HasAuthority() || !TargetMonster) return;

    PendingCounterPlayers.Reset();
    const bool bGroup = (SessionAttackType == ESkillTargetType::AllEnemies);

    auto LogCounterLS = [](const FString& Message)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CounterLS][Server] %s"), *Message);
    };

    LogCounterLS(FString::Printf(
        TEXT("TriggerCounterAttack: TargetMonster=%s Group=%s SessionTargets=%d Participants=%d CinematicManager=%s"),
        *GetNameSafe(TargetMonster),
        bGroup ? TEXT("true") : TEXT("false"),
        SessionTargets.Num(),
        RegisteredPlayerControllers.Num(),
        BattleCinematicManager ? TEXT("Valid") : TEXT("NULL")
    ));

    if (BattleCinematicManager)
    {
        bool bRequestedSequence = false;
        for (APlayerCharacter* Player : SessionTargets)
        {
            LogCounterLS(FString::Printf(
                TEXT("CounterSequence origin candidate: Player=%s Dead=%s"),
                *GetNameSafe(Player),
                Player && Player->IsDead() ? TEXT("true") : TEXT("false")
            ));

            if (Player && !Player->IsDead())
            {
                LogCounterLS(FString::Printf(
                    TEXT("Request PlayCounterLevelSequence: OriginPlayer=%s Group=%s"),
                    *GetNameSafe(Player),
                    bGroup ? TEXT("true") : TEXT("false")
                ));
                BattleCinematicManager->PlayCounterLevelSequence(Player, bGroup, RegisteredPlayerControllers);
                bRequestedSequence = true;
                break;
            }
        }

        if (!bRequestedSequence)
        {
            LogCounterLS(TEXT("PlayCounterLevelSequence skipped: no alive SessionTargets"));
        }
    }
    else
    {
        LogCounterLS(TEXT("PlayCounterLevelSequence skipped: BattleCinematicManager is NULL"));
    }

    for (APlayerCharacter* Player : SessionTargets)
    {
        if (!Player) continue;

        UCombatComponent* Combat = Player->FindComponentByClass<UCombatComponent>();
        if (Combat)
        {
            const int32 SuccessParryCount = FMath::Max(1, SessionParryCounts.FindRef(Player));

            PendingCounterPlayers.Add(Player);
            UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] CounterAttack: Player=%s SuccessParryCount=%d"),
                *Player->GetName(),
                SuccessParryCount);
            Combat->PerformCounterAttack(TargetMonster, SuccessParryCount, bGroup);
        }
    }
}

void ABattleManager::NotifyCounterFinished(APlayerCharacter* Player)
{
    if (!HasAuthority() || !Player) return;

    PendingCounterPlayers.Remove(Player);

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] CounterFinished: Player=%s Pending=%d"),
        *Player->GetName(),
        PendingCounterPlayers.Num());

    if (PendingCounterPlayers.IsEmpty())
    {
        EndAttackSession();
    }
}

void ABattleManager::Server_RegisterPlayer_Implementation(ABasePlayerController* PlayerController)
{
    RegisterPlayer(PlayerController);
}

void ABattleManager::RegisterPlayer(ABasePlayerController* PlayerController)
{
    if (PlayerController && !RegisteredPlayerControllers.Contains(PlayerController))
    {
        RegisteredPlayerControllers.Add(PlayerController);

        UE_LOG(LogTemp, Log, TEXT("BattleManager: 플레이어 등록 - %s"), *PlayerController->GetName());
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 06 RegisterPlayer: PC=%s Pawn=%s Total=%d"), *PlayerController->GetName(), PlayerController->GetPawn() ? *PlayerController->GetPawn()->GetName() : TEXT("None"), RegisteredPlayerControllers.Num());

    }
}

void ABattleManager::Server_UnregisterPlayer_Implementation(ABasePlayerController* PlayerController)
{
    UnregisterPlayer(PlayerController);
}

void ABattleManager::UnregisterPlayer(ABasePlayerController* PlayerController)
{
    RegisteredPlayerControllers.Remove(PlayerController);
}

void ABattleManager::RegisterMonster(AMonsterCharacter* Monster)
{
    if (Monster)
    {
        SpawnedMonsters.Add(Cast<AMonsterCharacter>(Monster));
        Monster->SetBattleManager(this);
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 06 RegisterMonster: Monster=%s Total=%d"), *Monster->GetName(), SpawnedMonsters.Num());
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("NO AMonsterCharacter*!!!!!!!!!!!!!!!!!!!!!!!!!"));
    }
}

void ABattleManager::UnregisterMonster(AMonsterCharacter* Monster)
{
    SpawnedMonsters.Remove(Cast<AMonsterCharacter>(Monster));
}

void ABattleManager::Server_RegisterMonster_Implementation(AMonsterCharacter* Monster) {
    RegisterMonster(Monster);
}

bool ABattleManager::CheckParryResult(ABaseCharacter* Target)
{
    if (!HasAuthority()) return false;
    if (!Target) return false;

    UCombatComponent* CombatComp = Target->FindComponentByClass<UCombatComponent>();
    if (!CombatComp) return false;

    return CombatComp->GetCurrentState() == ECombatState::Parrying;
}

void ABattleManager::RefreshBattlePlayerCharactersFromControllers()
{
    if (!HasAuthority()) return;

    BattlePlayerCharacters.Empty();

    for (ABasePlayerController* PC : RegisteredPlayerControllers)
    {
        if (!PC)
        {
            UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] RefreshBattlePlayers Skip: PCNull"));
            continue;
        }

        APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PC->GetPawn());
        if (!PlayerChar)
        {
            UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] RefreshBattlePlayers Skip: PawnNotPlayer PC=%s Pawn=%s"),
                *PC->GetName(),
                PC->GetPawn() ? *PC->GetPawn()->GetName() : TEXT("None"));
            continue;
        }

        BattlePlayerCharacters.AddUnique(PlayerChar);
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] RefreshBattlePlayers: Players=%d Controllers=%d"),
        BattlePlayerCharacters.Num(),
        RegisteredPlayerControllers.Num());

    ForceNetUpdate();
}

void ABattleManager::AssignBattleManagerToParticipants(const TArray<ABaseCharacter*>& Participants)
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] AssignBattleManager: Participants=%d BM=%s"),
        Participants.Num(),
        *GetName());

    for (ABaseCharacter* Participant : Participants)
    {
        if (!Participant)
        {
            continue;
        }

        Participant->SetBattleManager(this);

        if (UCombatComponent* Combat = Participant->FindComponentByClass<UCombatComponent>())
        {
            Combat->SaveOriginalPosition();
        }
    }
}

void ABattleManager::NotifyPlayersEnterBattle()
{
    if (!HasAuthority()) return;

    const TArray<APlayerCharacter*> PlayerCharacters = GetBattlePlayerCharacters();
    for (APlayerCharacter* PlayerChar : PlayerCharacters)
    {
        NotifyPlayerEnterBattle(PlayerChar);
    }

    if (PlayerCharacters.IsEmpty())
    {
        bBattleStartMontagesFinished = true;
        TryStartFirstTurn();
    }
}

void ABattleManager::NotifyPlayerEnterBattle(APlayerCharacter* PlayerChar)
{
    if (!HasAuthority() || !PlayerChar)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] NotifyPlayerEnterBattle: Player=%s Controller=%s BM=%s"),
        *PlayerChar->GetName(),
        PlayerChar->GetController() ? *PlayerChar->GetController()->GetName() : TEXT("None"),
        PlayerChar->GetBattleManager() ? *PlayerChar->GetBattleManager()->GetName() : TEXT("None"));

    PlayerChar->Client_EnterBattleMode();
    PlayerChar->SetInBattle(true);
    PreparePlayerForBattleStart(PlayerChar);
}

void ABattleManager::PreparePlayerForBattleStart(APlayerCharacter* PlayerChar)
{
    if (!HasAuthority() || !PlayerChar)
    {
        return;
    }

    if (UCombatComponent* Combat = PlayerChar->FindComponentByClass<UCombatComponent>())
    {
        Combat->SetCombatState(ECombatState::Idle);
        Combat->HandleStateChanged(ECombatState::Idle);
        PlayBattleStartMontageOrMarkReady(PlayerChar);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BattleStartCombatMissing: Player=%s"), *PlayerChar->GetName());
    MarkBattleStartMontageFinished(Cast<ABasePlayerController>(PlayerChar->GetController()));
}

void ABattleManager::PlayBattleStartMontageOrMarkReady(APlayerCharacter* PlayerChar)
{
    if (!HasAuthority() || !PlayerChar)
    {
        return;
    }

    UCombatComponent* Combat = PlayerChar->FindComponentByClass<UCombatComponent>();
    if (!Combat)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BattleStartCombatMissing: Player=%s"), *PlayerChar->GetName());
        MarkBattleStartMontageFinished(Cast<ABasePlayerController>(PlayerChar->GetController()));
        return;
    }

    if (Combat->BattleStartMontage)
    {
        Combat->PlayMontageWithSync(Combat->BattleStartMontage);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BattleStartMontageMissing: Player=%s"), *PlayerChar->GetName());
    MarkBattleStartMontageFinished(Cast<ABasePlayerController>(PlayerChar->GetController()));
}

TArray<APlayerCharacter*> ABattleManager::GetBattlePlayerCharacters() const
{
    TArray<APlayerCharacter*> Characters;
    for (APlayerCharacter* PlayerChar : BattlePlayerCharacters)
    {
        if (PlayerChar)
            Characters.Add(PlayerChar);
    }
    return Characters;
}

TArray<APlayerCharacter*> ABattleManager::GetRegisteredPlayerCharacters() const
{
    return GetBattlePlayerCharacters();
}

TArray<ABaseCharacter*> ABattleManager::BuildParticipants()
{
    TArray<ABaseCharacter*> Participants;

    for (APlayerCharacter* PlayerChar : GetBattlePlayerCharacters())
        Participants.Add(PlayerChar);

    for (ABaseCharacter* Monster : SpawnedMonsters)
        if (Monster)
            Participants.Add(Monster);

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 13 BuildParticipants: Count=%d Players=%d Monsters=%d"), Participants.Num(), RegisteredPlayerControllers.Num(), SpawnedMonsters.Num());

    return Participants;
}

void ABattleManager::NotifyCharacterDead(ABaseCharacter* DeadCharacter)
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 24 CharacterDead: Character=%s"), DeadCharacter ? *DeadCharacter->GetName() : TEXT("None"));

    if (!TurnManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 24 CharacterDead Blocked: TurnManagerNull"));
        return;
    }

    TurnManager->RemoveCharacter(DeadCharacter);
    CheckBattleResult();

    if (!TurnManager || BattleState != EBattleState::InBattle)
    {
        return;
    }

    for (ABasePlayerController* PC : RegisteredPlayerControllers)
        if (PC)
            PC->Client_UpdateInitiativeTracker(TurnManager->GetCurrentTurnCharacter(), TurnManager->GetTurnOrder());
}

void ABattleManager::OnRep_BattleState()
{

    // 클라이언트에서 BattleState 변경 감지
      // InBattle 진입 시 UI 표시 등
}

void ABattleManager::OnRep_BattlePlayerCharacters()
{
    int32 ValidCount = 0;
    for (APlayerCharacter* PlayerChar : BattlePlayerCharacters)
        if (PlayerChar) ++ValidCount;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] BattlePlayersReplicated: Received=%d Valid=%d"), BattlePlayerCharacters.Num(), ValidCount);

    // HUD 슬롯은 전투 플레이어 Pawn 목록을 전부 읽어야 하므로, 부분 resolve 상태에서는 알리지 않는다.
    if (BattlePlayerCharacters.Num() > 0 && ValidCount == BattlePlayerCharacters.Num())
    {
        OnBattlePlayersReplicated.Broadcast();
    }
}

void ABattleManager::OnRep_CurrentTurnCharacter()
{
    // 전투가 끝나면 ResetBattle 이 CurrentTurnCharacter 를 nullptr 로 만들고, 그 변경이
    // 클라에 복제되며 이 OnRep 이 클라에서만 실행된다(호스트=권위 측은 OnRep 미실행).
    // UpdateBattleInputState() 는 두 분기 모두 SetIgnoreMoveInput(true) 를 걸기 때문에,
    // 종료 후(턴 캐릭터 없음) 이를 호출하면 이미 오버월드로 복귀해 이동이 풀린 클라의
    // 이동을 다시 막아버린다. → 전투 중(턴 캐릭터 유효)일 때만 입력 게이팅을 적용한다.
    if (!CurrentTurnCharacter)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerCharacter* LocalChar = Cast<APlayerCharacter>(PC->GetPawn()))
            {
                LocalChar->UpdateBattleInputState();
            }
        }
    }
}

void ABattleManager::SpawnBattleMonsters()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!SpawnedMonsters.IsEmpty())
    {
        return;
    }

    const bool bUseEncounterSpawnSettings = MonsterSpawnSettings.HasValidMonsterClass();
    TArray<TSubclassOf<AMonsterCharacter>> ValidSpawnMonsterClasses;
    if (bUseEncounterSpawnSettings)
    {
        for (const TSubclassOf<AMonsterCharacter>& MonsterClass : MonsterSpawnSettings.MonsterClasses)
        {
            if (MonsterClass)
            {
                ValidSpawnMonsterClasses.Add(MonsterClass);
            }
        }
    }
    else
    {
        for (const TSubclassOf<AMonsterCharacter>& MonsterClass : MonsterClasses)
        {
            if (MonsterClass)
            {
                ValidSpawnMonsterClasses.Add(MonsterClass);
            }
        }
    }

    if (ValidSpawnMonsterClasses.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] SpawnBattleMonsters Blocked: SpawnMonsterClassesEmpty"));
        return;
    }

    if (EnemySpawnPoints.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] SpawnBattleMonsters Blocked: EnemySpawnPointsEmpty"));
        return;
    }

    const int32 RequestedMinCount = bUseEncounterSpawnSettings ? MonsterSpawnSettings.MinMonsterCount : MinMonsterCount;
    const int32 RequestedMaxCount = bUseEncounterSpawnSettings ? MonsterSpawnSettings.MaxMonsterCount : MaxMonsterCount;
    const int32 MinCount = FMath::Max(1, FMath::Min(RequestedMinCount, RequestedMaxCount));
    const int32 MaxCount = FMath::Max(MinCount, FMath::Max(RequestedMinCount, RequestedMaxCount));
    const int32 Count = FMath::RandRange(MinCount, MaxCount);
    const int32 SpawnCount = FMath::Min(Count, EnemySpawnPoints.Num());

    for (int32 i = 0; i < SpawnCount; i++)
    {
        TSubclassOf<AMonsterCharacter> MonsterClass = ValidSpawnMonsterClasses[FMath::RandRange(0, ValidSpawnMonsterClasses.Num() - 1)];

        ADungeonSpawnPoint* SpawnPoint = EnemySpawnPoints[i];
        if (!SpawnPoint)
        {
            continue;
        }

        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AMonsterCharacter* Monster = GetWorld()->SpawnActor<AMonsterCharacter>(
            MonsterClass,
            SpawnPoint->GetActorLocation(),
            SpawnPoint->GetActorRotation(),
            Params
        );

        if (!Monster)
        {
            UE_LOG(LogTemp, Warning, TEXT("No Monster! Continue..."));
            continue;
        }

        RegisterMonster(Monster);
        Monster->EnterBattle();

        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] SpawnBattleMonster: Monster=%s Index=%d"), *Monster->GetName(), i);
    }
}

void ABattleManager::ApplyCameraToPlayers(float BlendTime)
{
    if (!HasAuthority())
        return;

    if (!BattleCinematicManager)
        return;

    BattleCinematicManager->ApplySharedCamera(RegisteredPlayerControllers, BlendTime);
}

void ABattleManager::ShowAttackQTEToPlayers(float Duration, float SuccessStartTime, float SuccessEndTime, FVector2D ScreenPosition)
{
    if (!HasAuthority()) return;

    for (ABasePlayerController* PC : RegisteredPlayerControllers)
    {
        if (!PC) continue;

        APlayerCharacter* Char = PC->GetPawn<APlayerCharacter>();
        if (Char == CurrentTurnCharacter)
            PC->Client_ShowAttackQTE(Duration, SuccessStartTime, SuccessEndTime, ScreenPosition);  // 판정 있음
        else
            PC->Client_ShowQTESpectator(Duration, ScreenPosition);  // UI만
    }

    // 몽타주 슬로우 — 다음 QTE 노티파이 발동 지연
    if (CurrentTurnCharacter)
    {
        if (UAnimInstance* Anim = CurrentTurnCharacter->GetMesh()->GetAnimInstance())
        {
            UAnimMontage* CurrentMontage = Anim->GetCurrentActiveMontage();
            CurrentTurnCharacter->Multicast_SetMontagePlayRate(CurrentMontage, 0.1f);
            UE_LOG(LogTemp, Warning, TEXT("[QTE] MontageSlowed: Character=%s Montage=%s Rate=0.1"),
                *CurrentTurnCharacter->GetName(),
                CurrentMontage ? *CurrentMontage->GetName() : TEXT("None"));
        }
    }

    // 서버 타임아웃 타이머 — Duration 후 자동 실패 처리
    GetWorld()->GetTimerManager().SetTimer(
        QTETimerHandle,
        this,
        &ABattleManager::OnQTETimeout,
        Duration,
        false
    );

    UE_LOG(LogTemp, Warning, TEXT("[QTE] ShowAttackQTEToPlayers: Players=%d Duration=%.2f Success=%.2f~%.2f"),
        RegisteredPlayerControllers.Num(),
        Duration,
        SuccessStartTime,
        SuccessEndTime);
}

void ABattleManager::OnQTETimeout()
{
    UE_LOG(LogTemp, Warning, TEXT("[QTE] OnQTETimeout: 타임아웃 → 실패 처리"));

    if (APlayerCharacter* Attacker = Cast<APlayerCharacter>(CurrentTurnCharacter))
    {
        Attacker->AddQTEResult(false);
    }

    NotifyQTEResultToAllPlayers(false);
}

void ABattleManager::CancelQTETimer()
{
    GetWorld()->GetTimerManager().ClearTimer(QTETimerHandle);
}

void ABattleManager::NotifyQTEResultToAllPlayers(bool bSuccess)
{
    if (!HasAuthority()) return;

    for (ABasePlayerController* PC : RegisteredPlayerControllers)
    {
        if (PC)
        {
            PC->Client_OnQTEResult(bSuccess);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[QTE] NotifyQTEResult: Players=%d Success=%s"),
        RegisteredPlayerControllers.Num(),
        bSuccess ? TEXT("true") : TEXT("false"));
}

void ABattleManager::CheckBattleResult()
{
    if (BattleState != EBattleState::InBattle) return;

    bool bAllMonstersDead = true;
    for (ABaseCharacter* Monster : SpawnedMonsters)
        if (Monster && !Monster->IsDead()) { bAllMonstersDead = false; break; }

    bool bAllPlayersDead = true;
    for (APlayerCharacter* PlayerChar : GetBattlePlayerCharacters())
        if (PlayerChar && !PlayerChar->IsDead()) { bAllPlayersDead = false; break; }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 25 CheckBattleResult: AllMonstersDead=%s AllPlayersDead=%s"), bAllMonstersDead ? TEXT("true") : TEXT("false"), bAllPlayersDead ? TEXT("true") : TEXT("false"));

    if (bAllMonstersDead || bAllPlayersDead)
    {
        // 광역기로 한 프레임에 여러 마리 죽어도 종료는 한 번만 예약 (다중 사망 가드)
        if (bEndBattlePending) return;
        bEndBattlePending = true;

        const bool bPlayersWon = bAllMonstersDead && !bAllPlayersDead;

        // 막타 데미지/노티파이 콜스택을 다 빠져나온 뒤(죽는 연출 재생 후) 안전하게 종료.
        if (UWorld* World = GetWorld())
        {
            FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ABattleManager::EndBattle, bPlayersWon);
            World->GetTimerManager().SetTimer(EndBattleTimerHandle, Delegate, FMath::Max(BattleEndDelay, 0.01f), false);
        }
        else
        {
            EndBattle(bPlayersWon);
        }
    }
}

void ABattleManager::Multicast_OnBattleActionExecuted_Implementation(
    ABaseCharacter* Attacker,
    ABaseCharacter* Target,
    EBattleActionEventType ActionType
)
{
    OnBattleActionExecuted.Broadcast(Attacker, Target, ActionType);
}

void ABattleManager::Multicast_OnDamageDealt_Implementation(
    ABaseCharacter* Target,
    float Damage
)
{
    OnDamageDealt.Broadcast(Target, Damage);
}

void ABattleManager::Multicast_OnParried_Implementation(ABaseCharacter* Target)
{
    OnParried.Broadcast(Target);
}

void ABattleManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}
