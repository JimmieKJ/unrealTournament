// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "IInputInterface.h"

UForceFeedbackEffect::UForceFeedbackEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Make sure that by default the force feedback effect has an entry
	FForceFeedbackChannelDetails ChannelDetail;
	ChannelDetails.Add(ChannelDetail);
}

#if WITH_EDITOR
void UForceFeedbackEffect::PostEditChangeChainProperty( struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// After any edit (really we only care about the curve, but easier this way) update the cached duration value
	GetDuration();
}

#endif

float UForceFeedbackEffect::GetDuration()
{
	// Always recalc the duration when in the editor as it could change
	if( GIsEditor || ( Duration < SMALL_NUMBER ) )
	{
		Duration = 0.f;

		float MinTime, MaxTime;
		for (int32 Index = 0; Index < ChannelDetails.Num(); ++Index)
		{
			ChannelDetails[Index].Curve.GetRichCurve()->GetTimeRange(MinTime, MaxTime);

			if (MaxTime > Duration)
			{
				Duration = MaxTime;
			}
		}
	}

	return Duration;
}

void UForceFeedbackEffect::GetValues(const float EvalTime, FForceFeedbackValues& Values) const
{
	for (int32 Index = 0; Index < ChannelDetails.Num(); ++Index)
	{
		const FForceFeedbackChannelDetails& Details = ChannelDetails[Index];
		const float Value = Details.Curve.GetRichCurveConst()->Eval(EvalTime);

		if (Details.bAffectsLeftLarge)
		{
			Values.LeftLarge = FMath::Clamp(Value, Values.LeftLarge, 1.f);
		}
		if (Details.bAffectsLeftSmall)
		{
			Values.LeftSmall = FMath::Clamp(Value, Values.LeftSmall, 1.f);
		}
		if (Details.bAffectsRightLarge)
		{
			Values.RightLarge = FMath::Clamp(Value, Values.RightLarge, 1.f);
		}
		if (Details.bAffectsRightSmall)
		{
			Values.RightSmall = FMath::Clamp(Value, Values.RightSmall, 1.f);
		}
	}
}

bool FActiveForceFeedbackEffect::Update(const float DeltaTime, FForceFeedbackValues& Values)
{
	if (ForceFeedbackEffect == NULL)
	{
		return false;
	}

	const float Duration = ForceFeedbackEffect->GetDuration();

	PlayTime += DeltaTime;

	if (PlayTime > Duration && (!bLooping || Duration == 0.f) )
	{
		return false;
	}

	float EvalTime = PlayTime;
	while (EvalTime > Duration)
	{
		EvalTime -= Duration;
	}

	ForceFeedbackEffect->GetValues(EvalTime, Values);
	return true;
}