// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Matinee/InterpTrackVectorBase.h"
#include "InterpTrackAudioMaster.generated.h"

UCLASS(MinimalAPI, meta=( DisplayName = "Audio Master Track" ) )
class UInterpTrackAudioMaster : public UInterpTrackVectorBase
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
	// End UInterpTrack interface.

	/** Return the sound volume scale for the specified time */
	float GetVolumeScaleForTime( float Time ) const;

	/** Return the sound pitch scale for the specified time */
	float GetPitchScaleForTime( float Time ) const;

};



