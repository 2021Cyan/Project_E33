#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "BattleCinematicManager.generated.h"

class ABattleCameraActor;
class ABasePlayerController;
class AMonsterCharacter;
class APlayerCharacter;
class UBattleCameraSequenceData;
class ULevelSequence;

DECLARE_DELEGATE(FOnBattleCinematicFinished);

UCLASS()
class TEAMPROJECT_API ABattleCinematicManager : public AActor
{
	GENERATED_BODY()

public:
	ABattleCinematicManager();

	void OnBattleStarted(const TArray<ABasePlayerController*>& Participants, FOnBattleCinematicFinished OnFinished);

	void OnPlayerTurnStarted(APlayerCharacter* Player);

	void OnPlayerActionExecuted(APlayerCharacter* Player);

	void OnEnemyTurnStarted(AMonsterCharacter* Monster, const TArray<ABasePlayerController*>& Participants);

	void OnBattleEnded(const TArray<ABasePlayerController*>& Participants);

	// 모든 참가자를 공유 전투 카메라로 전환
	void ApplySharedCamera(const TArray<ABasePlayerController*>& Participants, float BlendTime);

	// 전투 시작 카메라 시퀀스를 모든 참가자에게 재생
	void PlaySharedCamera(const TArray<ABasePlayerController*>& Participants);

	// 적 턴 상황에 맞춰 공유 카메라 위치를 계산하고 참가자에게 동기화
	void MoveCameraToEnemyTurn(AMonsterCharacter* Monster, const TArray<ABasePlayerController*>& Participants);

	// 공유 카메라를 전투 시작 시점의 기본 위치로 복귀
	void MoveCameraToBase(const TArray<ABasePlayerController*>& Participants);

	void PlayEnemyAttackCameraImpact(const TArray<ABasePlayerController*>& Participants);

	void PlayCounterLevelSequence(APlayerCharacter* Player, bool bGroupCounter, const TArray<ABasePlayerController*>& Participants);
private:
	// 적 턴 카메라에 사용할 대상과 광역 스킬 여부를 판단
	AActor* GetEnemyTurnTarget(AMonsterCharacter* Monster, bool& bOutUseDefaultCameraLocation) const;

	// 적 턴 카메라가 기준으로 삼을 초점 위치를 반환
	FVector GetEnemyTurnFocusLocation(AActor* TargetActor) const;

	// 적, 대상, 스킬 정보를 바탕으로 최종 카메라 연출을 구성
	bool BuildEnemyTurnCameraTransform(
		AMonsterCharacter* Monster,
		AActor* TargetActor,
		bool bUseDefaultCameraLocation,
		FVector& OutCameraLocation,
		FRotator& OutCameraRotation,
		FVector& OutLookTarget
	) const;

	// 전투 시작 시퀀스 길이에 맞춰 완료 콜백을 예약
	void ScheduleBattleStartCompletion(FOnBattleCinematicFinished OnFinished);
	void CompletePendingBattleStart();

protected:
	UPROPERTY(EditInstanceOnly, Category = "BattleCamera")
	TObjectPtr<ABattleCameraActor> Camera;

	UPROPERTY(EditAnywhere, Category = "Battle Camera")
	UBattleCameraSequenceData* BattleStartSequence;

	UPROPERTY(EditAnywhere, Category = "Battle Camera|Counter")
	TObjectPtr<ULevelSequence> GroupCounterLevelSequence;

	UPROPERTY(EditInstanceOnly, Category = "Battle Camera|Counter")
	TObjectPtr<AActor> CounterSequenceOriginActor;

	UPROPERTY(EditAnywhere, Category = "Camera|BlendTime")
	float BlendTime_Fast = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Camera|EnemyTurn")
	float EnemyTurnForwardDistance = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|EnemyTurn")
	float EnemyTurnHeight = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|EnemyTurn")
	float EnemyTurnLookHeight = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|EnemyTurn")
	float EnemyTurnSideOffset = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|EnemyTurn")
	float EnemyTurnLookForwardDistance = 200.0f;

	FTimerHandle BattleStartTimerHandle;
	FOnBattleCinematicFinished PendingBattleStartFinished;
};
