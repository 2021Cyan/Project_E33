// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "MonsterCharacter.generated.h"

class USphereComponent;
class APlayerCharacter;
class UMonsterBattleSpawnComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyEnterBattle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyTurnStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyTurnEnd);

UCLASS()
class TEAMPROJECT_API AMonsterCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
	AMonsterCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void StartAITurn();
	virtual void EndAITurn();
	void EnterBattle();

	FName GetRandomSkillRowName() const;

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	bool IsBossEncounter() const { return bIsBossEncounter; }

	UPROPERTY(BlueprintAssignable)
	FOnEnemyEnterBattle OnEnemyEnterBattle;

	UPROPERTY(BlueprintAssignable)
	FOnEnemyTurnStart OnEnemyTurnStart;

	UPROPERTY(BlueprintAssignable)
	FOnEnemyTurnEnd OnEnemyTurnEnd;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnEncounterBeginOverlap(UPrimitiveComponent* OverlappedComponent,AActor* OtherActor,UPrimitiveComponent* OtherComp,int32 OtherBodyIndex,bool bFromSweep,const FHitResult& SweepResult);

	void TryStartEncounter(APlayerCharacter* TriggerPlayer);

	// 전투 진입 시 AI 컨트롤러를 끄는 함수
	UFUNCTION()
	void DisableOverworldAI();

	UFUNCTION()
	void OnRep_HasEnteredBattle();

	void BroadcastEnemyEnterBattle();
	void TryBroadcastEnemyEnterBattle();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> EncounterSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	bool bEncounterEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	float EncounterJoinRadius = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FName DungeonLevelName = TEXT("LV_Dungeon");

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	bool bIsBossEncounter = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter|Battle Spawn")
	TObjectPtr<UMonsterBattleSpawnComponent> BattleSpawnComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TArray<FName> MonsterSkillRowNames;

	UPROPERTY(ReplicatedUsing = OnRep_HasEnteredBattle)
	bool bHasEnteredBattle = false;

	bool bEnemyEnterBattleBroadcasted = false;

	bool bEncounterTriggered = false;
};
