// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HardwareTargetingPrivatePCH.h"
#include "HardwareTargetingModule.h"
#include "HardwareTargetingSettings.h"
#include "Internationalization.h"
#include "SDecoratedEnumCombo.h"


UHardwareTargetingSettings::UHardwareTargetingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TargetedHardwareClass(EHardwareClass::Unspecified)
	, AppliedTargetedHardwareClass(EHardwareClass::Unspecified)
	, DefaultGraphicsPerformance(EGraphicsPreset::Unspecified)
	, AppliedDefaultGraphicsPerformance(EGraphicsPreset::Unspecified)
{ }


bool UHardwareTargetingSettings::HasPendingChanges() const
{
	if (TargetedHardwareClass == EHardwareClass::Unspecified || DefaultGraphicsPerformance == EGraphicsPreset::Unspecified)
	{
		return false;
	}

	return AppliedTargetedHardwareClass != TargetedHardwareClass || AppliedDefaultGraphicsPerformance != DefaultGraphicsPerformance;
}


void UHardwareTargetingSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	SettingChangedEvent.Broadcast();
}
