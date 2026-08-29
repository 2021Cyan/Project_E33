#include "CombatComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "../KHB/Component/StatComponent.h"
#include "../KHB/Character/BaseCharacter.h"
#include "../KHB/Core/BattleManager.h"
#include "../KHB/Character/MonsterCharacter.h"
#include "../KHB/Core/TurnManager.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "../KHB/Character/PlayerCharacter.h" 
#include "NiagaraComponent.h" 
#include "../KHB/Character/PlayerCharacter.h"
#include "Animation/AnimInstance.h"
#include "../CYW/Camera/BattleCinematicManager.h"

UCombatComponent::UCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    CurrentSkillData = nullptr;
    CurrentTarget = nullptr;
    CurrentState = ECombatState::Idle;
}

void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCombatComponent, CurrentState);
    DOREPLIFETIME(UCombatComponent, CurrentActionID);
    DOREPLIFETIME(UCombatComponent, OriginalLocation);
    DOREPLIFETIME(UCombatComponent, OriginalRotation);
}


// [Action] 입력 처리 및 서버 요청
void UCombatComponent::SelectTarget(AActor* TargetActor)
{
    // 내가 턴이 아니면 타겟팅 불가! 
    if (CurrentState != ECombatState::Idle) return;

    CurrentTarget = TargetActor;
    OnTargetChanged.Broadcast(CurrentTarget);
    UE_LOG(LogTemp, Warning, TEXT("[%s]를 타겟으로 지정했습니다!"), *TargetActor->GetName());
}

void UCombatComponent::ExecuteAction(FName ActionID)
{
    
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

	// 현재 상태가 공격, 패링, 피격 중이면 입력 무시
    if (CurrentState == ECombatState::Countering || CurrentState == ECombatState::Attacking || CurrentState == ECombatState::Hit)
    {
        UE_LOG(LogTemp, Warning, TEXT("현재 액션 중입니다! (상태: %d) 입력 무시."), (int32)CurrentState);
        return; 
    }

    if (CurrentState != ECombatState::Idle) {
        UE_LOG(LogTemp, Warning, TEXT("현재 상태 %d"), static_cast<int32>(CurrentState));
        return;
    }

    const bool bMyTurn = OwnerChar->IsMyTurn();

    // 패링: 내 턴이 아닐 때만 (몬스터 공격 타이밍 대응)
    if (ActionID == "TestParrying" || ActionID == "Parry")
    {
        if (bMyTurn)
        {
            UE_LOG(LogTemp, Warning, TEXT("[%s] 패링 무시: 내 턴엔 불가"), *OwnerChar->GetName());
            return;
        }
        Server_ExecuteAction(ECombatState::Parrying, ActionID, nullptr);
        return;
    }

    // 공격: 내 턴만
    if (!bMyTurn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] 공격 무시: 내 턴 아님"), *OwnerChar->GetName());
        return;
    }
    if (!CurrentTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("타겟을 먼저 선택해주세요!"));
        return;
    }
    Server_ExecuteAction(ECombatState::Attacking, ActionID, CurrentTarget);
}

