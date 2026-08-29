#pragma once

#include "CoreMinimal.h"
#include "BasePlayerController.h"
#include "LobbyPlayerController.generated.h"

class ULobbyHUDWidget;

UCLASS()
class TEAMPROJECT_API ALobbyPlayerController : public ABasePlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
    ALobbyPlayerController();

    void RefreshLobbyUI();

    UFUNCTION(Client, Reliable)
    void Client_RefreshLobbyUI();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
    void Server_SetSelectedCharacterIndex(int32 NewIndex);

    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
    void Server_SetReady(bool bNewReady);

private:
    UPROPERTY(EditDefaultsOnly, Category = "Lobby")
    TSubclassOf<ULobbyHUDWidget> LobbyHUDWidgetClass;

    UPROPERTY()
    TObjectPtr<ULobbyHUDWidget> LobbyHUDWidget;
};
