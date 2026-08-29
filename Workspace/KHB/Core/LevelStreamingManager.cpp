#include "LevelStreamingManager.h"
#include "BattleManager.h"
#include "../Player/BasePlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "../Character/BaseCharacter.h"
#include "../Character/MonsterCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../Component/StatComponent.h"
#include "TimerManager.h"

namespace
{
    void StopLevelStreamingPawnMovement(APawn* Pawn)
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

ALevelStreamingManager::ALevelStreamingManager()
{
    bReplicates = true;
}

void ALevelStreamingManager::Server_RequestEnterDungeon_Implementation(
    const TArray<ABasePlayerController*>& Players,
    FName LevelName,
    const FBattleMonsterSpawnSettings& MonsterSpawnSettings,
    AMonsterCharacter* EncounterMonster)
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 04 LSM EnterDungeon: Level=%s Players=%d MonsterClasses=%d Count=%d~%d EncounterMonster=%s"),
        *LevelName.ToString(),
        Players.Num(),
        MonsterSpawnSettings.MonsterClasses.Num(),
        MonsterSpawnSettings.MinMonsterCount,
        MonsterSpawnSettings.MaxMonsterCount,
        EncounterMonster ? *EncounterMonster->GetName() : TEXT("None"));

    if (ActiveDungeons.Contains(LevelName))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 04 EnterDungeon Blocked: DungeonAlreadyActive Level=%s"), *LevelName.ToString());
        return;
    }

    if (UnloadingLevels.Contains(LevelName))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 04 EnterDungeon Blocked: DungeonUnloading Level=%s"), *LevelName.ToString());
        return;
    }

    // [멀티 던전] 다른 던전이 상주 중이면 먼저 언로드하고, 언로드 완료 후 이 진입 요청을 재개한다.
    if (!LastLoadedDungeon.IsNone() && LastLoadedDungeon != LevelName)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 SwitchDungeon: Unload=%s Enter=%s"), *LastLoadedDungeon.ToString(), *LevelName.ToString());

        PendingEnterPlayers = Players;
        PendingEnterLevel = LevelName;
        PendingEnterSpawnSettings = MonsterSpawnSettings;
        PendingEnterMonster = EncounterMonster;
        bHasPendingEnter = true;

        BeginDungeonUnload(LastLoadedDungeon, Players);
        return;
    }

    ABattleManager* ReusableBattleManager = nullptr;
    if (ABattleManager** FoundBattleManager = BattleManagers.Find(LevelName))
    {
        ReusableBattleManager = *FoundBattleManager;
        if (!IsValid(ReusableBattleManager))
        {
            BattleManagers.Remove(LevelName);
            ReusableBattleManager = nullptr;
        }
        else if (!ReusableBattleManager->IsReadyForNewBattle())
        {
            UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 04 EnterDungeon Blocked: BattleManagerBusy Level=%s BM=%s"),
                *LevelName.ToString(),
                *ReusableBattleManager->GetName());
            return;
        }
    }

    CancelPendingDungeonUnload(LevelName);

    for (ABasePlayerController* Player : Players)
    {
        if (!Player)
        {
            continue;
        }

        if (APawn* Pawn = Player->GetPawn())
        {
            StopLevelStreamingPawnMovement(Pawn);
            if (ABaseCharacter* Character = Cast<ABaseCharacter>(Pawn))
            {
                if (UStatComponent* Stat = Character->GetStatComponent())
                {
                    Stat->Server_SetAP(3.0f);
                }
            }
            UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 04 SaveOverworldTransform: PC=%s Pawn=%s Location=%s"), *Player->GetName(), *Pawn->GetName(), *Pawn->GetActorLocation().ToString());
            SaveOverworldTransform(Player, Pawn->GetActorTransform());
        }

        Player->DisableInput(Player);
        Player->SetIgnoreMoveInput(true);
        Player->SetIgnoreLookInput(true);
        Player->Client_PrepareDungeonEnter();
    }

    PendingPlayers.Add(LevelName, Players);
    LoadedPlayers.Remove(LevelName);
    SpawnPointSentPlayers.Remove(LevelName);
    SpawnCompletedPlayers.Remove(LevelName);
    PendingMonsterSpawnSettings.Add(LevelName, MonsterSpawnSettings);
    PendingEncounterMonsters.Add(LevelName, EncounterMonster);

    if (ReusableBattleManager)
    {
        PendingPlayers.Remove(LevelName);
        ActiveDungeons.Add(LevelName, Players);
        ReusableBattleManager->ReinitForReuse(Players, MonsterSpawnSettings, EncounterMonster);

        for (ABasePlayerController* Player : Players)
        {
            NotifyPlayerLoaded(Player, LevelName);
        }

        LastLoadedDungeon = LevelName;
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 04 ReuseLoadedDungeon: Level=%s BM=%s"),
            *LevelName.ToString(),
            *ReusableBattleManager->GetName());
        return;
    }

    FLatentActionInfo LatentInfo;
    UGameplayStatics::LoadStreamLevel(this, LevelName, true, false, LatentInfo);

    LoadLevelForPlayers(LevelName, Players);
    LastLoadedDungeon = LevelName;
}