void UCombatComponent::Server_ExecuteAction_Implementation(ECombatState IntendedState, FName ActionID, AActor* TargetActor)
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());

    if (CurrentState != ECombatState::Idle)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Server] 행동 거부: 현재 상태가 Idle 아님 Actor=%s CurrentState=%d IntendedState=%d"),
            *OwnerChar->GetName(),
            static_cast<int32>(CurrentState),
            static_cast<int32>(IntendedState));

        return;
    }

    // 공격은 내 턴에만, 패링은 내 턴이 아닐 때만 (서버 권위 검증)
    if (IntendedState == ECombatState::Attacking && !OwnerChar->IsMyTurn())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Server] 공격 거부: 턴 아님 Actor=%s"), *OwnerChar->GetName());
        return;
    }
    if (IntendedState == ECombatState::Parrying && OwnerChar->IsMyTurn())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Server] 패링 거부: 내 턴 Actor=%s"), *OwnerChar->GetName());
        return;
    }

    if (IntendedState == ECombatState::Attacking)
    {
        CurrentSkillData = SkillDataTable->FindRow<FSkillData>(ActionID, TEXT("Server_ExecuteAction"));

        UStatComponent* Stat = OwnerChar->FindComponentByClass<UStatComponent>();
        if (!CurrentSkillData || !Stat) return;

        if (Stat->GetAP() < CurrentSkillData->APCost)
        {
            UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] ActionBlocked: NotEnoughAP Actor=%s Action=%s AP=%.0f Cost=%d"),
                *OwnerChar->GetName(),
                *ActionID.ToString(),
                Stat->GetAP(),
                CurrentSkillData->APCost);
            return;
        }

        Stat->Server_SetAP(Stat->GetAP() - CurrentSkillData->APCost);
    }

    CurrentTarget = TargetActor;
    CurrentActionID = ActionID;
    CurrentState = IntendedState;

    if (IntendedState == ECombatState::Attacking)
    {
		// 방어자가 공격자를 바라보도록 지시함 (멀티캐스트)
        TArray<AActor*> TargetsToLookAtMe;
        if (CurrentSkillData && CurrentSkillData->TargetType == ESkillTargetType::SingleEnemy && CurrentTarget)
        {
            TargetsToLookAtMe.Add(CurrentTarget);
        }
        else if (CurrentSkillData && CurrentSkillData->TargetType == ESkillTargetType::AllEnemies)
        {
            for (ABaseCharacter* Enemy : GetAliveEnemyTargets()) TargetsToLookAtMe.Add(Enemy);
        }

        for (AActor* Def : TargetsToLookAtMe)
        {
            if (UCombatComponent* DefCombat = Def->FindComponentByClass<UCombatComponent>())
            {
				// 공격자(OwnerChar)를 바라보도록 지시
                DefCombat->Multicast_StartTracking(OwnerChar);
            }
        }
    }

    // 상태에 맞는 연출
    HandleStateChanged(CurrentState, CurrentActionID);

    if (IntendedState == ECombatState::Attacking)
    {
        if (ABattleManager* BM = OwnerChar->GetBattleManager())
        {
            BM->Multicast_OnBattleActionExecuted(
                OwnerChar,
                Cast<ABaseCharacter>(TargetActor),
                EBattleActionEventType::Attack
            );
        }
        StartAttackSession();
    }
}


// 상태 동기화 및 몽타주 연출
void UCombatComponent::OnRep_CurrentState()
{
    HandleStateChanged(CurrentState, CurrentActionID);
}

void UCombatComponent::SetPendingAction(FName ActionID)
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

    if (!OwnerChar->IsMyTurn()) return;
    if (CurrentState != ECombatState::Idle) return;

    PendingActionID = ActionID;
    CurrentTarget = nullptr;

    AutoSelectFirstTarget();
    UE_LOG(LogTemp, Warning, TEXT("PendingAction 설정: %s"), *PendingActionID.ToString());
}

void UCombatComponent::ResetPendingAction()
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

    if (!OwnerChar->IsMyTurn()) return;
    if (CurrentState != ECombatState::Idle) return;

    PendingActionID = NAME_None;
    CurrentTarget = nullptr;

    UE_LOG(LogTemp, Warning, TEXT("PendingAction 초기화"));
}

void UCombatComponent::ConfirmPendingAction()
{
    if (PendingActionID == NAME_None)
    {
        UE_LOG(LogTemp, Warning, TEXT("선택된 행동이 없습니다."));
        return;
    }

    ExecuteAction(PendingActionID);
    ResetPendingAction();
}

void UCombatComponent::SaveOriginalPosition()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        OriginalLocation = GetOwner()->GetActorLocation();
        OriginalRotation = GetOwner()->GetActorRotation();
    }
}

