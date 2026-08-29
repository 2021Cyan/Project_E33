#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h" // 데이터 테이블 쓰기 위한 헤더
#include "CombatTypes.generated.h" 

// --- 전방 선언 
class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;

 // 캐릭터 상태 (ECombatState)
UENUM(BlueprintType)
enum class ECombatState : uint8
{
    None        UMETA(DisplayName = "None"),
    Idle        UMETA(DisplayName = "Idle"),          // 대기 상태 (명령 대기)
    Attacking   UMETA(DisplayName = "Attacking"),     // 공격 중
    Parrying    UMETA(DisplayName = "Parrying"),      // 패링 판정 중
    Countering  UMETA(DisplayName = "Countering"),    // 반격 중 
    Hit         UMETA(DisplayName = "Hit"),           // 피격 당함
    Stunned     UMETA(DisplayName = "Stunned"),       
    Dead        UMETA(DisplayName = "Dead")           // 사망
};



 // 패링 판정 결과 (EParryResult)
UENUM(BlueprintType)
enum class EParryResult : uint8
{
    None        UMETA(DisplayName = "None"),
    Success     UMETA(DisplayName = "Success"),       // 일반 패링 성공 (데미지 방어)
    Fail        UMETA(DisplayName = "Fail"),          // 패링 실패 (데미지 받음)
};


// 스킬 타겟 타입 (ESkillTargetType)
UENUM(BlueprintType)
enum class ESkillTargetType : uint8
{
    SingleEnemy UMETA(DisplayName = "Single Enemy"),  // 적 1명
    AllEnemies  UMETA(DisplayName = "All Enemies"),   // 적 전체 (보스 광역기 등)
    Self        UMETA(DisplayName = "Self"),          
    AllAllies   UMETA(DisplayName = "All Allies")     
};



// 스킬/공격 마스터 데이터 (FSkillData)
USTRUCT(BlueprintType)
struct FSkillData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logic")
    FName SkillName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logic")
    FText SkillDescription; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logic")
    float Damage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logic")
    int32 APCost; // 스킬 사용에 필요한 AP

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logic")
    ESkillTargetType TargetType; // 타겟 수 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logic")
    int32 RequiredParryCount = 1; // 몇 번 패링해야 하는가


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* ActionMontage; // 이 스킬을 쓸 때 캐릭터가 재생할 몽타주


    // VFX
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    UNiagaraSystem* CastingEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    UNiagaraSystem* EruptionEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    UNiagaraSystem* ImpactEffect; // 적에게 맞았을 때
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFX")
    USoundBase* ImpactSound; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    UNiagaraSystem* ProjectileEffect;

    // =========================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Polish")
    TSubclassOf<UCameraShakeBase> CameraShake; // 때렸을 때 화면 흔들림 효과
};