// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"

USequencerSnapSettings::USequencerSnapSettings( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	bIsEnabled = true;
	SnapInterval = .05f;
	bSnapKeysToInterval = true;
	bSnapKeysToKeys = true;
	bSnapSectionsToInterval = true;
	bSnapSectionsToSections = true;
	bSnapPlayTimeToInterval = true;
}

bool USequencerSnapSettings::GetIsSnapEnabled() const
{
	return bIsEnabled;
}

void USequencerSnapSettings::SetIsSnapEnabled(bool InbIsEnabled)
{
	if ( bIsEnabled != InbIsEnabled )
	{
		bIsEnabled = InbIsEnabled;
		SaveConfig();
	}
}

float USequencerSnapSettings::GetSnapInterval() const
{
	return SnapInterval;
}

void USequencerSnapSettings::SetSnapInterval(float InSnapInterval)
{
	if ( SnapInterval != InSnapInterval )
	{
		SnapInterval = InSnapInterval;
		SaveConfig();
	}
}

bool USequencerSnapSettings::GetSnapKeysToInterval() const
{
	return bSnapKeysToInterval;
}

void USequencerSnapSettings::SetSnapKeysToInterval(bool InbSnapKeysToInterval)
{
	if ( bSnapKeysToInterval != InbSnapKeysToInterval )
	{
		bSnapKeysToInterval = InbSnapKeysToInterval;
		SaveConfig();
	}
}

bool USequencerSnapSettings::GetSnapKeysToKeys() const
{
	return bSnapKeysToKeys;
}

void USequencerSnapSettings::SetSnapKeysToKeys(bool InbSnapKeysToKeys)
{
	if ( bSnapKeysToKeys != InbSnapKeysToKeys )
	{
		bSnapKeysToKeys = InbSnapKeysToKeys;
		SaveConfig();
	}
}

bool USequencerSnapSettings::GetSnapSectionsToInterval() const
{
	return bSnapSectionsToInterval;
}

void USequencerSnapSettings::SetSnapSectionsToInterval(bool InbSnapSectionsToInterval)
{
	if ( bSnapSectionsToInterval != InbSnapSectionsToInterval )
	{
		bSnapSectionsToInterval = InbSnapSectionsToInterval;
		SaveConfig();
	}
}

bool USequencerSnapSettings::GetSnapSectionsToSections() const
{
	return bSnapSectionsToSections;
}

void USequencerSnapSettings::SetSnapSectionsToSections( bool InbSnapSectionsToSections )
{
	if ( bSnapSectionsToSections != InbSnapSectionsToSections )
	{
		bSnapSectionsToSections = InbSnapSectionsToSections;
		SaveConfig();
	}
}

bool USequencerSnapSettings::GetSnapPlayTimeToInterval() const
{
	return bSnapPlayTimeToInterval;
}

void USequencerSnapSettings::SetSnapPlayTimeToInterval(bool InbSnapPlayTimeToInterval)
{
	if ( bSnapPlayTimeToInterval != InbSnapPlayTimeToInterval )
	{
		bSnapPlayTimeToInterval = InbSnapPlayTimeToInterval;
		SaveConfig();
	}
}

float USequencerSnapSettings::SnapToInterval(float InValue) const
{
	return SnapInterval > 0 
		? FMath::RoundToInt(InValue / SnapInterval) * SnapInterval
		: InValue;
}