void UCombatComponent::HandleStateChanged(ECombatState NewState, FName ActionID)
{
    SetCombatState(NewState);

    switch (NewState)
    {
    case ECombatState::Attacking:
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(RotationTimerHandle);
        }

        if (SkillDataTable && ActionID != NAME_None)
        {
            CurrentSkillData = SkillDataTable->FindRow<FSkillData>(ActionID, "");
        }

        if (GetOwner() && CurrentSkillData)
        {
            FVector LookAtLocation = FVector::ZeroVector;
            bool bShouldRotate = false;

            // 광역기일 경우: 적 진형의 '중앙(평균 좌표)'을 바라봄
            if (CurrentSkillData->TargetType == ESkillTargetType::AllEnemies)
            {
                TArray<ABaseCharacter*> Enemies = GetAliveEnemyTargets();
                if (Enemies.Num() > 0)
                {
                    for (ABaseCharacter* Enemy : Enemies)
                    {
                        LookAtLocation += Enemy->GetActorLocation();
                    }
                    LookAtLocation /= Enemies.Num(); // 평균(중앙) 좌표
                    bShouldRotate = true;
                }
            }
            // 단일기일 경우: 지정된 '타겟'을 바라봄
            else if (CurrentTarget)
            {
                LookAtLocation = CurrentTarget->GetActorLocation();
                bShouldRotate = true;

                StartTrackingActor(CurrentTarget);
            }

            //  계산된 위치를 향해 캐릭터를 회전시킴
            if (bShouldRotate)
            {
                FVector Direction = LookAtLocation - GetOwner()->GetActorLocation();
                Direction.Z = 0.0f; // 기울어짐 방지

                if (!Direction.IsNearlyZero())
                {
                    GetOwner()->SetActorRotation(Direction.Rotation());
                }
            }

            // 방향 세팅이 끝났으니 공격 몽타주를 재생
            if (CurrentSkillData->ActionMontage)
            {
                PlayMontageWithSync(CurrentSkillData->ActionMontage);
            }
        }
        break;

    case ECombatState::Parrying:
        UE_LOG(LogTemp, Warning, TEXT("패링"));
        PlayMontageWithSync(ParryMontage);
        break;

    case ECombatState::Stunned:
        PlayMontageWithSync(StunMontage);
        break;

    case ECombatState::Idle:
        ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
        bool bIsMyTurn = OwnerChar && OwnerChar->IsMyTurn();
        bool bIsDead = OwnerChar && OwnerChar->IsDead();


        // (오버월드 복귀 후 OriginalLocation(던전 좌표)으로 스냅되는 것 방지)
        if (!OwnerChar || !OwnerChar->IsInBattle())
        {
            StopTrackingActor();
            ActorToTrack = nullptr;
            OriginalLocation = FVector::ZeroVector;
            break;
        }

        // 1. 공격자일 때 타겟의 트래킹 해제 명령
        if (bIsMyTurn && GetOwner() && GetOwner()->HasAuthority())
        {
            TArray<AActor*> TrackedTargets;
            if (CurrentSkillData && CurrentSkillData->TargetType == ESkillTargetType::SingleEnemy && CurrentTarget)
                TrackedTargets.Add(CurrentTarget);
            else if (CurrentSkillData && CurrentSkillData->TargetType == ESkillTargetType::AllEnemies)
                for (ABaseCharacter* Enemy : GetAliveEnemyTargets()) TrackedTargets.Add(Enemy);

            for (AActor* Def : TrackedTargets)
            {
                if (UCombatComponent* DefCombat = Def->FindComponentByClass<UCombatComponent>())
                {
                    DefCombat->Multicast_StopTracking();
                }
            }
        }

        // 2. 방어자일 때 강제 회전 방지
        if (!bIsMyTurn && ActorToTrack != nullptr)
        {
            // 죽지 않았을 때만 위치를 보정. 죽었으면 그 자리에 둠
            if (GetOwner() && OriginalLocation != FVector::ZeroVector && !bIsDead)
            {
                GetOwner()->SetActorLocation(OriginalLocation);
            }

            CurrentSkillData = nullptr;
            CurrentActionID = NAME_None;
            QTEResults.Empty();
            break;
        }

        StopTrackingActor();

        bool bNeedsDashOut = false;

        // 죽지 않았을 때(!bIsDead)만 원위치 복귀 거리를 재고 대시 아웃을 실행함
        if (GetOwner() && OriginalLocation != FVector::ZeroVector && !bIsDead)
        {
            FVector CurrentLoc = GetOwner()->GetActorLocation();

            if (FVector::DistXY(CurrentLoc, OriginalLocation) > 150.0f && DashOutMontage)
            {
                bNeedsDashOut = true;
            }
            else
            {
                GetOwner()->SetActorLocation(OriginalLocation);
                GetWorld()->GetTimerManager().SetTimer(RotationTimerHandle, this, &UCombatComponent::InterpolateRotation, 0.01f, true);
            }
        }

        // 죽었으면 bNeedsDashOut이 무조건 false이므로 몽타주가 재생되지 않음
        if (bNeedsDashOut)
        {
            PlayMontageWithSync(DashOutMontage);
            return;
        }

        CurrentSkillData = nullptr;
        CurrentTarget = nullptr;
        CurrentActionID = NAME_None;
        QTEResults.Empty();
        break;
    }
}

void UCombatComponent::Multicast_StartTracking_Implementation(AActor* TargetToTrack) { StartTrackingActor(TargetToTrack); }

void UCombatComponent::Multicast_StopTracking_Implementation() { 
    StopTrackingActor();
    if (GetOwner())
    {
        GetWorld()->GetTimerManager().SetTimer(RotationTimerHandle, this, &UCombatComponent::InterpolateRotation, 0.01f, true);
    }
}

void UCombatComponent::StartTrackingActor(AActor* TargetToTrack)
{
    if (!GetOwner() || !TargetToTrack) return;
    GetWorld()->GetTimerManager().ClearTimer(RotationTimerHandle);

    ActorToTrack = TargetToTrack;
    GetWorld()->GetTimerManager().SetTimer(TrackingTimerHandle, this, &UCombatComponent::TrackActorRoutine, 0.01f, true);
}

