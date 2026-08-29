#include "TurnManager.h"
#include "../Character/BaseCharacter.h"
#include "../Component/StatComponent.h"
#include "GameFramework/PlayerState.h"
namespace
{
    FString BuildTurnOrderLog(const TArray<ABaseCharacter*>& TurnOrder)
    {
        TArray<FString> Names;
        for (int32 Index = 0; Index < TurnOrder.Num(); ++Index)
        {
            const ABaseCharacter* Character = TurnOrder[Index];
            Names.Add(FString::Printf(TEXT("%d:%s"), Index, Character ? *Character->GetName() : TEXT("None")));
        }

        return FString::Join(Names, TEXT(" -> "));
    }
}

void UTurnManager::InitTurn(const TArray<ABaseCharacter*>& Participants)
{
    TurnOrder = Participants;
    CurrentCycle = 1;
    PendingNextIndexAfterRemoval = INDEX_NONE;
    SortBySpeed();
    CurrentTurnCharacter = TurnOrder.IsValidIndex(0) ? TurnOrder[0] : nullptr;

    LogTurnOrderSummary(13, TEXT("TurnOrderSummary"));
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 13 TurnInit: Participants=%d"), TurnOrder.Num());
    LogTurnOrderDetails();
}

void UTurnManager::NextTurn()
{
    RemoveInvalidParticipants();
    if (TurnOrder.IsEmpty())
    {
        CurrentTurnCharacter = nullptr;
        PendingNextIndexAfterRemoval = INDEX_NONE;
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 23 NextTurnReady Failed: Participants=0"));
        return;
    }

    int32 NextIndex = GetNextTurnIndex();
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 22 NextTurn: NextIndex=%d Cycle=%d"), NextIndex, CurrentCycle);

    if (NextIndex >= TurnOrder.Num())
    {
        BeginNewCycle(TEXT("NewCycle"));
        NextIndex = 0;
    }

    NextIndex = FindFirstAliveIndexFrom(NextIndex);
    if (NextIndex == INDEX_NONE || NextIndex >= TurnOrder.Num())
    {
        BeginNewCycle(TEXT("NewCycleAfterSkip"));
        NextIndex = FindFirstAliveIndexFrom(0);
    }

    SetCurrentTurnByIndex(NextIndex);

    if (CurrentTurnCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 23 NextTurnReady: Character=%s Index=%d Cycle=%d"),
            *CurrentTurnCharacter->GetName(),
            TurnOrder.IndexOfByKey(CurrentTurnCharacter),
            CurrentCycle);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 23 NextTurnReady Failed: Index=%d Participants=%d"), NextIndex, TurnOrder.Num());
    }
}

ABaseCharacter* UTurnManager::GetCurrentTurnCharacter() const
{
    return CurrentTurnCharacter;
}

int32 UTurnManager::GetCurrentCycle() const
{
    return CurrentCycle;
}

void UTurnManager::RemoveCharacter(ABaseCharacter* Target)
{
    int32 RemovedIndex = TurnOrder.IndexOfByKey(Target);
    if (RemovedIndex == INDEX_NONE) return;

    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 24 RemoveTurnCharacter: Character=%s Index=%d"), Target ? *Target->GetName() : TEXT("None"), RemovedIndex);

    TurnOrder.RemoveAt(RemovedIndex);

    if (CurrentTurnCharacter == Target)
    {
        CurrentTurnCharacter = nullptr;
        PendingNextIndexAfterRemoval = RemovedIndex;
    }
}

void UTurnManager::RemoveInvalidParticipants()
{
    TurnOrder.RemoveAll([](const ABaseCharacter* Char)
        {
            return !Char || !Char->GetStatComponent() || Char->GetStatComponent()->IsDead();
        });
}

void UTurnManager::SortBySpeed()
{
    RemoveInvalidParticipants();

    TurnOrder.Sort([](const ABaseCharacter& A, const ABaseCharacter& B)
        {
            const float SpeedA = A.GetStatComponent() ? A.GetStatComponent()->GetSpeed() : 0.f;
            const float SpeedB = B.GetStatComponent() ? B.GetStatComponent()->GetSpeed() : 0.f;

            if (SpeedA != SpeedB)
            {
                return SpeedA > SpeedB;
            }

            const bool bAIsPlayer = A.IsPlayerControlled();
            const bool bBIsPlayer = B.IsPlayerControlled();

            if (bAIsPlayer != bBIsPlayer)
            {
                return bAIsPlayer;
            }

            if (bAIsPlayer && bBIsPlayer)
            {
                const APlayerState* PlayerStateA = A.GetPlayerState();
                const APlayerState* PlayerStateB = B.GetPlayerState();

                const int32 PlayerIdA = PlayerStateA ? PlayerStateA->GetPlayerId() : MAX_int32;
                const int32 PlayerIdB = PlayerStateB ? PlayerStateB->GetPlayerId() : MAX_int32;

                if (PlayerIdA != PlayerIdB)
                {
                    return PlayerIdA < PlayerIdB;
                }
            }

            return A.GetName() < B.GetName();
        });
}

void UTurnManager::BeginNewCycle(const TCHAR* LogLabel)
{
    CurrentCycle++;
    PendingNextIndexAfterRemoval = INDEX_NONE;
    SortBySpeed();
    LogTurnOrderSummary(22, TEXT("TurnOrderSummary"));
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 22 %s: Cycle=%d Participants=%d"), LogLabel, CurrentCycle, TurnOrder.Num());
}

int32 UTurnManager::GetNextTurnIndex() const
{
    if (PendingNextIndexAfterRemoval != INDEX_NONE)
    {
        return PendingNextIndexAfterRemoval;
    }

    const int32 CurrentIndex = TurnOrder.IndexOfByKey(CurrentTurnCharacter);
    if (CurrentIndex == INDEX_NONE)
    {
        return 0;
    }

    return CurrentIndex + 1;
}

int32 UTurnManager::FindFirstAliveIndexFrom(int32 StartIndex) const
{
    if (StartIndex < 0)
    {
        return INDEX_NONE;
    }

    for (int32 Index = StartIndex; Index < TurnOrder.Num(); ++Index)
    {
        ABaseCharacter* Character = TurnOrder[Index];
        if (Character && !Character->IsDead())
        {
            return Index;
        }

        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 22 SkipDeadTurn: Character=%s"), Character ? *Character->GetName() : TEXT("None"));
    }

    return INDEX_NONE;
}

void UTurnManager::SetCurrentTurnByIndex(int32 NextIndex)
{
    CurrentTurnCharacter = TurnOrder.IsValidIndex(NextIndex) ? TurnOrder[NextIndex] : nullptr;
    PendingNextIndexAfterRemoval = INDEX_NONE;
}

void UTurnManager::LogTurnOrderSummary(int32 LogStep, const TCHAR* LogLabel) const
{
    UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] %d %s Cycle=%d | %s"), LogStep, LogLabel, CurrentCycle, *BuildTurnOrderLog(TurnOrder));
}

void UTurnManager::LogTurnOrderDetails() const
{
    for (int32 Index = 0; Index < TurnOrder.Num(); ++Index)
    {
        ABaseCharacter* Character = TurnOrder[Index];
        UE_LOG(LogTemp, Warning, TEXT("[BattleFlow] 13 TurnOrder[%d]: Character=%s Speed=%.1f"),
            Index,
            Character ? *Character->GetName() : TEXT("None"),
            Character && Character->GetStatComponent() ? Character->GetStatComponent()->GetSpeed() : 0.0f);
    }
}
