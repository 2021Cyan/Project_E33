#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../CWS/CombatTypes.h"
#include "../../CYW/Core/BattleSpawnSettings.h"
#include "BattleManager.generated.h"

class UTurnManager;
class ABaseCharacter;
class ABasePlayerController;
class APlayerCharacter;
class AMonsterCharacter;
class ALevelStreamingManager;
class ADungeonSpawnPoint;
class ABattleCinematicManager;

UENUM(BlueprintType)
enum class EBattleActionEventType : uint8
{
	Attack UMETA(DisplayName = "Attack"),
	Counter UMETA(DisplayName = "Counter"),
	GroupCounter UMETA(DisplayName = "GroupCounter")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnBattleActionExecuted,
	ABaseCharacter*, Attacker,
	ABaseCharacter*, Target,
	EBattleActionEventType, ActionType
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnDamageDealt,
	ABaseCharacter*, Target,
	float, Damage
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnParried,
	ABaseCharacter*, Target
);

// 클라에서 BattlePlayerCharacters 복제가 갱신될 때(특히 Pawn 레퍼런스가 뒤늦게 resolve될 때)
// 브로드캐스트. HUD가 여기 바인딩해 PlayerStatSlot을 재생성하면 배열/Pawn 복제 순서 race를 흡수한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattlePlayersReplicated);

UENUM(BlueprintType)
enum class EBattleState : uint8
{
	Idle,
	InBattle,
	End,
};

enum class ESkillTargetType : uint8;

UCLASS()
class TEAMPROJECT_API ABattleManager : public AActor
{
	GENERATED_BODY()

public:
	ABattleManager();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void StartBattle();
	void EndBattle(bool bPlayersWon = false);
	void StartTurn();
	void OnTurnEnd();

	// ===== 레벨 재사용 =====
	// 전투 종료 후 BM을 Idle로 되돌려 같은 레벨에서 다음 전투를 다시 열 수 있게 한다.
	// 스폰 몹 파괴 + 리스트/세션/타이머 정리 포함.
	void ResetBattle();

	// 이미 로드된 레벨을 다른 인카운터에 재사용할 때, LSM이 새 전투 데이터를 BM에 직접 주입.
	// (레벨 재사용 시 BeginPlay가 다시 돌지 않으므로 필요)
	void ReinitForReuse(const TArray<ABasePlayerController*>& InPlayers, const FBattleMonsterSpawnSettings& InMonsterSpawnSettings, AMonsterCharacter* InEncounterSourceMonster);

	// 새 전투를 받을 수 있는 상태인지 (진행 중/종료 처리 중이면 false)
	bool IsReadyForNewBattle() const { return BattleState == EBattleState::Idle && !bEndBattlePending; }

	// ===== 공격 세션 =====
	// [수정] 스킬의 요구 패링 횟수를 전달받도록 파라미터 추가
	void BeginAttackSession(const TArray<APlayerCharacter*>& Targets, ESkillTargetType AttackType, int32 RequiredParries);

	/*void NotifyParryFailed(APlayerCharacter* Target);*/

	// [신규] 패링 성공 시 호출되어 카운트를 올리는 함수
	void NotifyParrySuccess(APlayerCharacter* Target);

	// 마지막 타격 시점(ResolveParry 노티파이)에 호출 → 전원 패링 성공 시 반격 즉시 발동
	bool ResolveCounter();
	void EndAttackSession();
	void TriggerCounterAttack(ABaseCharacter* TargetMonster);
	void NotifyCounterFinished(APlayerCharacter* Player);
	// ==
	void RegisterPlayer(ABasePlayerController* PlayerController);
	void UnregisterPlayer(ABasePlayerController* PlayerController);
	void RegisterMonster(AMonsterCharacter* Monster);
	void UnregisterMonster(AMonsterCharacter* Monster);

	bool CheckParryResult(ABaseCharacter* Target);

	UFUNCTION(Server, Reliable)
	void Server_OnTurnEnd();

	UFUNCTION(Server, Reliable)
	void Server_RegisterPlayer(ABasePlayerController* PlayerController);

	UFUNCTION(Server, Reliable)
	void Server_UnregisterPlayer(ABasePlayerController* PlayerController);