void UCombatComponent::StopTrackingActor()
{
    ActorToTrack = nullptr;
    GetWorld()->GetTimerManager().ClearTimer(TrackingTimerHandle);
}

void UCombatComponent::StopBattleTimers()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RotationTimerHandle);
        World->GetTimerManager().ClearTimer(TrackingTimerHandle);
    }
    ActorToTrack = nullptr;
}

void UCombatComponent::TrackActorRoutine()
{
    if (!GetOwner() || !ActorToTrack)
    {
        StopTrackingActor();
        return;
    }

    FVector TargetLocation = ActorToTrack->GetActorLocation();
    FVector MyLocation = GetOwner()->GetActorLocation();
    TargetLocation.Z = MyLocation.Z;

    FVector DirectionToTarget = TargetLocation - MyLocation;
    if (!DirectionToTarget.IsNearlyZero())
    {
        FRotator TargetRot = FRotationMatrix::MakeFromX(DirectionToTarget).Rotator();
        FRotator CurrentRot = GetOwner()->GetActorRotation();

        FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, 0.01f, 15.0f);
        // TeleportPhysics로 강제 회전
        GetOwner()->SetActorRotation(NewRot, ETeleportType::TeleportPhysics);
    }
}

void UCombatComponent::InterpolateRotation()
{
    if (!GetOwner()) return;

    FRotator CurrentRot = GetOwner()->GetActorRotation();
    FRotator NewRot = FMath::RInterpTo(CurrentRot, OriginalRotation, 0.01f, 15.0f);

    GetOwner()->SetActorRotation(NewRot, ETeleportType::TeleportPhysics);

    if (CurrentRot.Equals(OriginalRotation, 1.0f))
    {
        GetOwner()->SetActorRotation(OriginalRotation);
        GetWorld()->GetTimerManager().ClearTimer(RotationTimerHandle);
    }
}

void UCombatComponent::StartAttackSession()
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;
    ABattleManager* BM = OwnerChar->GetBattleManager();   // ←BM 여기서 생김
    if (!BM || !CurrentSkillData) return;

    TArray<APlayerCharacter*> PlayerTargets;              // ←PlayerTargets 여기서 생김

    if (CurrentSkillData->TargetType == ESkillTargetType::SingleEnemy)
    {
        if (APlayerCharacter* P = Cast<APlayerCharacter>(CurrentTarget))
            PlayerTargets.Add(P);

        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] AttackSessionTarget: Attacker=%s Type=%s CurrentTarget=%s PlayerTargets=%d"),
            *OwnerChar->GetName(),
            CurrentSkillData->TargetType == ESkillTargetType::SingleEnemy ? TEXT("SingleEnemy") : TEXT("AllEnemies"),
            CurrentTarget ? *CurrentTarget->GetName() : TEXT("None"),
            PlayerTargets.Num());
    }
    else if (CurrentSkillData->TargetType == ESkillTargetType::AllEnemies)
    {
        for (ABaseCharacter* E : GetAliveEnemyTargets())  // ←E 여기서 생김 (루프 변수)
        {
            if (APlayerCharacter* P = Cast<APlayerCharacter>(E))
                PlayerTargets.Add(P);
        }
    }

    BM->BeginAttackSession(PlayerTargets, CurrentSkillData->TargetType, CurrentSkillData->RequiredParryCount);
}

