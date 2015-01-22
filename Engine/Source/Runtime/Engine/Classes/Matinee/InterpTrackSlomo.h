// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InterpTrackSlomo.generated.h"

UCLASS(meta=( DisplayName = "Slomo Track" ) )
class UInterpTrackSlomo : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()


	// Begin InterpTrack interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	virtual void SetTrackToSensibleDefault() override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	// End InterpTrack interface.

	/** @return the slomo factor we want at the given time. */
	ENGINE_API float GetSlomoFactorAtTime(float Time);
};



