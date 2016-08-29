// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"

USequencerSettings::USequencerSettings( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AutoKeyMode = EAutoKeyMode::KeyNone;
	bKeyAllEnabled = false;
	bKeyInterpPropertiesOnly = false;
	KeyInterpolation = EMovieSceneKeyInterpolation::Auto;
	SpawnPosition = SSP_Origin;
	bCreateSpawnableCameras = true;
	bShowFrameNumbers = true;
	bShowRangeSlider = false;
	bIsSnapEnabled = true;
	TimeSnapInterval = .05f;
	bSnapKeyTimesToInterval = true;
	bSnapKeyTimesToKeys = true;
	bSnapSectionTimesToInterval = true;
	bSnapSectionTimesToSections = true;
	bSnapPlayTimeToKeys = false;
	bSnapPlayTimeToInterval = true;
	bSnapPlayTimeToDraggedKey = false;
	CurveValueSnapInterval = 10.0f;
	bSnapCurveValueToInterval = true;
	bLabelBrowserVisible = false;
	bRewindOnRecord = true;
	ZoomPosition = ESequencerZoomPosition::SZP_CurrentTime;
	bAutoScrollEnabled = false;
	bShowCurveEditorCurveToolTips = true;
	bLinkCurveEditorTimeRange = false;
	bLooping = false;
	bKeepCursorInPlayRange = true;
	bKeepPlayRangeInSectionBounds = true;
	ZeroPadFrames = 0;
	bShowCombinedKeyframes = true;
	bInfiniteKeyAreas = false;
	bShowChannelColors = false;
	bShowViewportTransportControls = true;
	bAllowPossessionOfPIEViewports = false;
}

EAutoKeyMode USequencerSettings::GetAutoKeyMode() const
{
	return AutoKeyMode;
}

void USequencerSettings::SetAutoKeyMode(EAutoKeyMode InAutoKeyMode)
{
	if ( AutoKeyMode != InAutoKeyMode )
	{
		AutoKeyMode = InAutoKeyMode;
		SaveConfig();
	}
}

bool USequencerSettings::GetKeyAllEnabled() const
{
	return bKeyAllEnabled;
}

void USequencerSettings::SetKeyAllEnabled(bool InbKeyAllEnabled)
{
	if ( bKeyAllEnabled != InbKeyAllEnabled )
	{
		bKeyAllEnabled = InbKeyAllEnabled;
		SaveConfig();
	}
}

bool USequencerSettings::GetKeyInterpPropertiesOnly() const
{
	return bKeyInterpPropertiesOnly;
}

void USequencerSettings::SetKeyInterpPropertiesOnly(bool InbKeyInterpPropertiesOnly)
{
	if ( bKeyInterpPropertiesOnly != InbKeyInterpPropertiesOnly )
	{
		bKeyInterpPropertiesOnly = InbKeyInterpPropertiesOnly;
		SaveConfig();
	}
}

EMovieSceneKeyInterpolation USequencerSettings::GetKeyInterpolation() const
{
	return KeyInterpolation;
}

void USequencerSettings::SetKeyInterpolation(EMovieSceneKeyInterpolation InKeyInterpolation)
{
	if ( KeyInterpolation != InKeyInterpolation)
	{
		KeyInterpolation = InKeyInterpolation;
		SaveConfig();
	}
}

ESequencerSpawnPosition USequencerSettings::GetSpawnPosition() const
{
	return SpawnPosition;
}

void USequencerSettings::SetSpawnPosition(ESequencerSpawnPosition InSpawnPosition)
{
	if ( SpawnPosition != InSpawnPosition)
	{
		SpawnPosition = InSpawnPosition;
		SaveConfig();
	}
}

bool USequencerSettings::GetCreateSpawnableCameras() const
{
	return bCreateSpawnableCameras;
}

void USequencerSettings::SetCreateSpawnableCameras(bool bInCreateSpawnableCameras)
{
	if ( bCreateSpawnableCameras != bInCreateSpawnableCameras)
	{
		bCreateSpawnableCameras = bInCreateSpawnableCameras;
		SaveConfig();
	}
}

bool USequencerSettings::GetShowFrameNumbers() const
{
	return bShowFrameNumbers;
}

void USequencerSettings::SetShowFrameNumbers(bool InbShowFrameNumbers)
{
	if ( bShowFrameNumbers != InbShowFrameNumbers )
	{
		bShowFrameNumbers = InbShowFrameNumbers;
		SaveConfig();
	}
}

bool USequencerSettings::GetShowRangeSlider() const
{
	return bShowRangeSlider;
}

void USequencerSettings::SetShowRangeSlider(bool InbShowRangeSlider)
{
	if ( bShowRangeSlider != InbShowRangeSlider )
	{
		bShowRangeSlider = InbShowRangeSlider;
		SaveConfig();
	}
}

bool USequencerSettings::GetIsSnapEnabled() const
{
	return bIsSnapEnabled;
}

void USequencerSettings::SetIsSnapEnabled(bool InbIsSnapEnabled)
{
	if ( bIsSnapEnabled != InbIsSnapEnabled )
	{
		bIsSnapEnabled = InbIsSnapEnabled;
		SaveConfig();
	}
}

