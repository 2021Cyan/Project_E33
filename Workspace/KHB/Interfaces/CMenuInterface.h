#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CMenuInterface.generated.h"

UINTERFACE(MinimalAPI)
class UCMenuInterface : public UInterface
{
	GENERATED_BODY()
};

class TEAMPROJECT_API ICMenuInterface
{
	GENERATED_BODY()

public:
	virtual void Host(FString ServerName) = 0;
	virtual void Join(uint32 Index) = 0;
	virtual void OpenMainMenuLevel() = 0;
	virtual void RefreshServerList() = 0;
};
