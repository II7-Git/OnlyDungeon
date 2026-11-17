// OnlyDungeon 화면 흔들림

#include "ODCameraShakes.h"

#include "Shakes/PerlinNoiseCameraShakePattern.h"

UODCameraShake_Heavy::UODCameraShake_Heavy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UPerlinNoiseCameraShakePattern* Pattern = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
	Pattern->Duration = 1.2f;
	Pattern->BlendInTime = 0.05f;
	Pattern->BlendOutTime = 0.6f;

	Pattern->LocationAmplitudeMultiplier = 1.0f;
	Pattern->X.Amplitude = 2.0f;
	Pattern->X.Frequency = 12.0f;
	Pattern->Y.Amplitude = 2.0f;
	Pattern->Y.Frequency = 12.0f;
	Pattern->Z.Amplitude = 5.0f;
	Pattern->Z.Frequency = 14.0f;

	Pattern->RotationAmplitudeMultiplier = 1.0f;
	Pattern->Pitch.Amplitude = 1.2f;
	Pattern->Pitch.Frequency = 10.0f;
	Pattern->Yaw.Amplitude = 0.8f;
	Pattern->Yaw.Frequency = 9.0f;
	Pattern->Roll.Amplitude = 0.6f;
	Pattern->Roll.Frequency = 8.0f;

	SetRootShakePattern(Pattern);
}