TArray<ABasePlayerController*> ALevelStreamingManager::GetDungeonPlayers(FName LevelName)
{
    TArray<ABasePlayerController*>* Players = PendingPlayers.Find(LevelName);
    if (!Players)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 06 GetDungeonPlayers Failed: Level=%s"), *LevelName.ToString());
        return {};
    }

    ActiveDungeons.Add(LevelName, *Players);
    PendingPlayers.Remove(LevelName);

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 06 GetDungeonPlayers: Level=%s Players=%d"), *LevelName.ToString(), ActiveDungeons[LevelName].Num());

    return ActiveDungeons[LevelName];
}

FBattleMonsterSpawnSettings ALevelStreamingManager::GetBattleMonsterSpawnSettings(FName LevelName) const
{
    if (const FBattleMonsterSpawnSettings* Settings = PendingMonsterSpawnSettings.Find(LevelName))
    {
        return *Settings;
    }

    return FBattleMonsterSpawnSettings();
}

AMonsterCharacter* ALevelStreamingManager::GetPendingEncounterMonster(FName LevelName) const
{
    if (const TObjectPtr<AMonsterCharacter>* EncounterMonster = PendingEncounterMonsters.Find(LevelName))
    {
        return EncounterMonster->Get();
    }

    return nullptr;
}

void ALevelStreamingManager::RegisterBattleManager(FName LevelName, ABattleManager* BattleManager)
{
    UnloadingLevels.Remove(LevelName);
    BattleManagers.Add(LevelName, BattleManager);
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 07 RegisterBattleManager: Level=%s BM=%s"), *LevelName.ToString(), BattleManager ? *BattleManager->GetName() : TEXT("None"));
    TrySendSpawnPointsForLoadedPlayers(LevelName);
}

void ALevelStreamingManager::UnregisterBattleManager(FName LevelName, ABattleManager* BattleManager)
{
    ABattleManager** RegisteredBattleManager = BattleManagers.Find(LevelName);
    if (RegisteredBattleManager && *RegisteredBattleManager == BattleManager)
    {
        BattleManagers.Remove(LevelName);
        UnloadingLevels.Remove(LevelName);
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 07 UnregisterBattleManager: Level=%s BM=%s"),
            *LevelName.ToString(),
            BattleManager ? *BattleManager->GetName() : TEXT("None"));
    }
}

void ALevelStreamingManager::NotifyPlayerLoaded(ABasePlayerController* Player, FName LevelName)
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 08 PlayerLoaded: Level=%s PC=%s"), *LevelName.ToString(), Player ? *Player->GetName() : TEXT("None"));

    if (!Player)
    {
        return;
    }

    LoadedPlayers.FindOrAdd(LevelName).AddUnique(Player);
    TrySendSpawnPointsForLoadedPlayers(LevelName);
}

void ALevelStreamingManager::TrySendSpawnPointsForLoadedPlayers(FName LevelName)
{
    ABattleManager** BM = BattleManagers.Find(LevelName);
    if (!BM || !*BM)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 08 PlayerLoaded Pending: BattleManagerNotFound Level=%s"), *LevelName.ToString());
        return;
    }

    TArray<ABasePlayerController*>* Players = LoadedPlayers.Find(LevelName);
    if (!Players)
    {
        return;
    }

    TArray<ABasePlayerController*>& SentPlayers = SpawnPointSentPlayers.FindOrAdd(LevelName);
    for (ABasePlayerController* Player : *Players)
    {
        if (!Player || SentPlayers.Contains(Player))
        {
            continue;
        }

        SentPlayers.Add(Player);
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 08 ProcessLoadedPlayer: Level=%s PC=%s"), *LevelName.ToString(), *Player->GetName());
        (*BM)->SendSpawnPoint(Player);
    }
}