// [Combat Logic] 데미지 판정 및 처리 (AnimNotify 트리거)
void UCombatComponent::ApplySkillEffect()
{
    UE_LOG(LogTemp, Warning, TEXT("===== 노티파이가 정상 작동했습니다! ====="));

    // 적 공격시 카메라 Shake 효과 재생
    if (GetOwner()->HasAuthority())
    {
        if (AMonsterCharacter* OwnerMonster = Cast<AMonsterCharacter>(GetOwner()))
        {
            if (ABattleManager* BM = OwnerMonster->GetBattleManager())
            {
                if (BM->BattleCinematicManager)
                {
                    BM->BattleCinematicManager->PlayEnemyAttackCameraImpact(BM->GetRegisteredPlayerControllers());
                }
            }
        }
    }


    // 이번 스킬이 때려야 할 타겟들의 배열을 만듬
    TArray<AActor*> TargetsToHit;

    if (CurrentSkillData->TargetType == ESkillTargetType::SingleEnemy && CurrentTarget)
    {
        // 단일 공격이면 기존 타겟 1명만
        TargetsToHit.Add(CurrentTarget);
    }
    else if (CurrentSkillData->TargetType == ESkillTargetType::AllEnemies)
    {
        // 광역 공격이면 살아있는 모든 적을 
        TArray<ABaseCharacter*> AliveEnemies = GetAliveEnemyTargets();
        for (ABaseCharacter* Enemy : AliveEnemies)
        {
            TargetsToHit.Add(Enemy);
            UE_LOG(LogTemp, Warning, TEXT("===== 전체인원 탐색 ====="));
        }
    }
    for (AActor* Target : TargetsToHit)
    {
        if (CurrentSkillData->ImpactEffect && Target)
        {
            FVector TargetLocation = Target->GetActorLocation();
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CurrentSkillData->ImpactEffect, TargetLocation);
        }
    }
    for (UNiagaraComponent* Projectile : FlyingProjectiles)
    {
        if (Projectile)
        {
            Projectile->DestroyComponent();
        }
    }
    FlyingProjectiles.Empty();

    if (CurrentSkillData->CameraShake)
    {
    }


    // 실제 데미지 적용. 오직 서버에서만 실행

    float FailedQTEs = 0;

    for (bool bSuccess : QTEResults)
    {
        if (bSuccess) continue;
        FailedQTEs++;
    }

    float TotalDamage = CurrentSkillData->Damage;

    for(int32 i = 0; i < FailedQTEs; i++)
    {
        TotalDamage /= 2.f;
	}

    if (GetOwner()->HasAuthority())
    {
        for (AActor* Target : TargetsToHit)
        {
            if(TargetsToHit.Num() > 1)
                UE_LOG(LogTemp, Warning, TEXT("[서버] 광역 공격! %s에게 데미지 전달"), *Target->GetName()); 

            Server_ApplyDamage(Target, TotalDamage);
        }
        if(TargetsToHit.Num() == 1)
			UE_LOG(LogTemp, Warning, TEXT("[서버] 단일 공격! %s에게 %f 데미지 전달"), *TargetsToHit[0]->GetName(), TotalDamage);
    }
}

void UCombatComponent::Server_ApplyDamage_Implementation(AActor* TargetActor, float Damage)
{
    ApplyDamageInternal(TargetActor, Damage);
}

void UCombatComponent::ApplyDamageInternal(AActor * TargetActor, float SkillDamage, int32 DamageMultiplier)
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    ABaseCharacter* TargetChar = Cast<ABaseCharacter>(TargetActor);
    if (!OwnerChar || !TargetChar)
    {
        UE_LOG(LogTemp, Warning, TEXT("상대 또는 내가 없음"));
        return;
    }

    if (TargetChar->IsDead()) return;

    ABattleManager* BM = OwnerChar->GetBattleManager();
    if (!BM)
    {
        UE_LOG(LogTemp, Warning, TEXT("배틀 메니저가 없음"));
        return;
    }

    UCombatComponent* TargetCombat = TargetActor->FindComponentByClass<UCombatComponent>();
    if (!TargetCombat) return;

    //  배틀 매니저에게 타겟이 패링을 성공했는지 여부 체크

    bool bParrySuccess = BM->CheckParryResult(TargetChar);

    if (bParrySuccess)
    {

        // 패링 성공 처리 (체력 안 깎음)

        TargetCombat->Multicast_OnReceivedHit(EParryResult::Success);

        if (APlayerCharacter* ParryPlayer = Cast<APlayerCharacter>(TargetChar))
        {
            UStatComponent* Stat = ParryPlayer->FindComponentByClass<UStatComponent>();
            if (Stat)
            {
                const float OldAP = Stat->GetAP();
                Stat->Server_SetAP(OldAP + 1.f);
                BM->Multicast_OnParried(TargetChar);
            }
            BM->NotifyParrySuccess(ParryPlayer);
        }

        UE_LOG(LogTemp, Warning, TEXT("[서버] 패링 성공! %s의 체력을 깎지 않습니다."), *TargetChar->GetName());
    }
    else
    {
        //  패링 실패 (피격 처리)
        UStatComponent* OwnerStat = OwnerChar->FindComponentByClass<UStatComponent>();
        UStatComponent* TargetStat = TargetChar->FindComponentByClass<UStatComponent>();

        if (!OwnerStat || !TargetStat)
        {
            UE_LOG(LogTemp, Warning, TEXT("[서버] 피격 처리 실패: StatComponent 없음 Attacker=%s Target=%s"),
                *OwnerChar->GetName(),
                *TargetChar->GetName());
            return;
        }

        const int32 RoundedSkillDamage = FMath::RoundToInt(SkillDamage);
        const int32 Attack = FMath::RoundToInt(OwnerStat->GetAttack());
        const int32 Defense = FMath::RoundToInt(TargetStat->GetDefense());
        const int32 Multiplier = FMath::Max(1, DamageMultiplier);
        const int32 FinalDamage = FMath::Max(1, RoundedSkillDamage + Attack - Defense) * Multiplier;

        UE_LOG(LogTemp, Warning, TEXT("[서버] 피격 발생! Attacker=%s Target=%s SkillDamage=%d Attack=%d Defense=%d Multiplier=%d FinalDamage=%d"),
            *OwnerChar->GetName(),
            *TargetChar->GetName(),
            RoundedSkillDamage,
            Attack,
            Defense,
            Multiplier,
            FinalDamage);

        float CurrentHP = TargetStat->GetHP();
        TargetStat->Server_SetHP(CurrentHP - FinalDamage); // 체력 차감
        BM->Multicast_OnDamageDealt(
            TargetChar,
            FinalDamage
        );
        TargetCombat->SetCombatState(ECombatState::Hit);
        TargetCombat->Multicast_OnReceivedHit(EParryResult::Fail);
    }
}

