// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/** 
 * InterpTrackColorScale
 *
 */

#pragma once
#include "Matinee/InterpTrackVectorBase.h"
#include "InterpTrackColorScale.generated.h"

UCLASS(meta=( DisplayName = "Color Scale Track" ) )
class UInterpTrackColorScale : public UInterpTrackVectorBase
{
	GENERATED_UCLASS_BODY()


	// Begin UInterpTrack interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	virtual void SetTrackToSensibleDefault() override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	// Begin UInterpTrack interface.

	/** Return the blur alpha we want at the given time. */
	ENGINE_API FVector GetColorScaleAtTime(float Time);
};