void ALevelStreamingManager::NotifyPlayerSpawnComplete(ABasePlayerController* Player, FName LevelName)
{
    TArray<ABasePlayerController*>* Active = ActiveDungeons.Find(LevelName);
    ABattleManager** BM = BattleManagers.Find(LevelName);
    if (!Active || !BM)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 10 SpawnComplete Blocked: Level=%s Active=%s BM=%s"), *LevelName.ToString(), Active ? TEXT("Valid") : TEXT("Null"), BM ? TEXT("Valid") : TEXT("Null"));
        return;
    }

    SpawnCompletedPlayers.FindOrAdd(LevelName).AddUnique(Player);
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 10 SpawnComplete: Level=%s PC=%s Ready=%d/%d"), *LevelName.ToString(), Player ? *Player->GetName() : TEXT("None"), SpawnCompletedPlayers[LevelName].Num(), Active->Num());

    if (SpawnCompletedPlayers[LevelName].Num() == Active->Num())
    {
        SpawnCompletedPlayers.Remove(LevelName);
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 11 AllPlayersReady: Level=%s"), *LevelName.ToString());
        (*BM)->StartBattle();
    }
}

void ALevelStreamingManager::NotifyPlayerUnloaded(ABasePlayerController* Player, FName LevelName)
{
    // 입장의 NotifyPlayerSpawnComplete 와 대칭인 언로드 완료 배리어.
    // 언로드 대상 인원은 전환 시퀀스가 PendingUnloadPlayers 에 담아둔 인원 기준으로 친다.
    TArray<ABasePlayerController*>* UnloadingPlayers = PendingUnloadPlayers.Find(LevelName);
    if (!UnloadingPlayers)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 PlayerUnloaded Blocked: NoPendingUnload Level=%s PC=%s"), *LevelName.ToString(), Player ? *Player->GetName() : TEXT("None"));
        return;
    }

    UnloadCompletedPlayers.FindOrAdd(LevelName).AddUnique(Player);
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 PlayerUnloaded: Level=%s PC=%s Ready=%d/%d"),
        *LevelName.ToString(),
        Player ? *Player->GetName() : TEXT("None"),
        UnloadCompletedPlayers[LevelName].Num(),
        UnloadingPlayers->Num());

    // 전원 언로드 완료 → 배리어 통과. 잠금 해제까지가 이 단계의 책임이고,
    // 다음 단계(신규 던전 로드)는 전환 시퀀스에서 이 완료 시점에 연결한다.
    if (UnloadCompletedPlayers[LevelName].Num() == UnloadingPlayers->Num())
    {
        UnloadCompletedPlayers.Remove(LevelName);
        PendingUnloadPlayers.Remove(LevelName);
        UnloadingLevels.Remove(LevelName);
        if (LastLoadedDungeon == LevelName)
        {
            LastLoadedDungeon = NAME_None;
        }
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 AllPlayersUnloaded: Level=%s"), *LevelName.ToString());

        // 보류된 진입 요청 재개. LastLoadedDungeon 이 비워졌으므로 이번엔 신규 로드 경로를 탄다.
        if (bHasPendingEnter)
        {
            bHasPendingEnter = false;
            UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 ResumePendingEnter: Enter=%s"), *PendingEnterLevel.ToString());
            Server_RequestEnterDungeon_Implementation(PendingEnterPlayers, PendingEnterLevel, PendingEnterSpawnSettings, PendingEnterMonster);
        }
    }
}