void UCombatComponent::Server_SetCombatState_Implementation(ECombatState NewState)
{
    CurrentState = NewState;
}

void UCombatComponent::Multicast_OnReceivedHit_Implementation(EParryResult Result)
{
    switch (Result)
    {
    case EParryResult::Fail:
    case EParryResult::None:
        PlayMontageWithSync(DefaultHitMontage);
        break;

    case EParryResult::Success:

        if (ParrySparkEffect)
        {
            FVector SpawnLocation = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 50.0f; // 내 앞쪽에 패링 이펙트 스폰
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ParrySparkEffect, SpawnLocation);
        }
        break;
    }
}

void UCombatComponent::AutoSelectFirstTarget()
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

    ABattleManager* BM = OwnerChar->GetBattleManager();
    if (!BM) return;

    if (!OwnerChar->IsMyTurn())
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] 타겟 변경 무시: 내 턴 아님"), *OwnerChar->GetName());
        return;
    }
    // 내가 때려야 할 적군 목록을 가져옴 (플레이어, 몬스터 공통)
    TArray<ABaseCharacter*> Enemies = GetAliveEnemyTargets();

    // 첫 번째를 무조건 타겟으로 지정
    if (Enemies.Num() > 0)
    {
        SelectTarget(Enemies[0]);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] 타겟팅 실패: 살아있는 적이 없습니다!"), *GetOwner()->GetName());
    }
}


// [타겟팅] 좌우(A/D)로 타겟 변경하기
void UCombatComponent::CycleTarget(bool bNext)
{

    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

    ABattleManager* BM = OwnerChar->GetBattleManager();
    if (!BM) return;

    if (!OwnerChar->IsMyTurn())
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] 타겟 변경 무시: 내 턴 아님"), *OwnerChar->GetName());
        return;
    }

    if (PendingActionID == NAME_None)
    {
        UE_LOG(LogTemp, Warning, TEXT("행동을 먼저 선택하세요."));
        return;
    }

    // 1. 피아식별된 살아있는 적군 목록 가져오기
    TArray<ABaseCharacter*> AliveEnemies = GetAliveEnemyTargets();

    if (AliveEnemies.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] 타겟 변경 무시: 살아있는 적이 없습니다!"), *GetOwner()->GetName());
        return;
    }

    // 2. 현재 타겟의 인덱스 찾기 (AActor를 ABaseCharacter로 캐스팅하여 비교)
    int32 CurrentIndex = AliveEnemies.IndexOfByKey(Cast<ABaseCharacter>(CurrentTarget));
    int32 OldIndex = CurrentIndex;

    if (CurrentIndex == INDEX_NONE) CurrentIndex = 0;

    // 3. 인덱스 이동 (순회)
    FString DirectionStr = bNext ? TEXT("오른쪽(D)") : TEXT("왼쪽(A)");
    if (bNext)
    {
        CurrentIndex = (CurrentIndex + 1) % AliveEnemies.Num();
    }
    else
    {
        CurrentIndex = (CurrentIndex - 1 + AliveEnemies.Num()) % AliveEnemies.Num();
    }

    UE_LOG(LogTemp, Log, TEXT("[%s] 타겟 순회: %s | [%d] -> [%d] (총 %d마리)"),
        *GetOwner()->GetName(), *DirectionStr, OldIndex, CurrentIndex, AliveEnemies.Num());

    // 4. 새로운 타겟 확정!
    SelectTarget(AliveEnemies[CurrentIndex]);
}

