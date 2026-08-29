#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TurnManager.generated.h"

class ABaseCharacter;

UCLASS()
class TEAMPROJECT_API UTurnManager : public UObject
{
    GENERATED_BODY()

public:
    void InitTurn(const TArray<ABaseCharacter*>& Participants);

    void NextTurn();

    ABaseCharacter* GetCurrentTurnCharacter() const;

    int32 GetCurrentCycle() const;

    void RemoveCharacter(ABaseCharacter* Target);

    const TArray<ABaseCharacter*>& GetTurnOrder() const { return TurnOrder; }
private:
    void RemoveInvalidParticipants();
    void SortBySpeed();
    void BeginNewCycle(const TCHAR* LogLabel);
    int32 GetNextTurnIndex() const;
    int32 FindFirstAliveIndexFrom(int32 StartIndex) const;
    void SetCurrentTurnByIndex(int32 NextIndex);
    void LogTurnOrderSummary(int32 LogStep, const TCHAR* LogLabel) const;
    void LogTurnOrderDetails() const;

    // 턴 순서 목록 (Speed 정렬된 참가자 배열)
    TArray<ABaseCharacter*> TurnOrder;

    UPROPERTY()
    ABaseCharacter* CurrentTurnCharacter;

    int32 PendingNextIndexAfterRemoval;

    // 현재 턴 수 
    int32 CurrentCycle;
};
