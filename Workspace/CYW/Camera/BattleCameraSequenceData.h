#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BattleCameraSequenceData.generated.h"

UENUM(BlueprintType)
enum class EBattleCameraEaseType : uint8 {
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut
};

USTRUCT(BlueprintType)
struct FBattleCameraCue
{
    GENERATED_BODY()

    // 카메라 기준 위치/회전에 더해질 정보 큐
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Cue")
    float Duration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Cue")
    FVector LocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Cue")
    FRotator RotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Cue")
    FVector ControlOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Cue")
    bool bUseControlOffset = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Cue")
    EBattleCameraEaseType EaseType = EBattleCameraEaseType::EaseOut;
};

USTRUCT(BlueprintType)
struct FBattleCameraFOVCue {
    GENERATED_BODY()

    // 위치/회전 큐와 별도로 재생되는 FOV 정보 큐
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FOV Cue")
    float Duration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FOV Cue")
    float FOV = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FOV Cue")
    EBattleCameraEaseType EaseType = EBattleCameraEaseType::EaseOut;
};
UCLASS()
class TEAMPROJECT_API UBattleCameraSequenceData : public UDataAsset
{
	GENERATED_BODY()
public:
    // 위치/회전 트랙과 FOV 트랙 중 더 긴 재생 시간을 반환
    float GetCameraSequenceDuration() const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle Camera Sequence")
	TArray<FBattleCameraCue> Cues;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle Camera Sequence")
    TArray<FBattleCameraFOVCue> FOVCues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle Camera Sequence")
	bool bEnableHandheldShakeSequence = true;
	
};
