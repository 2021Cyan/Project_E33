#include "MonsterCharacter.h"
#include "../Core/BattleManager.h"
#include "Components/SphereComponent.h"
#include "PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "../Core/LevelStreamingManager.h"
#include "../Player/BasePlayerController.h"
#include "GameFramework/PlayerState.h"
#include "../../CWS/CombatComponent.h"
#include "../../CYW/Component/MonsterBattleSpawnComponent.h"
#include "../../CWS/Player/MonsterAIController.h"
#include "Net/UnrealNetwork.h"

#define ENCOUNTER_LOG(Message) \
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] %s"), Message)

AMonsterCharacter::AMonsterCharacter()
{
    EncounterSphere = CreateDefaultSubobject<USphereComponent>(TEXT("EncounterSphere"));
    EncounterSphere->SetupAttachment(RootComponent);
    EncounterSphere->SetSphereRadius(150.0f);
    EncounterSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    EncounterSphere->SetCollisionObjectType(ECC_WorldDynamic);
    EncounterSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    EncounterSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    BattleSpawnComponent = CreateDefaultSubobject<UMonsterBattleSpawnComponent>(TEXT("BattleSpawnComponent"));

    // 몬스터의 기본 AI 컨트롤러를 우리가 만든 클래스로 교체
    AIControllerClass = AMonsterAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AMonsterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMonsterCharacter, bHasEnteredBattle);
    DOREPLIFETIME(AMonsterCharacter, bIsBossEncounter);
}

void AMonsterCharacter::BeginPlay() {
    Super::BeginPlay();
    EncounterSphere->OnComponentBeginOverlap.AddUniqueDynamic(
        this,
        &AMonsterCharacter::OnEncounterBeginOverlap
    );

    // 전투 진입 이벤트가 터지면, 오버월드 AI를 끄는 함수를 실행!
    if (HasAuthority())
    {
        OnEnemyEnterBattle.AddUniqueDynamic(this, &AMonsterCharacter::DisableOverworldAI);
    }

    if (bHasEnteredBattle)
    {
        TryBroadcastEnemyEnterBattle();
    }
}

void AMonsterCharacter::EnterBattle()
{
    if (!HasAuthority() || bHasEnteredBattle)
    {
        return;
    }

    bHasEnteredBattle = true;

    // 전투에 들어간 몹은 다시 인카운터를 일으키면 안 된다.
    // 전투 중 다가와 공격할 때 EncounterSphere가 플레이어와 오버랩되면
    // 가짜 인카운터(기본 DungeonLevelName)로 SwitchDungeon이 터져 던전이 갈아치워진다.
    bEncounterTriggered = true;
    if (EncounterSphere)
    {
        EncounterSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 전투 몬스터는 모든 참여자에게 거리 무관 항상 복제되어야 한다.
    // (기본 NetCull 거리 밖이면 원격 참여자에게 보스가 relevant하지 않아,
    //  BM의 복제 배열 SpawnedMonsters가 unmapped 참조로 보류되어 클라에서 0이 된다.)
    bAlwaysRelevant = true;
    SetNetDormancy(DORM_Awake);
    ForceNetUpdate();

    TryBroadcastEnemyEnterBattle();
}

void AMonsterCharacter::OnRep_HasEnteredBattle()
{
    if (bHasEnteredBattle)
    {
        TryBroadcastEnemyEnterBattle();
    }
}

void AMonsterCharacter::TryBroadcastEnemyEnterBattle()
{
    if (bEnemyEnterBattleBroadcasted || !bHasEnteredBattle || !HasActorBegunPlay())
    {
        return;
    }

    bEnemyEnterBattleBroadcasted = true;
    BroadcastEnemyEnterBattle();
}

void AMonsterCharacter::BroadcastEnemyEnterBattle()
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] EnemyEnterBattle Broadcast: Monster=%s Role=%d"),
        *GetName(),
        static_cast<int32>(GetLocalRole()));

    OnEnemyEnterBattle.Broadcast();
}

void AMonsterCharacter::DisableOverworldAI()
{
    AMonsterAIController* AICon = Cast<AMonsterAIController>(GetController());
    if (AICon)
    {
        AICon->SetOverworldAIEnabled(false);
    }
}

void AMonsterCharacter::OnEncounterBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bEncounterEnabled)
    {
        return;
    }

    if (bEncounterTriggered) {
        return;
    }

    APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);

    if (!Player) {
        return;
    }

    if (!HasAuthority())
    {
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 01 EncounterOverlap: Monster=%s TriggerPlayer=%s"), *GetName(), *Player->GetName());
    TryStartEncounter(Player);
}

