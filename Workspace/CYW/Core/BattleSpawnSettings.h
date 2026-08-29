#pragma once

#include "CoreMinimal.h"
#include "../../KHB/Character/MonsterCharacter.h"
#include "BattleSpawnSettings.generated.h"

USTRUCT(BlueprintType)
struct TEAMPROJECT_API FBattleMonsterSpawnSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Monster")
	TArray<TSubclassOf<AMonsterCharacter>> MonsterClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Monster", meta = (ClampMin = "1"))
	int32 MinMonsterCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Monster", meta = (ClampMin = "1"))
	int32 MaxMonsterCount = 3;

	bool HasValidMonsterClass() const
	{
		for (const TSubclassOf<AMonsterCharacter>& MonsterClass : MonsterClasses)
		{
			if (MonsterClass)
			{
				return true;
			}
		}

		return false;
	}
};
