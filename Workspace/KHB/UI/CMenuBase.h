#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Interfaces/CMenuInterface.h"
#include "CMenuBase.generated.h"

UCLASS()
class TEAMPROJECT_API UCMenuBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningInstance(ICMenuInterface* InInstance);

	void Startup();
	void Shutdown();

protected:
	ICMenuInterface* OwningInstance;
};