TArray<ABaseCharacter*> UCombatComponent::GetAliveEnemyTargets()
{
    TArray<ABaseCharacter*> AliveEnemies;

    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return AliveEnemies;

    ABattleManager* BM = OwnerChar->GetBattleManager();
    if (!BM)
    {
        UE_LOG(LogTemp, Error, TEXT("배틀매니저(BM)를 찾을 수 없습니다!"));
        return AliveEnemies;
    }

    // 1. 내가 플레이어라면? -> 몬스터 배열을 긁어옴
    if (OwnerChar->IsPlayerControlled())
    {
        TArray<AMonsterCharacter*> Monsters = BM->GetSpawnedMonsters();
        UE_LOG(LogTemp, Warning, TEXT("배틀매니저에 등록된 몬스터 수: %d"), Monsters.Num());

        for (AMonsterCharacter* Monster : Monsters)
        {
            // 보스 액터 자체가 클라이언트에 동기화되지 않음
            if (!Monster)
            {
                UE_LOG(LogTemp, Error, TEXT("몬스터 배열 안에 비어있는 값(Nullptr)이 있습니다! (리플리케이션 누락)"));
                continue;
            }

            UStatComponent* StatComp = Monster->FindComponentByClass<UStatComponent>();

            // StatComponent가 없거나 이름이 다름
            if (!StatComp)
            {
                UE_LOG(LogTemp, Error, TEXT("%s 에게 StatComponent가 없습니다!"), *Monster->GetName());
                continue;
            }

            // 클라이언트에서 HP가 0 이하로 판정됨
            if (StatComp->IsDead())
            {
                UE_LOG(LogTemp, Warning, TEXT(" %s 는 이미 죽은 것으로 판정됩니다. (현재 HP: %f)"), *Monster->GetName(), StatComp->GetHP());
                continue;
            }

            AliveEnemies.Add(Monster);
            UE_LOG(LogTemp, Warning, TEXT("타겟 등록 완료: %s"), *Monster->GetName());
        }
    }
    // 2. 내가 몬스터라면? -> 플레이어 배열을 긁어옵니다.
    else
    {
        for (APlayerCharacter* PlayerChar : BM->GetRegisteredPlayerCharacters())
        {
            if (PlayerChar)
            {
                UStatComponent* StatComp = PlayerChar->FindComponentByClass<UStatComponent>();
                if (StatComp && !StatComp->IsDead())
                {
                    AliveEnemies.Add(PlayerChar);
                }
            }
        }
    }

    return AliveEnemies;
}

void UCombatComponent::PerformCounterAttack(ABaseCharacter* TargetMonster, float CounterDamage, bool bGroup)
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!GetOwner()->HasAuthority() || !OwnerChar || !TargetMonster) return;

    SetCombatState(ECombatState::Countering); 

    if (ABattleManager* BM = OwnerChar->GetBattleManager())
    {
        if (bGroup) {
            BM->Multicast_OnBattleActionExecuted(
                OwnerChar,
                TargetMonster,
                EBattleActionEventType::GroupCounter
            );
        }
        else {
            BM->Multicast_OnBattleActionExecuted(
                OwnerChar,
                TargetMonster,
                EBattleActionEventType::Counter
            );
        }
    }

    FName CounterSkillID = bGroup ? GroupCounterSkillID : SingleCounterSkillID;

    Multicast_PlayCounter(CounterSkillID, TargetMonster);                // 모든 클라에 반격 몽타주
    const int32 SuccessParryCount = FMath::Max(1, FMath::RoundToInt(CounterDamage));
}

void UCombatComponent::Multicast_PlayCounter_Implementation(FName CounterSkillID, AActor* TargetMonster)
{
    SetCombatState(ECombatState::Countering);
    PendingActionID = NAME_None;

    if (SkillDataTable && CounterSkillID != NAME_None)
    {
        CurrentSkillData = SkillDataTable->FindRow<FSkillData>(CounterSkillID, TEXT("CounterAttack"));
    }

    // 내가 때릴 타겟을 설정해 둠
    CurrentTarget = TargetMonster;

    if (ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner()))
    {
        if (UAnimInstance* Anim = OwnerChar->GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Stop(0.f);
        }
    }

    // 장전된 스킬 데이터 안에 들어있는 몽타주 재생
    if (CurrentSkillData && CurrentSkillData->ActionMontage)
    {
        PlayMontageWithSync(CurrentSkillData->ActionMontage);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[Counter] 카운터 스킬 데이터 또는 몽타주가 세팅되지 않았습니다!"));
    }
}

// =======================================================================
// [Helper] 
// =======================================================================

