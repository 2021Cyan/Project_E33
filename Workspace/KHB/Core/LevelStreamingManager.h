#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../CYW/Core/BattleSpawnSettings.h"
#include "LevelStreamingManager.generated.h"

class ABasePlayerController;
class ABattleManager;
class AMonsterCharacter;

UCLASS()
class TEAMPROJECT_API ALevelStreamingManager : public AActor
{
    GENERATED_BODY()

public:
    ALevelStreamingManager();

    TArray<ABasePlayerController*> GetDungeonPlayers(FName LevelName);
    void SaveOverworldTransform(ABasePlayerController* Player, FTransform Transform);
    void ReturnToOverworld(ABasePlayerController* Player);
    void RegisterBattleManager(FName LevelName, ABattleManager* BattleManager);
    void UnregisterBattleManager(FName LevelName, ABattleManager* BattleManager);
    void NotifyPlayerLoaded(ABasePlayerController* Player, FName LevelName);
    void NotifyPlayerSpawnComplete(ABasePlayerController* Player, FName LevelName);
    void NotifyPlayerUnloaded(ABasePlayerController* Player, FName LevelName);
    FBattleMonsterSpawnSettings GetBattleMonsterSpawnSettings(FName LevelName) const;
    AMonsterCharacter* GetPendingEncounterMonster(FName LevelName) const;

    UFUNCTION(Server, Reliable)
    void Server_RequestEnterDungeon(
        const TArray<ABasePlayerController*>& Players,
        FName LevelName,
        const FBattleMonsterSpawnSettings& MonsterSpawnSettings,
        AMonsterCharacter* EncounterMonster
    );

    UFUNCTION(Server, Reliable)
    void Server_RequestExitDungeon(FName LevelName);

    ABattleManager* GetBattleManagerForActor(AActor* Actor);

private:
    void LoadLevelForPlayers(FName LevelName, const TArray<ABasePlayerController*>& Players);
    void UnLoadLevelForPlayers(FName LevelName, const TArray<ABasePlayerController*>& Players);
    void BeginDungeonUnload(FName LevelName, const TArray<ABasePlayerController*>& Players);
    void TrySendSpawnPointsForLoadedPlayers(FName LevelName);
    void CancelPendingDungeonUnload(FName LevelName);
    void ScheduleDungeonUnload(FName LevelName, const TArray<ABasePlayerController*>& Players);
    void ExecutePendingDungeonUnload(FName LevelName);

    TMap<FName, TArray<ABasePlayerController*>> PendingPlayers;
    TMap<FName, TArray<ABasePlayerController*>> ActiveDungeons;
    TMap<FName, TArray<ABasePlayerController*>> LoadedPlayers;
    TMap<FName, TArray<ABasePlayerController*>> SpawnPointSentPlayers;
    TMap<FName, TArray<ABasePlayerController*>> SpawnCompletedPlayers;
    TMap<FName, TArray<ABasePlayerController*>> UnloadCompletedPlayers;
    TMap<FName, ABattleManager*> BattleManagers;
    TMap<FName, FBattleMonsterSpawnSettings> PendingMonsterSpawnSettings;
    TMap<FName, TObjectPtr<AMonsterCharacter>> PendingEncounterMonsters;
    TMap<FName, TArray<ABasePlayerController*>> PendingUnloadPlayers;
    TMap<FName, FTimerHandle> PendingUnloadTimers;
    TSet<FName> UnloadingLevels;

    // [멀티 던전] 현재 로드되어 상주 중인 던전. 다른 던전 진입 시 이 던전을 먼저 언로드한다.
    FName LastLoadedDungeon;

    // 다른 던전 언로드 완료 후 재개할 보류된 진입 요청 (단일 파티 기준 1건)
    bool bHasPendingEnter = false;
    TArray<ABasePlayerController*> PendingEnterPlayers;
    FName PendingEnterLevel;
    FBattleMonsterSpawnSettings PendingEnterSpawnSettings;
    TObjectPtr<AMonsterCharacter> PendingEnterMonster;
    TMap<ABasePlayerController*, FTransform> OverworldTransforms;

    UPROPERTY(EditAnywhere, Category = "Dungeon")
    float DungeonUnloadDelay = 5.0f;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
};
