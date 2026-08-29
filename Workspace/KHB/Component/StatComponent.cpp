#include "StatComponent.h"
#include "Net/UnrealNetwork.h"
#include "../Character/BaseCharacter.h"

UStatComponent::UStatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UStatComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UStatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UStatComponent, HP);
    DOREPLIFETIME(UStatComponent, MaxHP);
    DOREPLIFETIME(UStatComponent, AP);
    DOREPLIFETIME(UStatComponent, MaxAP);
    DOREPLIFETIME(UStatComponent, Attack);
    DOREPLIFETIME(UStatComponent, Defense);
    DOREPLIFETIME(UStatComponent, Speed);
}

void UStatComponent::OnRep_HP()
{
    OnStatChanged.Broadcast(ECharacterStatType::HP, HP);
    if (IsDead()) OnDead.Broadcast(Cast<ABaseCharacter>(GetOwner()));
}

void UStatComponent::OnRep_MaxHP() 
{ 
    OnStatChanged.Broadcast(ECharacterStatType::MaxHP, MaxHP);
}

void UStatComponent::OnRep_AP() 
{ 
    OnStatChanged.Broadcast(ECharacterStatType::AP, AP);
}

void UStatComponent::OnRep_MaxAP() 
{ 
    OnStatChanged.Broadcast(ECharacterStatType::MaxAP, MaxAP);
}

void UStatComponent::OnRep_Attack() 
{ 
    OnStatChanged.Broadcast(ECharacterStatType::Attack, Attack);
}

void UStatComponent::OnRep_Defense() 
{ 
    OnStatChanged.Broadcast(ECharacterStatType::Defense, Defense);
}

void UStatComponent::OnRep_Speed() 
{ 
    OnStatChanged.Broadcast(ECharacterStatType::Speed, Speed);
}

// Server RPC
void UStatComponent::Server_SetHP_Implementation(float NewHP)
{
    HP = FMath::Clamp(NewHP, 0.f, MaxHP);
    OnRep_HP();
}

void UStatComponent::Server_SetMaxHP_Implementation(float NewMaxHP)
{
    MaxHP = FMath::Max(0.f, NewMaxHP);
    OnRep_MaxHP();
}

void UStatComponent::Server_SetAP_Implementation(float NewAP)
{
    AP = FMath::Clamp(NewAP, 0.f, MaxAP);
    OnRep_AP();
}

void UStatComponent::Server_SetMaxAP_Implementation(float NewMaxAP)
{
    MaxAP = FMath::Max(0.f, NewMaxAP);
    OnRep_MaxAP();
}

void UStatComponent::Server_SetAttack_Implementation(float NewAttack)
{
    Attack = FMath::Max(0.f, NewAttack);
    OnRep_Attack();
}

void UStatComponent::Server_SetDefense_Implementation(float NewDefense)
{
    Defense = FMath::Max(0.f, NewDefense);
    OnRep_Defense();
}

void UStatComponent::Server_SetSpeed_Implementation(float NewSpeed)
{
    Speed = FMath::Max(0.f, NewSpeed);
    OnRep_Speed();
}