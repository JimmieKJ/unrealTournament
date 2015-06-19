// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InterpTrackFloatAnimBPParam.generated.h"

UCLASS(meta=( DisplayName = "Float Anim BP Parameter Track" ) )
class UInterpTrackFloatAnimBPParam : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()
	
	/** Materials whose parameters we want to change and the references to those materials. */
	UPROPERTY(EditAnywhere, Category=InterpTrackFloatAnimBPParam)
	class UAnimBlueprintGeneratedClass* AnimBlueprintClass;

	/** Name of parameter in the MaterialInstance which this track will modify over time. */
	UPROPERTY(EditAnywhere, Category=InterpTrackFloatAnimBPParam)
	FName ParamName;

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface

	// Begin UInterpTrack interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	// End UInterpTrack interface.

private:
	bool bRefreshParamter;
};



