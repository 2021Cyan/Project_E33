// Fill out your copyright notice in the Description page of Project Settings.


#include "QTEWidget.h"
#include "Kismet/GameplayStatics.h"
#include "../Player/BasePlayerController.h"

void UQTEWidget::StartQTE(float InDuration, float InSuccessStart, float InSuccessEnd)
{
	bQTEActive = true;
	bQTESubmitted = false;

	Duration = InDuration;
	SuccessStartTime = InSuccessStart;
	SuccessEndTime = InSuccessEnd;
	StartRealTime = UGameplayStatics::GetRealTimeSeconds(this);

	UE_LOG(LogTemp, Warning, TEXT("[QTE] StartQTE: Duration=%.2f Success=%.2f~%.2f"),
		Duration,
		SuccessStartTime,
		SuccessEndTime);

	BP_PlayQTEAnimation();
}

void UQTEWidget::SubmitQTE()
{
	if (!bQTEActive || bQTESubmitted)
	{
		return;
	}

	bQTESubmitted = true;

	const float ElapsedTime = UGameplayStatics::GetRealTimeSeconds(this) - StartRealTime;
	const bool bSuccess = ElapsedTime >= SuccessStartTime && ElapsedTime <= SuccessEndTime;

	bQTEActive = false;

	UE_LOG(LogTemp, Warning, TEXT("[QTE] SubmitQTE: Elapsed=%.3f Success=%s"),
		ElapsedTime,
		bSuccess ? TEXT("true") : TEXT("false"));

	if (ABasePlayerController* PC = Cast<ABasePlayerController>(GetOwningPlayer()))
	{
		PC->Server_SubmitQTE(bSuccess);
	}
}
