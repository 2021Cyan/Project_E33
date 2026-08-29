#include "BTTask_FindPatrolPos.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Player/MonsterAIController.h"

UBTTask_FindPatrolPos::UBTTask_FindPatrolPos()
{
	NodeName = TEXT("Find Patrol Position");
}

EBTNodeResult::Type UBTTask_FindPatrolPos::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	EBTNodeResult::Type Result = Super::ExecuteTask(OwnerComp, NodeMemory);

	// ���� ������ AI ��Ʈ�ѷ� ��������
	AMonsterAIController* AIController = Cast<AMonsterAIController>(OwnerComp.GetAIOwner());
	if (!AIController) return EBTNodeResult::Failed;

	APawn* ControllingPawn = AIController->GetPawn();
	if (!ControllingPawn) return EBTNodeResult::Failed;

	// ������̼� �ý��� ��������
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetNavigationSystem(ControllingPawn->GetWorld());
	if (!NavSystem) return EBTNodeResult::Failed;

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	// 1. �������忡�� �� ��ǥ
	FVector HomeOrigin = BB->GetValueAsVector(TEXT("HomeLocation"));

	float PatrolRadius = 700.0f;

	FNavLocation RandomLocation;

	// 3. ��(HomeOrigin)�� �߽����� ������ �ݰ� �ȿ��� �� �� �ִ� ������ ��ǥ�� ã��
	if (NavSystem->GetRandomReachablePointInRadius(HomeOrigin, PatrolRadius, RandomLocation))
	{
		// 4. ã�� ���� ��ǥ�� ���������� 'PatrolLocation' �̶�� Ű�� ����
		BB->SetValueAsVector(TEXT("PatrolLocation"), RandomLocation.Location);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}