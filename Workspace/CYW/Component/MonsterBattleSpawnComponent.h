#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Core/BattleSpawnSettings.h"
#include "MonsterBattleSpawnComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_API UMonsterBattleSpawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMonsterBattleSpawnComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Monster")
	FBattleMonsterSpawnSettings SpawnSettings;
};