void ALevelStreamingManager::Server_RequestExitDungeon_Implementation(FName LevelName)
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 30 LSM ExitDungeon: Level=%s"), *LevelName.ToString());

    TArray<ABasePlayerController*>* Players = ActiveDungeons.Find(LevelName);
    if (!Players)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 30 ExitDungeon Blocked: ActivePlayersNotFound Level=%s"), *LevelName.ToString());
        return;
    }

    for (ABasePlayerController* PC : *Players)
    {
        if (PC)
        {
            PC->DisableInput(PC);
            PC->SetIgnoreMoveInput(true);
            PC->SetIgnoreLookInput(true);
            PC->Client_PrepareDungeonExit();
        }
    }

    for (ABasePlayerController* PC : *Players)
    {
        ReturnToOverworld(PC);
    }

    // [공유 상주 던전] LV_Dungeon은 여러 몹이 공유하며 상시 유지되는 레벨이다.
    // 전투 종료 시 레벨을 언로드하면 안 된다:
    //   - 사전 배치된 BattleManager / DungeonSpawnPoint 가 파괴되고
    //   - 상주 레벨이라 BM EndPlay 가 안 돌면 UnloadingLevels 가 영원히 안 비워져
    //     다음 Server_RequestEnterDungeon 이 UnloadingLevels 가드에 걸려 영구 차단된다.
    // BM 은 EndBattle 안의 ResetBattle 로 Idle 상태가 되어 다음 전투에 그대로 재사용된다.
    // (예전: ScheduleDungeonUnload(LevelName, *Players) — 더 이상 호출하지 않음)

    ActiveDungeons.Remove(LevelName);
    LoadedPlayers.Remove(LevelName);
    SpawnPointSentPlayers.Remove(LevelName);
    SpawnCompletedPlayers.Remove(LevelName);
    PendingMonsterSpawnSettings.Remove(LevelName);
    PendingEncounterMonsters.Remove(LevelName);
}

void ALevelStreamingManager::CancelPendingDungeonUnload(FName LevelName)
{
    if (FTimerHandle* TimerHandle = PendingUnloadTimers.Find(LevelName))
    {
        GetWorldTimerManager().ClearTimer(*TimerHandle);
        PendingUnloadTimers.Remove(LevelName);
        PendingUnloadPlayers.Remove(LevelName);

        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 CancelDungeonUnload: Level=%s"), *LevelName.ToString());
    }
}

void ALevelStreamingManager::ScheduleDungeonUnload(FName LevelName, const TArray<ABasePlayerController*>& Players)
{
    PendingUnloadPlayers.Add(LevelName, Players);

    FTimerHandle& TimerHandle = PendingUnloadTimers.FindOrAdd(LevelName);
    FTimerDelegate UnloadDelegate = FTimerDelegate::CreateUObject(this, &ALevelStreamingManager::ExecutePendingDungeonUnload, LevelName);
    GetWorldTimerManager().SetTimer(TimerHandle, UnloadDelegate, FMath::Max(DungeonUnloadDelay, 0.01f), false);

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 ScheduleDungeonUnload: Level=%s Delay=%.2f Players=%d"),
        *LevelName.ToString(),
        DungeonUnloadDelay,
        Players.Num());
}

void ALevelStreamingManager::ExecutePendingDungeonUnload(FName LevelName)
{
    PendingUnloadTimers.Remove(LevelName);
    UnloadingLevels.Add(LevelName);

    if (TArray<ABasePlayerController*>* Players = PendingUnloadPlayers.Find(LevelName))
    {
        UnLoadLevelForPlayers(LevelName, *Players);
    }

    PendingUnloadPlayers.Remove(LevelName);

    FLatentActionInfo LatentInfo;
    UGameplayStatics::UnloadStreamLevel(this, LevelName, LatentInfo, false);

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 ExecuteDungeonUnload: Level=%s"), *LevelName.ToString());
}

