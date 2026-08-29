#include "BattleCinematicManager.h"
#include "BattleCameraActor.h"
#include "BattleCameraSequenceData.h"
#include "../../KHB/Player/BasePlayerController.h"
#include "../../KHB/Character/PlayerCharacter.h"
#include "../../KHB/Character/MonsterCharacter.h"
#include "../../CWS/CombatComponent.h"
#include "LevelSequence.h"
#include "TimerManager.h"

ABattleCinematicManager::ABattleCinematicManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABattleCinematicManager::OnBattleStarted(const TArray<ABasePlayerController*>& Participants, FOnBattleCinematicFinished OnFinished)
{
	if (!HasAuthority())
	{
		return;
	}

	// 전투 시작 시 모든 참가자가 같은 공유 카메라와 시작 시퀀스를 보도록 설정
	MoveCameraToBase(Participants);
	ApplySharedCamera(Participants, 0.0f);
	PlaySharedCamera(Participants);
	ScheduleBattleStartCompletion(OnFinished);
}

void ABattleCinematicManager::OnPlayerTurnStarted(APlayerCharacter* Player)
{
	if (!HasAuthority() || !Player)
	{
		return;
	}

	if (ABasePlayerController* PC = Cast<ABasePlayerController>(Player->GetController()))
	{
		// 플레이어 턴에는 본인 전투 카메라로 복귀
		PC->Client_ReturnToPersonalBattleCamera(BlendTime_Fast);
	}
}

void ABattleCinematicManager::OnPlayerActionExecuted(APlayerCharacter* Player)
{
	if (!HasAuthority() || !Player)
	{
		return;
	}

	if (ABasePlayerController* PC = Cast<ABasePlayerController>(Player->GetController()))
	{
		// 연출이 실행되면 연출을 공유 카메라 기준으로 전환
		PC->Client_ApplySharedBattleCamera(Camera, BlendTime_Fast);
	}
}

void ABattleCinematicManager::OnEnemyTurnStarted(AMonsterCharacter* Monster, const TArray<ABasePlayerController*>& Participants)
{
	if (!HasAuthority())
	{
		return;
	}

	// 적 턴은 모든 참가자가 같은 구도를 보도록 공유 카메라를 적용
	ApplySharedCamera(Participants, BlendTime_Fast);
	MoveCameraToEnemyTurn(Monster, Participants);
}

void ABattleCinematicManager::OnBattleEnded(const TArray<ABasePlayerController*>& Participants)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Camera)
	{
		// 전투 종료 시 적 턴 추적 상태가 남지 않도록 정리
		Camera->StopEnemyTurnPullBackTracking();
	}

	for (ABasePlayerController* PC : Participants)
	{
		if (PC)
		{
			PC->Client_StopCounterLevelSequence();
			PC->Client_ReturnToExplorationCamera(0.0f);
		}
	}
}

void ABattleCinematicManager::ApplySharedCamera(const TArray<ABasePlayerController*>& Participants, float BlendTime)
{
	if (!HasAuthority()) 
	{
		return;
	}

	if (!Camera) 
	{
		return;
	}

	// 서버에서 각 클라이언트의 ViewTarget을 공유 카메라로 전환
	for (ABasePlayerController* PC : Participants)
	{
		PC->Client_ApplySharedBattleCamera(Camera, BlendTime);
	}
}

void ABattleCinematicManager::PlaySharedCamera(const TArray<ABasePlayerController*>& Participants)
{
	if (!HasAuthority()) 
	{
		return;
	}

	if (!Camera) 
	{
		return;
	}

	// 공유 카메라가 적용된 뒤 실제 카메라 시퀀스를 재생
	for (ABasePlayerController* PC : Participants)
	{
		PC->Client_PlaySharedBattleCamera(Camera, BattleStartSequence);
	}
}

