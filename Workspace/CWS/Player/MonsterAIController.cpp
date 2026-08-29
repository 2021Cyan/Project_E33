// Fill out your copyright notice in the Description page of Project Settings.

#include "MonsterAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h" // BT 멈춤용
#include "BrainComponent.h"                     // AI 뇌 제어용
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "../../KHB/Character/PlayerCharacter.h" 
#include "Perception/AISense_Sight.h"

AMonsterAIController::AMonsterAIController()
{
	// 1. 퍼셉션(인지) 컴포넌트 생성
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));

	// 2. 시각(Sight) 설정
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = 1300.0f; // 인지 거리 13미터
	SightConfig->LoseSightRadius = 1500.0f; // 시야에서 놓치는 거리 15미터
	SightConfig->PeripheralVisionAngleDegrees = 100.0f; // 시야각 200도
	SightConfig->SetMaxAge(2.0f); // 기억 시간 

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
}

void AMonsterAIController::BeginPlay()
{
	Super::BeginPlay();

	// 시각 인지 업데이트 이벤트 바인딩
	if (HasAuthority() && AIPerceptionComp)
	{
		AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AMonsterAIController::OnTargetPerceptionUpdated);
	}
}

void AMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!HasAuthority()) return;

	// 비헤이비어 트리 실행 및 초기 위치 저장
	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);

		if (UBlackboardComponent* BB = GetBlackboardComponent())
		{
			BB->SetValueAsVector(BBKey_HomeLocation, InPawn->GetActorLocation());
		}

		//  0.5초마다 플레이어가 구역을 벗어났는지 감시
		GetWorld()->GetTimerManager().SetTimer(
			TetherTimerHandle,
			this,
			&AMonsterAIController::CheckTetherDistance,
			0.5f,
			true
		);
	}
}

void AMonsterAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!HasAuthority()) return;

	APlayerCharacter* Player = Cast<APlayerCharacter>(Actor);
	if (!Player || Player->IsDead()) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		// 시야에 들어왔다 -> 일단 기억만
		SensedPlayers.AddUnique(Player);
	}
	else
	{
		SensedPlayers.Remove(Player);
	}
}

void AMonsterAIController::CheckTetherDistance()
{
	if (!HasAuthority()) return;

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB) return;

	FVector HomeLoc = BB->GetValueAsVector(BBKey_HomeLocation);
	AActor* CurrentTarget = Cast<AActor>(BB->GetValueAsObject(BBKey_TargetActor));

	
	// 이미 누군가를 쫓아가고 있는 경우 (포기할지 검사)
	if (CurrentTarget)
	{
		float TargetDistanceFromHome = FVector::Dist(HomeLoc, CurrentTarget->GetActorLocation());

		// 쫓던 애가 MaxChaseDistance에서 너무 멀어졌다면
		if (TargetDistanceFromHome > MaxChaseDistance || Cast<APlayerCharacter>(CurrentTarget)->IsDead())
		{
			BB->ClearValue(BBKey_TargetActor); //  포기
			CurrentTarget = nullptr;            // 기억도 지움

			UE_LOG(LogTemp, Warning, TEXT("[AI] 플레이어가 구역을 벗어났습니다. 집으로 돌아갑니다."));
		}
	}

	// 쫓는 사람은 없는데, 내 시야(SensedPlayer)에 누군가 보일 때 
	if (!CurrentTarget)
	{
		//죽었거나 연결이 끊긴 플레이어가 명단에 있다면 싹 지워줌
		SensedPlayers.RemoveAll([](APlayerCharacter* P) { return P == nullptr || P->IsDead(); });

		APlayerCharacter* NewTarget = nullptr;
		float ClosestDistance = MaxChaseDistance; // 집 구역 안에 있는 사람만 찾음

		// 명단에 있는 모든 플레이어를 검사함
		for (APlayerCharacter* Player : SensedPlayers)
		{
			float DistFromHome = FVector::Dist(HomeLoc, Player->GetActorLocation());

			// 플레이어가 집 구역 안에 있다면
			if (DistFromHome <= MaxChaseDistance)
			{
				// 그 중 집에서 가장 가까운 사람을 타겟
				if (DistFromHome < ClosestDistance)
				{
					ClosestDistance = DistFromHome;
					NewTarget = Player;
				}
			}
		}

		// 구역 안에 있는 새로운 타겟을 찾았다면 즉시 쫓아감
		if (NewTarget)
		{
			BB->SetValueAsObject(BBKey_TargetActor, NewTarget);
			UE_LOG(LogTemp, Warning, TEXT("[AI] 구역 안의 새로운 타겟(%s)을 포착했습니다! 추적 시작."), *NewTarget->GetName());
		}
		// 만약 구역 안에 아무도 없다면 타겟은 계속 nullptr이 유지되고,
		// 비헤이비어 트리의 오른쪽 시퀀스가 작동하여 자연스럽게 집으로 돌아감
	}
}


	// 전투 진입 시 순찰 로직 종료 및 시야 끄기, 전투 종료 시 다시 켜기
void AMonsterAIController::SetOverworldAIEnabled(bool bEnable)
{
	if (!HasAuthority()) return;

	if (!bEnable)
	{
		// 즉시 정지
		StopMovement();

		// 비헤이비어 트리 연산 완전 중지
		if (UBrainComponent* BrainComp = GetBrainComponent())
		{
			BrainComp->StopLogic("Entered Battle Mode");
		}

		// 시야(눈) 끄기 - 전투 중에 타겟팅 바뀌거나 고개 돌아가는 것 방지
		if (AIPerceptionComp)
		{
			AIPerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
		}

		UE_LOG(LogTemp, Warning, TEXT("[AI] 전투 진입! 오버월드 추적 AI 및 시야를 셧다운합니다."));
	}
	else
	{
		if (BehaviorTreeAsset) RunBehaviorTree(BehaviorTreeAsset);
		if (AIPerceptionComp) AIPerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), true);

		GetWorld()->GetTimerManager().SetTimer(TetherTimerHandle, this, &AMonsterAIController::CheckTetherDistance, 0.5f, true);
	}
}
