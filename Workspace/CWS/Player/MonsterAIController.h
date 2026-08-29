// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "MonsterAIController.generated.h"

class UBehaviorTree;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class APlayerCharacter;

UCLASS()
class TEAMPROJECT_API AMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMonsterAIController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float MaxChaseDistance = 1100.0f;

	// 전투 돌입 시 오버월드 AI를 끄고 켜는 스위치
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetOverworldAIEnabled(bool bEnable);

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	// AI가 사용할 비헤이비어 트리 (에디터에서 할당)
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	// 시각 인지 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "AI")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComp;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	// 무언가를 보거나 놓쳤을 때 실행될 함수
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// 거리를 주기적으로 검사할 타이머
	FTimerHandle TetherTimerHandle;
	void CheckTetherDistance();

	// 블랙보드 키 이름 캐싱
	const FName BBKey_TargetActor = TEXT("TargetActor");
	const FName BBKey_HomeLocation = TEXT("HomeLocation");

	UPROPERTY()
	TArray<APlayerCharacter*> SensedPlayers;

};