void ABattleCinematicManager::MoveCameraToEnemyTurn(AMonsterCharacter* Monster, const TArray<ABasePlayerController*>& Participants)
{
	if (!Monster || Monster->IsDead() || !Camera)
	{
		return;
	}

	// 적 턴 구도는 대상 선택, 카메라 위치 계산, 클라이언트 동기화 순서로 처리
	bool bUseDefaultCameraLocation = false;
	AActor* TargetActor = GetEnemyTurnTarget(Monster, bUseDefaultCameraLocation);
	if (!TargetActor)
	{
		return;
	}

	FVector FinalCameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	FVector LookTarget = FVector::ZeroVector;

	if (!BuildEnemyTurnCameraTransform(
		Monster,
		TargetActor,
		bUseDefaultCameraLocation,
		FinalCameraLocation,
		CameraRotation,
		LookTarget
	))
	{
		return;
	}

	Camera->SetBaseCameraTransform(FinalCameraLocation, CameraRotation);
	Camera->StartEnemyTurnPullBackTracking(Monster, TargetActor, LookTarget, bUseDefaultCameraLocation);

	// 클라이언트도 같은 기준 위치에서 적 턴 추적을 시작하도록 전달
	for (ABasePlayerController* PC : Participants)
	{
		if (PC)
		{
			PC->Client_MoveSharedBattleCameraEnemyTurn(
				Camera,
				Monster,
				TargetActor,
				FinalCameraLocation,
				CameraRotation,
				LookTarget,
				bUseDefaultCameraLocation
			);
		}
	}
}

void ABattleCinematicManager::MoveCameraToBase(const TArray<ABasePlayerController*>& Participants)
{
	if (!Camera) 
	{
		return;
	}

	// 공유 카메라 상태를 서버와 클라이언트 모두 기본 위치로 복귀
	Camera->StopEnemyTurnPullBackTracking();
	Camera->ReturnToDefaultPosition();

	for (ABasePlayerController* PC : Participants)
	{
		if (PC)
		{
			PC->Client_ReturnSharedBattleCameraToBase(Camera);
		}
	}
}

void ABattleCinematicManager::PlayEnemyAttackCameraImpact(const TArray<ABasePlayerController*>& Participants)
{
	if (!Camera)
	{
		return;
	}

	// 서버 카메라와 각 클라이언트 카메라에 동일한 연출 효과를 재생
	Camera->PlayAttackImpact();

	for (ABasePlayerController* PC : Participants)
	{
		if (PC)
		{
			PC->Client_PlaySharedBattleCameraImpact(Camera);
		}
	}
}

void ABattleCinematicManager::PlayCounterLevelSequence(APlayerCharacter* Player, bool bGroupCounter, const TArray<ABasePlayerController*>& Participants)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!Player)
	{
		return;
	}

	if (!CounterSequenceOriginActor)
	{
		return;
	}

	ULevelSequence* CounterSequence = bGroupCounter ? GroupCounterLevelSequence.Get() : Player->GetSingleCounterLevelSequence();
	
	if (!CounterSequence)
	{
		return;
	}

	// 단체 카운터는 지정된 액터를, 단일 카운터는 플레이어 위치를 시퀀스 원점으로 설정
	const FVector OriginLocation = bGroupCounter
		? CounterSequenceOriginActor->GetActorLocation()
		: Player->GetActorLocation();
	const FRotator OriginRotation = bGroupCounter
		? CounterSequenceOriginActor->GetActorRotation()
		: Player->GetActorRotation();

	for (ABasePlayerController* PC : Participants)
	{
		if (PC)
		{
			PC->Client_PlayCounterLevelSequence(
				CounterSequence,
				CounterSequenceOriginActor,
				OriginLocation,
				OriginRotation,
				!bGroupCounter
			);
		}
	}
}

