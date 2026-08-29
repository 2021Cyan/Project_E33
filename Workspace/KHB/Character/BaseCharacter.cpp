#include "BaseCharacter.h"
#include "Net/UnrealNetwork.h"
#include "../Component/StatComponent.h"
#include "../Core/BattleManager.h"
#include "Kismet/GameplayStatics.h" // ?? 맵에서 액터 찾기 위해 추가
#include "../Core/LevelStreamingManager.h"


ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	StatComponent = CreateDefaultSubobject<UStatComponent>(TEXT("StatComponent"));
}

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABaseCharacter, StatComponent);
	DOREPLIFETIME(ABaseCharacter, bInBattle);
	DOREPLIFETIME(ABaseCharacter, ReplicatedBattleManager);
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	StatComponent->OnDead.AddUniqueDynamic(this, &ABaseCharacter::OnDead);
}

void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

bool ABaseCharacter::IsDead() const
{
	return StatComponent->IsDead();
}

ABattleManager* ABaseCharacter::GetBattleManager()
{
	// 클라: 서버가 복제해 준 직통 포인터 사용. (BattleManagers TMap은 서버 전용이라
	// 클라가 BM 액터를 전수 스캔 + 배열 역추적하던 기존 방식은 replication race로 null이 됐다.)
	if (!HasAuthority())
		return ReplicatedBattleManager;

	// 서버/호스트: 권위 LSM 경로로 직접 조회
	ALevelStreamingManager* LSM = Cast<ALevelStreamingManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ALevelStreamingManager::StaticClass()));
	if (!LSM) return nullptr;
	return LSM->GetBattleManagerForActor(const_cast<ABaseCharacter*>(this));
}

void ABaseCharacter::SetBattleManager(ABattleManager* InBattleManager)
{
	if (!HasAuthority()) return;
	ReplicatedBattleManager = InBattleManager;
	ForceNetUpdate();
}

void ABaseCharacter::OnRep_ReplicatedBattleManager()
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] OnRepBattleManager: Character=%s BM=%s"),
		*GetName(),
		ReplicatedBattleManager ? *ReplicatedBattleManager->GetName() : TEXT("None"));
}

void ABaseCharacter::SetInBattle(bool bNewInBattle)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bInBattle == bNewInBattle)
	{
		return;
	}

	bInBattle = bNewInBattle;
}

bool ABaseCharacter::IsMyTurn()
{
	// 클라: 서버가 RPC로 알려준 턴 결정을 사용 (복제 BM에 의존하지 않음)
	if (!HasAuthority())
		return bClientTurnActive;

	// 서버/호스트: 권위 BattleManager로 직접 판정
	ABattleManager* BM = GetBattleManager();
	return BM && BM->GetCurrentTurnCharacter() == this;
}

void ABaseCharacter::OnDead(ABaseCharacter* DeadCharacter)
{
	if (!HasAuthority()) return;

	ABattleManager* BattleManager = GetBattleManager();
	if (BattleManager)
		BattleManager->NotifyCharacterDead(DeadCharacter);
}

void ABaseCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* Montage)
{
	if (!Montage) return;
	GetMesh()->GetAnimInstance()->Montage_Play(Montage);
}

void ABaseCharacter::Multicast_PlayMontageWithSync_Implementation(UAnimMontage* Montage, float ServerTime)
{
	if (!Montage) return;

	// 서버 전송 시각과 현재 시각의 차이만큼 몽타주 시작 위치를 앞으로 당겨 재생
	float Delay = GetWorld()->GetTimeSeconds() - ServerTime;
	float StartPosition = FMath::Max(0.f, Delay);
	GetMesh()->GetAnimInstance()->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, StartPosition);
}

void ABaseCharacter::Multicast_SetMontagePlayRate_Implementation(UAnimMontage* Montage, float Rate)
{
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		Anim->Montage_SetPlayRate(Montage, Rate);
	}
}

void ABaseCharacter::Server_RestorePlayRate_Implementation()
{
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		UAnimMontage* CurrentMontage = Anim->GetCurrentActiveMontage();
		Multicast_SetMontagePlayRate(CurrentMontage, 1.0f);
		UE_LOG(LogTemp, Warning, TEXT("[QTE] MontageRestored: Character=%s Montage=%s Rate=1.0"),
			*GetName(),
			CurrentMontage ? *CurrentMontage->GetName() : TEXT("None"));
	}
}
