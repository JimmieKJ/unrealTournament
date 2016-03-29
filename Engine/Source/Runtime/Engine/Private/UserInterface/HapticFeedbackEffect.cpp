// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "EnginePrivate.h"
#include "GameFramework/HapticFeedbackEffect.h"

UHapticFeedbackEffect::UHapticFeedbackEffect(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UHapticFeedbackEffect::GetValues(const float EvalTime, FHapticFeedbackValues& Values) const
{
	Values.Amplitude = HapticDetails.Amplitude.GetRichCurveConst()->Eval(EvalTime);
	Values.Frequency = HapticDetails.Frequency.GetRichCurveConst()->Eval(EvalTime);
}

float UHapticFeedbackEffect::GetDuration() const
{
	float AmplitudeMinTime, AmplitudeMaxTime;
	float FrequencyMinTime, FrequencyMaxTime;

	HapticDetails.Amplitude.GetRichCurveConst()->GetTimeRange(AmplitudeMinTime, AmplitudeMaxTime);
	HapticDetails.Frequency.GetRichCurveConst()->GetTimeRange(FrequencyMinTime, FrequencyMaxTime);

	return FMath::Max(AmplitudeMaxTime, FrequencyMaxTime);
}

bool FActiveHapticFeedbackEffect::Update(const float DeltaTime, FHapticFeedbackValues& Values)
{
	if (HapticEffect == nullptr)
	{
		return false;
	}

	const float Duration = HapticEffect->GetDuration();
	PlayTime += DeltaTime;

	if ((PlayTime > Duration) || (Duration == 0.f))
	{
		return false;
	}

	HapticEffect->GetValues(PlayTime, Values);
	Values.Amplitude *= Scale;
	return true;
}