void AMonsterCharacter::TryStartEncounter(APlayerCharacter* TriggerPlayer)
{
    if (!HasAuthority())
    {
        ENCOUNTER_LOG(TEXT("02-Blocked: NoAuthority"));
        return;
    }

    if (!bEncounterEnabled)
    {
        ENCOUNTER_LOG(TEXT("02-Blocked: EncounterDisabled"));
        return;
    }

    if (bEncounterTriggered)
    {
        ENCOUNTER_LOG(TEXT("02-Blocked: AlreadyTriggered"));
        return;
    }

    if (!TriggerPlayer)
    {
        ENCOUNTER_LOG(TEXT("01-Blocked: TriggerPlayerNull"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 02 TryStartEncounter: Monster=%s Level=%s Radius=%.0f"), *GetName(), *DungeonLevelName.ToString(), EncounterJoinRadius);

    TArray<ABasePlayerController*> DungeonPlayers;

    TArray<AActor*> PlayerActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerCharacter::StaticClass(), PlayerActors);

    const FVector MonsterLocation = GetActorLocation();
    const float JoinRadiusSq = EncounterJoinRadius * EncounterJoinRadius;

    for (AActor* Actor : PlayerActors)
    {
        APlayerCharacter* NearbyPlayer = Cast<APlayerCharacter>(Actor);
        if (!NearbyPlayer)
        {
            continue;
        }

        const float DistSq = FVector::DistSquared(MonsterLocation, NearbyPlayer->GetActorLocation());
        if (DistSq > JoinRadiusSq)
        {
            continue;
        }

        ABasePlayerController* PC = Cast<ABasePlayerController>(NearbyPlayer->GetController());
        if (PC)
        {
            DungeonPlayers.AddUnique(PC);
            UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 02 AddDungeonPlayer: PC=%s Pawn=%s Distance=%.0f"), *PC->GetName(), *NearbyPlayer->GetName(), FMath::Sqrt(DistSq));
        }
    }

    if (DungeonPlayers.IsEmpty())
    {
        ENCOUNTER_LOG(TEXT("02-Blocked: DungeonPlayersEmpty"));
        return;
    }

    DungeonPlayers.Sort([](const ABasePlayerController& A, const ABasePlayerController& B)
        {
            const APlayerState* StateA = A.PlayerState;
            const APlayerState* StateB = B.PlayerState;

            const int32 PlayerIdA = StateA ? StateA->GetPlayerId() : MAX_int32;
            const int32 PlayerIdB = StateB ? StateB->GetPlayerId() : MAX_int32;

            return PlayerIdA < PlayerIdB;
        });

    ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass()));
    if (!LSM)
    {
        ENCOUNTER_LOG(TEXT("02-Blocked: LSMNull"));
        return;
    }
    EnterBattle();
    bEncounterTriggered = true;

    FBattleMonsterSpawnSettings SpawnSettings;
    if (BattleSpawnComponent)
    {
        SpawnSettings = BattleSpawnComponent->SpawnSettings;
    }

    if (!SpawnSettings.HasValidMonsterClass())
    {
        SpawnSettings.MonsterClasses.Add(GetClass());
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 03 RequestEnterDungeon: Level=%s Players=%d"), *DungeonLevelName.ToString(), DungeonPlayers.Num());
    LSM->Server_RequestEnterDungeon(DungeonPlayers, DungeonLevelName, SpawnSettings, this);
}

void AMonsterCharacter::StartAITurn()
{
    if (!HasAuthority()) return;

    UCombatComponent* Combat = FindComponentByClass<UCombatComponent>();
    if (!Combat) { EndAITurn(); return; }

    Combat->SetCombatState(ECombatState::Idle);

    TArray<ABaseCharacter*> Enemies = Combat->GetAliveEnemyTargets();
    if (Enemies.Num() == 0) { EndAITurn(); return; }

    OnEnemyTurnStart.Broadcast();

    const FName SkillRowName = GetRandomSkillRowName();
    if (SkillRowName == NAME_None)
    {
        EndAITurn();
        return;
    }

    FSkillData* SkillData = Combat->SkillDataTable ? Combat->SkillDataTable->FindRow<FSkillData>(SkillRowName, TEXT("Monster StartAITurn")) : nullptr;
    if (!SkillData)
    {
        EndAITurn();
        return;
    }
    const int32 TargetIndex = FMath::RandRange(0, Enemies.Num() - 1);
    Combat->SelectTarget(Enemies[TargetIndex]);

    Combat->ExecuteAction(SkillRowName);
}

void AMonsterCharacter::EndAITurn()
{
	ABattleManager* BattleManager = GetBattleManager();
    if (BattleManager)
    {
        OnEnemyTurnEnd.Broadcast();
        BattleManager->Server_OnTurnEnd();
    }
}

FName AMonsterCharacter::GetRandomSkillRowName() const
{
    if (MonsterSkillRowNames.IsEmpty())
    {
        return TEXT("TestAttack");
    }

    return MonsterSkillRowNames[FMath::RandRange(0, MonsterSkillRowNames.Num() - 1)];
}