	UFUNCTION(Server, Reliable)
	void Server_RegisterMonster(AMonsterCharacter* Monster);

	void NotifyCharacterDead(ABaseCharacter* DeadCharacter);

	void SendSpawnPoint(ABasePlayerController* PC);

	void CacheSpawnPoints();
	void SetMonsterSpawnSettings(const FBattleMonsterSpawnSettings& InMonsterSpawnSettings);
	void SetEncounterSourceMonster(AMonsterCharacter* InEncounterSourceMonster);
	void NotifyBattleStartMontageFinished(ABasePlayerController* PlayerController);

	UFUNCTION()
	void OnRep_BattleState();

	UFUNCTION()
	void OnRep_BattlePlayerCharacters();

	UFUNCTION()
	void OnRep_CurrentTurnCharacter();

	void SpawnBattleMonsters();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	TArray<AMonsterCharacter*> GetSpawnedMonsters() const { return SpawnedMonsters; }

	UFUNCTION(BlueprintCallable, Category = "Battle")
	TArray<ABasePlayerController*> GetRegisteredPlayerControllers() const { return RegisteredPlayerControllers; }

	UFUNCTION(BlueprintCallable, Category = "Battle")
	TArray<APlayerCharacter*> GetBattlePlayerCharacters() const;

	UFUNCTION(BlueprintCallable, Category = "Battle")
	TArray<APlayerCharacter*> GetRegisteredPlayerCharacters() const;

	UFUNCTION(BlueprintCallable, Category = "Battle")
	UTurnManager* GetTurnManager() const { return TurnManager; }

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	FName DungeonLevelName;

	UPROPERTY(EditInstanceOnly, Category = "Battle Camera")
	TObjectPtr<ABattleCinematicManager> BattleCinematicManager;

	UFUNCTION()
	void ApplyCameraToPlayers(float BlendTime);

	void ShowAttackQTEToPlayers(float Duration, float SuccessStartTime, float SuccessEndTime, FVector2D ScreenPosition);
	void NotifyQTEResultToAllPlayers(bool bSuccess);
	void CancelQTETimer();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	ABaseCharacter* GetCurrentTurnCharacter() const { return CurrentTurnCharacter; }

	UPROPERTY(BlueprintAssignable, Category = "Battle|Event")
	FOnBattleActionExecuted OnBattleActionExecuted;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnBattleActionExecuted(
		ABaseCharacter* Attacker,
		ABaseCharacter* Target,
		EBattleActionEventType ActionType
	);

	UPROPERTY(BlueprintAssignable, Category = "Battle|Event")
	FOnDamageDealt OnDamageDealt;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDamageDealt(
		ABaseCharacter* Target,
		float Damage
	);

	UPROPERTY(BlueprintAssignable, Category = "Battle|Event")
	FOnParried OnParried;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnParried(ABaseCharacter* Target);

	// 클라에서 BattlePlayerCharacters 복제/Pawn resolve 시 발생. HUD가 슬롯 재생성에 바인딩한다.
	UPROPERTY(BlueprintAssignable, Category = "Battle|Event")
	FOnBattlePlayersReplicated OnBattlePlayersReplicated;

private:
	void CheckBattleResult();
	void InitializeTurnManager(const TArray<ABaseCharacter*>& Participants);
	void InitializeParticipantsForBattle(const TArray<ABaseCharacter*>& Participants);
	void StartBattleCinematicOrFirstTurn();
	void RefreshBattlePlayerCharactersFromControllers();
	void AssignBattleManagerToParticipants(const TArray<ABaseCharacter*>& Participants);
	void NotifyPlayersEnterBattle();
	void NotifyPlayerEnterBattle(APlayerCharacter* PlayerChar);
	void PreparePlayerForBattleStart(APlayerCharacter* PlayerChar);
	void PlayBattleStartMontageOrMarkReady(APlayerCharacter* PlayerChar);
	void ResetBattleStartReadiness();
	void NotifyBattleStartCinematicFinished();
	void TryStartFirstTurn();
	void MarkBattleStartMontageFinished(ABasePlayerController* PlayerController);
	void StartCurrentTurnCharacter();
	void StartPlayerTurn(APlayerCharacter* Player);
	void StartMonsterTurn(AMonsterCharacter* Monster);
	void CleanupAttackSession();
	void FinishAttackSessionAndAdvanceTurn();

