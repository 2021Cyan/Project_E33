// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"

class UStatComponent;
class ABattleManager;

UCLASS()
class TEAMPROJECT_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UTexture2D> PortraitTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UTexture2D> TurnPortraitTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText CharacterName;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
                                                                                                                                                                                                  
	UStatComponent* GetStatComponent() const { return StatComponent; }

	UFUNCTION(BlueprintCallable)
	bool IsDead() const;

	// 딜레이 보정 없이 단순 재생 (연출용 - 타이밍 정밀도 불필요한 경우)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMontage(UAnimMontage* Montage);

	// 서버 호출 시각 기준으로 클라이언트에서 재생 위치 보정 (전투 판정 연동 몽타주용)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMontageWithSync(UAnimMontage* Montage, float ServerTime);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetMontagePlayRate(UAnimMontage* Montage, float Rate);

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_RestorePlayRate();

	// 베틀	매니저 접근자
	UFUNCTION(BlueprintCallable, Category = "Battle")
	class ABattleManager* GetBattleManager();

	// 서버 전용: 이 캐릭터가 속한 전투 매니저를 권위적으로 지정(클라에 복제됨)
	void SetBattleManager(class ABattleManager* InBattleManager);

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsInBattle() const { return bInBattle;}

	void SetInBattle(bool bNewInBattle);

	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsMyTurn();
protected:
	UFUNCTION()
	void OnDead(ABaseCharacter* DeadCharacter);

	UFUNCTION()
	virtual void OnRep_ReplicatedBattleManager();
protected:
	UPROPERTY(VisibleAnywhere, Replicated, Category = "Stats")
	TObjectPtr<UStatComponent> StatComponent;

	UPROPERTY(Replicated)
	bool bInBattle;

	// 클라 전용 턴 플래그. 서버가 Client_StartPlayerTurn/EndPlayerTurn RPC로 갱신.
	// 클라에서는 (비복제) BattleManager 대신 이 값으로 내 턴을 판정한다.
	bool bClientTurnActive = false;

	// 서버가 권위적으로 정한 전투 매니저. 클라는 BM 액터 전수 스캔/배열 역추적 대신
	// 이 복제 포인터로 자기 BM을 직통으로 얻는다(replication race 제거).
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedBattleManager)
	TObjectPtr<ABattleManager> ReplicatedBattleManager = nullptr;
};