float USequencerSettings::GetTimeSnapInterval() const
{
	return TimeSnapInterval;
}

void USequencerSettings::SetTimeSnapInterval(float InTimeSnapInterval)
{
	if ( TimeSnapInterval != InTimeSnapInterval )
	{
		TimeSnapInterval = InTimeSnapInterval;
		OnTimeSnapIntervalChanged.Broadcast();
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapKeyTimesToInterval() const
{
	return bSnapKeyTimesToInterval;
}

void USequencerSettings::SetSnapKeyTimesToInterval(bool InbSnapKeyTimesToInterval)
{
	if ( bSnapKeyTimesToInterval != InbSnapKeyTimesToInterval )
	{
		bSnapKeyTimesToInterval = InbSnapKeyTimesToInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapKeyTimesToKeys() const
{
	return bSnapKeyTimesToKeys;
}

void USequencerSettings::SetSnapKeyTimesToKeys(bool InbSnapKeyTimesToKeys)
{
	if ( bSnapKeyTimesToKeys != InbSnapKeyTimesToKeys )
	{
		bSnapKeyTimesToKeys = InbSnapKeyTimesToKeys;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapSectionTimesToInterval() const
{
	return bSnapSectionTimesToInterval;
}

void USequencerSettings::SetSnapSectionTimesToInterval(bool InbSnapSectionTimesToInterval)
{
	if ( bSnapSectionTimesToInterval != InbSnapSectionTimesToInterval )
	{
		bSnapSectionTimesToInterval = InbSnapSectionTimesToInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapSectionTimesToSections() const
{
	return bSnapSectionTimesToSections;
}

void USequencerSettings::SetSnapSectionTimesToSections( bool InbSnapSectionTimesToSections )
{
	if ( bSnapSectionTimesToSections != InbSnapSectionTimesToSections )
	{
		bSnapSectionTimesToSections = InbSnapSectionTimesToSections;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapPlayTimeToKeys() const
{
	return bSnapPlayTimeToKeys;
}

void USequencerSettings::SetSnapPlayTimeToKeys(bool InbSnapPlayTimeToKeys)
{
	if ( bSnapPlayTimeToKeys != InbSnapPlayTimeToKeys )
	{
		bSnapPlayTimeToKeys = InbSnapPlayTimeToKeys;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapPlayTimeToInterval() const
{
	return bSnapPlayTimeToInterval;
}

void USequencerSettings::SetSnapPlayTimeToInterval(bool InbSnapPlayTimeToInterval)
{
	if ( bSnapPlayTimeToInterval != InbSnapPlayTimeToInterval )
	{
		bSnapPlayTimeToInterval = InbSnapPlayTimeToInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapPlayTimeToDraggedKey() const
{
	return bSnapPlayTimeToDraggedKey;
}

void USequencerSettings::SetSnapPlayTimeToDraggedKey(bool InbSnapPlayTimeToDraggedKey)
{
	if ( bSnapPlayTimeToDraggedKey != InbSnapPlayTimeToDraggedKey )
	{
		bSnapPlayTimeToDraggedKey = InbSnapPlayTimeToDraggedKey;
		SaveConfig();
	}
}

float USequencerSettings::GetCurveValueSnapInterval() const
{
	return CurveValueSnapInterval;
}

void USequencerSettings::SetCurveValueSnapInterval( float InCurveValueSnapInterval )
{
	if ( CurveValueSnapInterval != InCurveValueSnapInterval )
	{
		CurveValueSnapInterval = InCurveValueSnapInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapCurveValueToInterval() const
{
	return bSnapCurveValueToInterval;
}

void USequencerSettings::SetSnapCurveValueToInterval( bool InbSnapCurveValueToInterval )
{
	if ( bSnapCurveValueToInterval != InbSnapCurveValueToInterval )
	{
		bSnapCurveValueToInterval = InbSnapCurveValueToInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetLabelBrowserVisible() const
{
	return bLabelBrowserVisible;
}

void USequencerSettings::SetLabelBrowserVisible(bool Visible)
{
	if (bLabelBrowserVisible != Visible)
	{
		bLabelBrowserVisible = Visible;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldRewindOnRecord() const
{
	return bRewindOnRecord;
}

void USequencerSettings::SetRewindOnRecord(bool bInRewindOnRecord)
{
	if (bInRewindOnRecord != bRewindOnRecord)
	{
		bRewindOnRecord = bInRewindOnRecord;
		SaveConfig();
	}
}

ESequencerZoomPosition USequencerSettings::GetZoomPosition() const
{
	return ZoomPosition;
}

void USequencerSettings::SetZoomPosition(ESequencerZoomPosition InZoomPosition)
{
	if ( ZoomPosition != InZoomPosition)
	{
		ZoomPosition = InZoomPosition;
		SaveConfig();
	}
}

bool USequencerSettings::GetAutoScrollEnabled() const
{
	return bAutoScrollEnabled;
}

void USequencerSettings::SetAutoScrollEnabled(bool bInAutoScrollEnabled)
{
	if (bAutoScrollEnabled != bInAutoScrollEnabled)
	{
		bAutoScrollEnabled = bInAutoScrollEnabled;
		SaveConfig();
	}
}

bool USequencerSettings::IsLooping() const
{
	return bLooping;
}

void USequencerSettings::SetLooping(bool bInLooping)
{
	if (bLooping != bInLooping)
	{
		bLooping = bInLooping;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldKeepCursorInPlayRange() const
{
	return bKeepCursorInPlayRange;
}

void USequencerSettings::SetKeepCursorInPlayRange(bool bInKeepCursorInPlayRange)
{
	if (bKeepCursorInPlayRange != bInKeepCursorInPlayRange)
	{
		bKeepCursorInPlayRange = bInKeepCursorInPlayRange;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldKeepPlayRangeInSectionBounds() const
{
	return bKeepPlayRangeInSectionBounds;
}

void USequencerSettings::SetKeepPlayRangeInSectionBounds(bool bInKeepPlayRangeInSectionBounds)
{
	if (bKeepPlayRangeInSectionBounds != bInKeepPlayRangeInSectionBounds)
	{
		bKeepPlayRangeInSectionBounds = bInKeepPlayRangeInSectionBounds;
		SaveConfig();
	}
}

bool USequencerSettings::GetShowCurveEditorCurveToolTips() const
{
	return bShowCurveEditorCurveToolTips;
}

void USequencerSettings::SetShowCurveEditorCurveToolTips(bool InbShowCurveEditorCurveToolTips)
{
	if (bShowCurveEditorCurveToolTips != InbShowCurveEditorCurveToolTips)
	{
		bShowCurveEditorCurveToolTips = InbShowCurveEditorCurveToolTips;
		SaveConfig();
	}
}


bool USequencerSettings::GetLinkCurveEditorTimeRange() const
{
	return bLinkCurveEditorTimeRange;
}

void USequencerSettings::SetLinkCurveEditorTimeRange(bool InbLinkCurveEditorTimeRange)
{
	if (bLinkCurveEditorTimeRange != InbLinkCurveEditorTimeRange)
	{
		bLinkCurveEditorTimeRange = InbLinkCurveEditorTimeRange;
		SaveConfig();
	}
}


uint8 USequencerSettings::GetZeroPadFrames() const
{
	return ZeroPadFrames;
}

void USequencerSettings::SetZeroPadFrames(uint8 InZeroPadFrames)
{
	if (ZeroPadFrames != InZeroPadFrames)
	{
		ZeroPadFrames = InZeroPadFrames;
		SaveConfig();
	}
}

bool USequencerSettings::GetShowCombinedKeyframes() const
{
	return bShowCombinedKeyframes;
}

void USequencerSettings::SetShowCombinedKeyframes(bool InbShowCombinedKeyframes)
{
	if (bShowCombinedKeyframes != InbShowCombinedKeyframes)
	{
		bShowCombinedKeyframes = InbShowCombinedKeyframes;
		SaveConfig();
	}
}


bool USequencerSettings::GetInfiniteKeyAreas() const
{
	return bInfiniteKeyAreas;
}

void USequencerSettings::SetInfiniteKeyAreas(bool InbInfiniteKeyAreas)
{
	if (bInfiniteKeyAreas != InbInfiniteKeyAreas)
	{
		bInfiniteKeyAreas = InbInfiniteKeyAreas;
		SaveConfig();
	}
}


bool USequencerSettings::GetShowChannelColors() const
{
	return bShowChannelColors;
}

void USequencerSettings::SetShowChannelColors(bool InbShowChannelColors)
{
	if (bShowChannelColors != InbShowChannelColors)
	{
		bShowChannelColors = InbShowChannelColors;
		SaveConfig();
	}
}

bool USequencerSettings::GetShowViewportTransportControls() const
{
	return bShowViewportTransportControls;
}

void USequencerSettings::SetShowViewportTransportControls(bool bVisible)
{
	if (bShowViewportTransportControls != bVisible)
	{
		bShowViewportTransportControls = bVisible;
		SaveConfig();
	}
}

float USequencerSettings::SnapTimeToInterval( float InTimeValue ) const
{
	return TimeSnapInterval > 0
		? FMath::RoundToInt( InTimeValue / TimeSnapInterval ) * TimeSnapInterval
		: InTimeValue;
}

bool USequencerSettings::ShouldAllowPossessionOfPIEViewports() const
{
	return bAllowPossessionOfPIEViewports;
}

void USequencerSettings::SetAllowPossessionOfPIEViewports(bool bInAllowPossessionOfPIEViewports)
{
	if (bInAllowPossessionOfPIEViewports != bAllowPossessionOfPIEViewports)
	{
		bAllowPossessionOfPIEViewports = bInAllowPossessionOfPIEViewports;
		SaveConfig();
	}
}

USequencerSettings::FOnTimeSnapIntervalChanged& USequencerSettings::GetOnTimeSnapIntervalChanged()
{
	return OnTimeSnapIntervalChanged;
}
