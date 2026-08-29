#include "BattleCameraSequenceData.h"

float UBattleCameraSequenceData::GetCameraSequenceDuration() const
{
	// 위치/회전 트랙과 FOV 트랙 길이가 다를 수 있으므로 더 긴 쪽을 전체 재생 시간으로 반환하여 사용
	float TransformDuration = 0.0f;

	for (const FBattleCameraCue& Cue : Cues) {
		TransformDuration += FMath::Max(Cue.Duration, 0.0f);
	}

	float FOVDuration = 0.0f;

	for (const FBattleCameraFOVCue& Cue : FOVCues) {
		FOVDuration += FMath::Max(Cue.Duration, 0.0f);
	}

	return FMath::Max(TransformDuration, FOVDuration);
}
