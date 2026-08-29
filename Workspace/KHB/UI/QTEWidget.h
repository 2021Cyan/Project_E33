// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTEWidget.generated.h"

/**
 * 
 */
UCLASS()
class TEAMPROJECT_API UQTEWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void StartQTE(float InDuration, float InSuccessStart, float InSuccessEnd);

	UFUNCTION(BlueprintCallable)
	void SubmitQTE();

	UFUNCTION(BlueprintImplementableEvent)
	void BP_PlayQTEAnimation();

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnQTEFinished(bool bSuccess);

protected:


private:
	bool bQTEActive = false;
	bool bQTESubmitted = false;

	float StartRealTime = 0.f;
	float Duration = 1.25f;
	float SuccessStartTime = 1.0f;
	float SuccessEndTime = 1.25f;
};
