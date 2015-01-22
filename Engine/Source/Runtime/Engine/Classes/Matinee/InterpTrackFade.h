// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Matinee/InterpTrackFloatBase.h"
#include "InterpTrackFade.generated.h"

UCLASS(meta=( DisplayName = "Fade Track" ) )
class UInterpTrackFade : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()

	/** 
	 * InterpTrackFade
	 *
	 * Special float property track that controls camera fading over time.
	 * Should live in a Director group.
	 *
	 */
	UPROPERTY(EditAnywhere, Category=InterpTrackFade)
	uint32 bPersistFade:1;


	// Begin UInterpTrack interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	// End UInterpTrack interface.

	/** @return the amount of fading we want at the given time. */
	ENGINE_API float GetFadeAmountAtTime(float Time);
};



