// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/BrushComponent.h"
#include "Engine/PostProcessVolume.h"

APostProcessVolume::APostProcessVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// post process volume needs physics data for trace
	GetBrushComponent()->bAlwaysCreatePhysicsState = true;
	GetBrushComponent()->Mobility = EComponentMobility::Movable;
	
	bEnabled = true;
	BlendRadius = 100.0f;
	BlendWeight = 1.0f;
}

bool APostProcessVolume::EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint)
{
	return Super::EncompassesPoint(Point, SphereRadius, OutDistanceToPoint);
}

#if WITH_EDITOR
void APostProcessVolume::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		Settings.OnAfterLoad();
	}
}

void APostProcessVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FName NAME_Blendables = FName(TEXT("Blendables"));
	
	if(PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == NAME_Blendables)
	{
		// remove unsupported types
		uint32 Count = Settings.WeightedBlendables.Array.Num();
		for(uint32 i = 0; i < Count; ++i)
		{
			UObject* Obj = Settings.WeightedBlendables.Array[i].Object;

			if(!Cast<IBlendableInterface>(Obj))
			{
				Settings.WeightedBlendables.Array[i] = FWeightedBlendable();
			}
		}
	}
}

bool APostProcessVolume::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		// Settings, can be shared for multiple objects types (volume, component, camera, player)
		{
			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldScale))
			{
				return	( Settings.DepthOfFieldMethod == EDepthOfFieldMethod::DOFM_BokehDOF ||
						  Settings.DepthOfFieldMethod == EDepthOfFieldMethod::DOFM_Gaussian );
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldMaxBokehSize) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldColorThreshold) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldSizeThreshold) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldBokehShape))
			{
				return Settings.DepthOfFieldMethod == EDepthOfFieldMethod::DOFM_BokehDOF;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldNearBlurSize) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldFarBlurSize) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldSkyFocusDistance) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldVignetteSize))
			{
				return Settings.DepthOfFieldMethod == EDepthOfFieldMethod::DOFM_Gaussian;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldNearTransitionRegion) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldFarTransitionRegion) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldFocalRegion))
			{
				return Settings.DepthOfFieldMethod != EDepthOfFieldMethod::DOFM_CircleDOF;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldDepthBlurAmount) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldDepthBlurRadius) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, DepthOfFieldFstop))
			{
				return Settings.DepthOfFieldMethod == EDepthOfFieldMethod::DOFM_CircleDOF;
			}

			// Parameters supported by both log-average and histogram Auto Exposure
			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, AutoExposureMinBrightness) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, AutoExposureMaxBrightness) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, AutoExposureSpeedUp)       ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, AutoExposureSpeedDown)     ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, AutoExposureBias)          ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, HistogramLogMin)           || 
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, HistogramLogMax))
			{
				return  ( Settings.AutoExposureMethod == EAutoExposureMethod::AEM_Histogram || 
					      Settings.AutoExposureMethod == EAutoExposureMethod::AEM_Basic );
			}

			// Parameters supported by only the histogram AutoExposure
			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, AutoExposureLowPercent)  ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FPostProcessSettings, AutoExposureHighPercent) )
			{
				return Settings.AutoExposureMethod == EAutoExposureMethod::AEM_Histogram;
			}
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(APostProcessVolume, bEnabled))
		{
			return true;
		}

		if (!bEnabled)
		{
			return false;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(APostProcessVolume, BlendRadius))
		{
			if (bUnbound)
			{
				return false;
			}
		}
	}

	return Super::CanEditChange(InProperty);
}

#endif // WITH_EDITOR