AActor* ABattleCinematicManager::GetEnemyTurnTarget(AMonsterCharacter* Monster, bool& bOutUseDefaultCameraLocation) const
{
	bOutUseDefaultCameraLocation = false;

	if (!Monster)
	{
		return nullptr;
	}

	UCombatComponent* Combat = Monster->FindComponentByClass<UCombatComponent>();
	if (!Combat)
	{
		return nullptr;
	}

	if (const FSkillData* SkillData = Combat->GetCurrentSkillData())
	{
		// 광역 공격은 기본 전투 카메라 위치를 우선 사용
		bOutUseDefaultCameraLocation = SkillData->TargetType == ESkillTargetType::AllEnemies;
	}

	if (AActor* CurrentTarget = Combat->GetCurrentTarget())
	{
		return CurrentTarget;
	}

	// 현재 타겟이 없으면 살아있는 대상 중 하나를 카메라 기준 대상으로 사용
	const TArray<ABaseCharacter*> AliveTargets = Combat->GetAliveEnemyTargets();
	return AliveTargets.Num() > 0 ? AliveTargets[0] : nullptr;
}

FVector ABattleCinematicManager::GetEnemyTurnFocusLocation(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return FVector::ZeroVector;
	}

	if (ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(TargetActor))
	{
		if (UCombatComponent* TargetCombat = TargetCharacter->FindComponentByClass<UCombatComponent>())
		{
			// 원래 전투 위치를 기준으로 설정
			return TargetCombat->GetOriginalLocation();
		}
	}

	return TargetActor->GetActorLocation();
}

bool ABattleCinematicManager::BuildEnemyTurnCameraTransform(
	AMonsterCharacter* Monster,
	AActor* TargetActor,
	bool bUseDefaultCameraLocation,
	FVector& OutCameraLocation,
	FRotator& OutCameraRotation,
	FVector& OutLookTarget
) const
{
	if (!Monster || !TargetActor || !Camera)
	{
		return false;
	}

	const FVector MonsterLocation = Monster->GetActorLocation();
	const FVector TargetLocation = GetEnemyTurnFocusLocation(TargetActor);

	// 공격 방향을 기준으로 카메라와 주시점을 계산
	FVector AttackDirection = MonsterLocation - TargetLocation;
	AttackDirection.Z = 0.0f;
	if (AttackDirection.IsNearlyZero())
	{
		AttackDirection = Monster->GetActorForwardVector();
		AttackDirection.Z = 0.0f;
	}

	AttackDirection = AttackDirection.GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, AttackDirection).GetSafeNormal();

	OutLookTarget =
		TargetLocation +
		AttackDirection * EnemyTurnLookForwardDistance +
		FVector(0.0f, 0.0f, EnemyTurnLookHeight);

	const FVector CameraLocation =
		TargetLocation -
		AttackDirection * EnemyTurnForwardDistance +
		Right * EnemyTurnSideOffset +
		FVector(0.0f, 0.0f, EnemyTurnHeight);

	// 광역 공격은 계산한 개별 대상 구도 대신 기본 전투 카메라 위치를 사용
	OutCameraLocation = bUseDefaultCameraLocation ? Camera->GetDefaultCameraLocation() : CameraLocation;
	OutCameraRotation = (OutLookTarget - OutCameraLocation).Rotation();

	return true;
}

void ABattleCinematicManager::ScheduleBattleStartCompletion(FOnBattleCinematicFinished OnFinished)
{
	PendingBattleStartFinished = OnFinished;

	// 시작 시퀀스가 없으면 즉시 전투 시작 완료 콜백을 실행
	const float SequenceDuration = BattleStartSequence ? BattleStartSequence->GetCameraSequenceDuration() : 0.0f;
	if (SequenceDuration <= 0.0f)
	{
		CompletePendingBattleStart();
		return;
	}

	GetWorldTimerManager().ClearTimer(BattleStartTimerHandle);
	GetWorldTimerManager().SetTimer(
		BattleStartTimerHandle,
		this,
		&ABattleCinematicManager::CompletePendingBattleStart,
		SequenceDuration,
		false
	);
}

void ABattleCinematicManager::CompletePendingBattleStart()
{
	GetWorldTimerManager().ClearTimer(BattleStartTimerHandle);
	FOnBattleCinematicFinished FinishedCallback = PendingBattleStartFinished;
	PendingBattleStartFinished.Unbind();

	if (FinishedCallback.IsBound())
	{
		FinishedCallback.Execute();
	}
}

