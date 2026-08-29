#include "AnimNotifyState_MeleeDash.h"
#include "../CombatComponent.h"
#include "GameFramework/Character.h"
#include "../../KHB/Character/BaseCharacter.h"

void UAnimNotifyState_MeleeDash::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

    if (!MeshComp || !MeshComp->GetOwner()) return;

    ACharacter* OwnerChar = Cast<ACharacter>(MeshComp->GetOwner());
    UCombatComponent* CombatComp = OwnerChar ? OwnerChar->FindComponentByClass<UCombatComponent>() : nullptr;

    if (OwnerChar && CombatComp)
    {

        FVector StartPos = OwnerChar->GetActorLocation();
        FVector EndPos;

        //  목적지 계산
        if (bIsDashOut)
        {
            // 저장해둔 원래 자리로
            EndPos = CombatComp->GetOriginalLocation();
        }
        else
        {
            //  현재 사용 중인 스킬 데이터를 가져옴
            FSkillData* SkillData = CombatComp->GetCurrentSkillData();

            // 타겟도 없고 스킬 데이터도 없으면 돌진 취소 (에러 방지)
            if (!SkillData && !CombatComp->GetCurrentTarget()) return;

            //  광역기(AllEnemies)일 경우 
            if (SkillData && SkillData->TargetType == ESkillTargetType::AllEnemies)
            {
                TArray<ABaseCharacter*> Enemies = CombatComp->GetAliveEnemyTargets();
                if (Enemies.Num() > 0)
                {
                    FVector CenterPos = FVector::ZeroVector;

                    // 모든 적의 좌표를 더함
                    for (ABaseCharacter* Enemy : Enemies)
                    {
                        CenterPos += Enemy->GetActorLocation();
                    }

                    //평균 위치(정중앙)
                    CenterPos /= Enemies.Num();

                    EndPos = CenterPos;
                }
                else
                {
                    return; // 적이 아무도 없으면 취소
                }
            }
            // 단일기(SingleEnemy)일 경우 
            else
            {
                AActor* Target = CombatComp->GetCurrentTarget();
                if (!Target) return;

                EndPos = Target->GetActorLocation();
            }

            //  멈출 위치(StopDistance) 적용
            // 타겟(또는 중앙)에서 나를 향하는 방향 벡터를 구함
            FVector Direction = (StartPos - EndPos).GetSafeNormal();

            // Z값(높이)은 무시해서 허공으로 날거나 땅으로 파고들지 않게 함
            Direction.Z = 0.0f;

            // 목적지에서 내 쪽으로 StopDistance 만큼 뒤로 물러난 곳에 멈춤
            EndPos += Direction * StopDistance;
        }



        FVector NewLocation = FMath::VInterpConstantTo(StartPos, EndPos, FrameDeltaTime, DashSpeed);

        // Z축은 내 원래 높이를 유지
        NewLocation.Z = StartPos.Z;

        OwnerChar->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

