// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatComponent.generated.h"

class ABaseCharacter;

UENUM(BlueprintType)
enum class ECharacterStatType : uint8 {
    HP, 
    MaxHP,
    AP,
    MaxAP,
    Attack,
    Defense,
    Speed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatChanged, ECharacterStatType, StatType, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDead, ABaseCharacter*, DeadCharacter);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TEAMPROJECT_API UStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    UStatComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    float GetHP() const { return HP; }
    float GetMaxHP() const { return MaxHP; }
    float GetAP() const { return AP; }
    float GetMaxAP() const { return MaxAP; }
    float GetAttack() const { return Attack; }
    float GetDefense() const { return Defense; }
    float GetSpeed() const { return Speed; }
    bool IsDead() const { return HP <= 0.f; }

    UFUNCTION(Server, Reliable) 
    void Server_SetHP(float NewHP);

    UFUNCTION(Server, Reliable) 
    void Server_SetMaxHP(float NewMaxHP);

    UFUNCTION(Server, Reliable) 
    void Server_SetAP(float NewAP);

    UFUNCTION(Server, Reliable) 
    void Server_SetMaxAP(float NewMaxAP);

    UFUNCTION(Server, Reliable) 
    void Server_SetAttack(float NewAttack);

    UFUNCTION(Server, Reliable) 
    void Server_SetDefense(float NewDefense);

    UFUNCTION(Server, Reliable) 
    void Server_SetSpeed(float NewSpeed);

    UPROPERTY(BlueprintAssignable) FOnStatChanged OnStatChanged;
    UPROPERTY(BlueprintAssignable) FOnDead OnDead;
protected:
	virtual void BeginPlay() override;

    UFUNCTION() 
    void OnRep_HP();

    UFUNCTION() 
    void OnRep_MaxHP();

    UFUNCTION() 
    void OnRep_AP();

    UFUNCTION() 
    void OnRep_MaxAP();

    UFUNCTION()
    void OnRep_Attack();

    UFUNCTION()
    void OnRep_Defense();

    UFUNCTION()
    void OnRep_Speed();

private:
    UPROPERTY(ReplicatedUsing = OnRep_HP, EditDefaultsOnly, Category = "Stats") 
    float HP = 1.0f;

    UPROPERTY(ReplicatedUsing = OnRep_MaxHP, EditDefaultsOnly, Category = "Stats") 
    float MaxHP = 1.0f;

    UPROPERTY(ReplicatedUsing = OnRep_AP, EditDefaultsOnly, Category = "Stats") 
    float AP;

    UPROPERTY(ReplicatedUsing = OnRep_MaxAP, EditDefaultsOnly, Category = "Stats") 
    float MaxAP;

    UPROPERTY(ReplicatedUsing = OnRep_Attack, EditDefaultsOnly, Category = "Stats") 
    float Attack;

    UPROPERTY(ReplicatedUsing = OnRep_Defense, EditDefaultsOnly, Category = "Stats") 
    float Defense;

    UPROPERTY(ReplicatedUsing = OnRep_Speed, EditDefaultsOnly, Category = "Stats") 
    float Speed;
};