ABattleManager* ALevelStreamingManager::GetBattleManagerForActor(AActor* Actor)
{
    if (!Actor) return nullptr;

    // 서버: BattleManagers 맵으로 직접 조회
    if (HasAuthority())
    {
        if (ABaseCharacter* Char = Cast<ABaseCharacter>(Actor))
            if (ABasePlayerController* PC = Cast<ABasePlayerController>(Char->GetController()))
            {
                ABattleManager** BM = BattleManagers.Find(PC->GetPendingLevelName());
                if (BM) return *BM;
            }

        for (auto& Pair : BattleManagers)
            if (Pair.Value && Pair.Value->GetLevel() == Actor->GetLevel())
                return Pair.Value;

        return nullptr;
    }

    //// 클라: 복제된 BM 액터를 PendingLevelName으로 찾기
    //FName TargetLevelName = NAME_None;
    //if (ABaseCharacter* Char = Cast<ABaseCharacter>(Actor))
    //    if (ABasePlayerController* PC = Cast<ABasePlayerController>(Char->GetController()))
    //        TargetLevelName = PC->GetPendingLevelName();

    //TArray<AActor*> BMActors;
    //UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABattleManager::StaticClass(), BMActors);
    //for (AActor* BMActor : BMActors)
    //{
    //    ABattleManager* BM = Cast<ABattleManager>(BMActor);
    //    if (BM && BM->DungeonLevelName == TargetLevelName)
    //        return BM;
    //}
    TArray<AActor*> BMActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABattleManager::StaticClass(), BMActors);

    // 클라: 이 액터가 등록된 BM을 선택
    for (AActor* BMActor : BMActors)
    {
        ABattleManager* BM = Cast<ABattleManager>(BMActor);
        if (!BM) continue;
        // 플레이어면 RegisteredPlayerControllers, 몬스터면 SpawnedMonsters 로 자기 전투 BM 식별
        if (BM->GetRegisteredPlayerControllers().Contains(Cast<ABaseCharacter>(Actor)->GetController())
            || BM->GetSpawnedMonsters().Contains(Cast<AMonsterCharacter>(Actor)))
            return BM;
    }

    return nullptr;
}


void ALevelStreamingManager::LoadLevelForPlayers(FName LevelName, const TArray<ABasePlayerController*>& Players)
{
    if (!HasAuthority()) return;
    for (ABasePlayerController* PC : Players)
    {
        if (PC)
        {
            PC->SetPendingLevelName(LevelName);
            PC->Client_LoadStreamLevel(LevelName);
        }
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 05 ClientLoadRequest: Level=%s PC=%s"), *LevelName.ToString(), PC ? *PC->GetName() : TEXT("None"));
    }
}

void ALevelStreamingManager::UnLoadLevelForPlayers(FName LevelName, const TArray<ABasePlayerController*>& Players)
{
    if (!HasAuthority()) return;
    for (ABasePlayerController* PC : Players)
    {
        if (PC)
        {
            PC->SetPendingLevelName(NAME_None);
            PC->Client_UnLoadStreamLevel(LevelName);
        }
    }
}

void ALevelStreamingManager::BeginDungeonUnload(FName LevelName, const TArray<ABasePlayerController*>& Players)
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 31 BeginDungeonUnload: Level=%s Players=%d"), *LevelName.ToString(), Players.Num());

    // 언로드 완료 배리어 준비 (NotifyPlayerUnloaded 가 이 인원 기준으로 전원 완료를 친다)
    UnloadingLevels.Add(LevelName);
    PendingUnloadPlayers.Add(LevelName, Players);
    UnloadCompletedPlayers.Remove(LevelName);

    // BM 은 레벨 언로드와 함께 파괴된다. 재사용 후보에서 미리 제외해 stale 참조를 막는다.
    // (전투 정리 자체는 EndBattle→ResetBattle 에서 이미 수행됨)
    if (ABattleManager** BM = BattleManagers.Find(LevelName))
    {
        UnregisterBattleManager(LevelName, *BM);
    }

    // 서버 월드 언로드 + 각 클라 언로드(클라는 OnLevelUnloaded 핸드셰이크로 완료 보고)
    FLatentActionInfo LatentInfo;
    UGameplayStatics::UnloadStreamLevel(this, LevelName, LatentInfo, false);
    UnLoadLevelForPlayers(LevelName, Players);
}

void ALevelStreamingManager::SaveOverworldTransform(ABasePlayerController* Player, FTransform Transform)
{
    if (Player)
        OverworldTransforms.Add(Player, Transform);
}

void ALevelStreamingManager::ReturnToOverworld(ABasePlayerController* Player)
{
    if (!Player) return;
    FTransform* SavedTransform = OverworldTransforms.Find(Player);
    if (!SavedTransform) return;
    if (APawn* Pawn = Player->GetPawn())
    {
        StopLevelStreamingPawnMovement(Pawn);
        Pawn->SetActorTransform(*SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);
        StopLevelStreamingPawnMovement(Pawn);
        Player->SetControlRotation(SavedTransform->GetRotation().Rotator());
        Player->Client_ReturnToOverworld(SavedTransform->GetLocation(), SavedTransform->GetRotation().Rotator());
    }
}

void ALevelStreamingManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 00 LSM BeginPlay: %s"), *GetName());
}

void ALevelStreamingManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}