void UCombatComponent::PlayMontageWithSync(UAnimMontage* MontageToPlay)
{
    if (!MontageToPlay) return;

    ABaseCharacter* OwnerCharacter = Cast<ABaseCharacter>(GetOwner());
    if (OwnerCharacter)
    {
        // 서버 호출 시각 기준으로 클라이언트 재생 위치 보정
        // 보정 없이 재생 시 네트워크 지연만큼 애니메이션이 늦게 시작되어 판정 타이밍과 어긋남
        // 몬스터 공격 몽타주(패링)와 같이 판정 타이밍이 서버 시간 기준으로 작동 필요시 호출하여 사용
        OwnerCharacter->Multicast_PlayMontageWithSync(MontageToPlay, GetWorld()->GetTimeSeconds());
    }
}

void UCombatComponent::SpawnSkillEffect(EEffectSpawnType EffectType)
{
    if (!CurrentSkillData) return;

    // 오직 서버만 이펙트 스폰 명령을 내림 (이중 스폰/렉 방지)
    if (!GetOwner()->HasAuthority()) return;

    TArray<AActor*> TargetsToHit;

    // Cast 이펙트는 타겟이 필요 없으므로 빈 배열로
    if (EffectType != EEffectSpawnType::HandCasting)
    {
        if (CurrentSkillData->TargetType == ESkillTargetType::SingleEnemy && CurrentTarget)
            TargetsToHit.Add(CurrentTarget);
        else if (CurrentSkillData->TargetType == ESkillTargetType::AllEnemies)
            TargetsToHit.Append(GetAliveEnemyTargets());
    }

    // 서버가 모든 클라이언트에게 "이 타입의 이펙트를 뿌려라" 명령
    Multicast_SpawnSkillEffect(EffectType, TargetsToHit);
}

void UCombatComponent::Multicast_SpawnSkillEffect_Implementation(EEffectSpawnType EffectType, const TArray<AActor*>& TargetsToHit)
{
    if (!CurrentSkillData) return;

    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

    // 1. 시전자(나) 위치에 스폰하는 이펙트 (HandCasting)
    if (EffectType == EEffectSpawnType::HandCasting)
    {
        if (CurrentSkillData->CastingEffect)
        {
            UNiagaraFunctionLibrary::SpawnSystemAttached(
                CurrentSkillData->CastingEffect, // 스폰할 이펙트
                OwnerChar->GetMesh(),            
                FName("hand_r"),                 
                FVector::ZeroVector,            
                FRotator::ZeroRotator,         
                EAttachLocation::SnapToTarget,  
                true                             // 자동 파괴
            );
        }
        return;
    }

    // 2. 타겟 위치에 스폰하는 이펙트들 (Projectile, Ground, Impact)
    for (AActor* Target : TargetsToHit)
    {
        if (!Target) continue;

        UNiagaraSystem* EffectToSpawn = nullptr;
        FVector SpawnLocation = Target->GetActorLocation();

        switch (EffectType)
        {
        case EEffectSpawnType::HandCasting: 
        case EEffectSpawnType::Casting:
            EffectToSpawn = CurrentSkillData->CastingEffect;
            SpawnLocation.Z -= Target->GetSimpleCollisionHalfHeight();
            break;

        case EEffectSpawnType::Eruption:
            EffectToSpawn = CurrentSkillData->EruptionEffect;
            SpawnLocation.Z -= Target->GetSimpleCollisionHalfHeight();
            break;

        case EEffectSpawnType::Projectile:
            EffectToSpawn = CurrentSkillData->ProjectileEffect;
            SpawnLocation = OwnerChar->GetMesh()->GetSocketLocation(FName("hand_r"));
            break;

        case EEffectSpawnType::Impact:
            EffectToSpawn = CurrentSkillData->ImpactEffect;
            break;

        default:
            break;
        }

        if (EffectToSpawn)
        {
            UNiagaraComponent* SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), EffectToSpawn, SpawnLocation);

            if (EffectType == EEffectSpawnType::Projectile && SpawnedEffect)
            {
                float Distance = FVector::Dist(SpawnLocation, Target->GetActorLocation());
               
                float FlightTime = 1.0f;

               
                float CalculatedSpeed = Distance / FlightTime;

              
                UProjectileMovementComponent* ProjMove = NewObject<UProjectileMovementComponent>(SpawnedEffect);
                ProjMove->SetUpdatedComponent(SpawnedEffect);

                ProjMove->InitialSpeed = CalculatedSpeed;   
                ProjMove->MaxSpeed = CalculatedSpeed;       

                ProjMove->bIsHomingProjectile = true;      
                ProjMove->HomingAccelerationMagnitude = 50000.f; 
                ProjMove->HomingTargetComponent = Target->GetRootComponent();
                ProjMove->RegisterComponent();

                FlyingProjectiles.Add(SpawnedEffect);
            }
        }
    }
}