	UPROPERTY()
	TArray<ADungeonSpawnPoint*> SpawnPoints;

	UPROPERTY()
	TArray<ADungeonSpawnPoint*> EnemySpawnPoints;

	TArray<ABaseCharacter*> BuildParticipants();

	UPROPERTY(ReplicatedUsing = OnRep_BattleState)
	EBattleState BattleState;

	UPROPERTY(Replicated)
	TArray<ABasePlayerController*> RegisteredPlayerControllers;

	// 클라에 복제되는 플레이어 Pawn 배열. PlayerController는 owning client에만 복제되므로,
	// 클라/서버 모두 전원을 보게 하려면 Pawn 배열을 복제해야 한다. StartBattle에서 갱신된다.
	UPROPERTY(ReplicatedUsing = OnRep_BattlePlayerCharacters)
	TArray<TObjectPtr<APlayerCharacter>> BattlePlayerCharacters;

	UPROPERTY(Replicated)
	TArray<AMonsterCharacter*> SpawnedMonsters;

	UPROPERTY()
	FBattleMonsterSpawnSettings MonsterSpawnSettings;

	UPROPERTY()
	TObjectPtr<AMonsterCharacter> EncounterSourceMonster;

	UPROPERTY()
	UTurnManager* TurnManager;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentTurnCharacter)
	ABaseCharacter* CurrentTurnCharacter;

	UPROPERTY(Replicated)
	bool bBattleStartMontagesFinished = false;

	UPROPERTY(Replicated)
	bool bBattleStartCinematicFinished = false;

	UPROPERTY(Replicated)
	bool bFirstTurnStarted = false;

	UPROPERTY()
	TSet<ABasePlayerController*> BattleStartMontageFinishedPlayers;

	// ===== 공격 세션 상태 (서버 전용, 매 공격 시작 시 초기화) =====
	UPROPERTY()
	TArray<APlayerCharacter*> SessionTargets;     // 이번 공격이 노린 타겟

	//UPROPERTY() SessionParryCounts가 대체
	//TArray<APlayerCharacter*> ParryFailedTargets; // 패링 실패한 타겟 (비면 전원 패링 성공)

	// [신규] 이번 공격에서 요구되는 패링 성공 횟수 (총 타격 수)
	int32 SessionRequiredParryCount = 1;

	// [신규] 플레이어별 패링 성공 횟수 누적 맵
	UPROPERTY()
	TMap<APlayerCharacter*, int32> SessionParryCounts;

	ESkillTargetType SessionAttackType = ESkillTargetType::SingleEnemy;

	// 이번 세션의 공격 주체 (BeginAttackSession에서 확정). 반격 후 턴 종료 중복 판정에 사용
	UPROPERTY()
	ABaseCharacter* SessionAttacker = nullptr;

	// 반격이 이미 처리됐는지 (ResolveCounter 중복 실행 방지)
	bool bCounterResolved = false;

	TSet<APlayerCharacter*> PendingCounterPlayers;
	bool bAttackSessionEnded = false;

	FTimerHandle QTETimerHandle;

	// === 전투 종료 지연 처리 ===
	// 죽음/노티파이 콜스택 한복판에서 즉시 종료(파괴·언로드)하면 재진입 크래시 위험.
	// 광역기로 한 프레임에 여러 마리가 죽어도 종료는 한 번만 예약되게 가드.
	bool bEndBattlePending = false;
	FTimerHandle EndBattleTimerHandle;


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 전투 종료까지 대기 시간(죽는 연출 재생 + 콜스택 정리용). 디자이너 조정 가능.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle")
	float BattleEndDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Monster|Fallback")
	TArray<TSubclassOf<AMonsterCharacter>> MonsterClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Monster|Fallback")
	int32 MinMonsterCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Monster|Fallback")
	int32 MaxMonsterCount = 3;

public:
	virtual void Tick(float DeltaTime) override;

	void OnQTETimeout();
};
