#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonSpawnPoint.generated.h"

UENUM(BlueprintType)
enum class EBattleSpawnTeam : uint8
{
	Player UMETA(DisplayName = "Player"),
	Enemy UMETA(DisplayName = "Enemy")
};


UCLASS()
class TEAMPROJECT_API ADungeonSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ADungeonSpawnPoint();

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	FName LevelName;

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	EBattleSpawnTeam Team = EBattleSpawnTeam::Player;

	UPROPERTY(EditAnywhere, Category = "Dungeon")
	int32 Index;
};
