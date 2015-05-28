// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerSettings.generated.h"

/** Defines visibility states for the curves in the curve editor. */
UENUM()
namespace ESequencerCurveVisibility
{
	enum Type
	{
		/** All curves should be visible. **/
		AllCurves,
		/** Only curves from selected nodes should be visible. **/
		SelectedCurves,
		/** Only curves which have keyframes should be visible. **/
		AnimatedCurves
	};
}

/** Serializable options for sequencer. */
UCLASS(config=EditorPerProjectUserSettings)
class USequencerSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	DECLARE_MULTICAST_DELEGATE( FOnCurveVisibilityChanged );

	/** Gets whether or not snapping is enabled. */
	bool GetIsSnapEnabled() const;
	/** Sets whether or not snapping is enabled. */
	void SetIsSnapEnabled(bool InbIsSnapEnabled);

	/** Gets the time in seconds used for interval snapping. */
	float GetTimeSnapInterval() const;
	/** Sets the time in seconds used for interval snapping. */
	void SetTimeSnapInterval(float InTimeSnapInterval);

	/** Gets whether or not to snap key times to the interval. */
	bool GetSnapKeyTimesToInterval() const;
	/** Sets whether or not to snap keys to the interval. */
	void SetSnapKeyTimesToInterval(bool InbSnapKeyTimesToInterval);

	/** Gets whether or not to snap keys to other keys. */
	bool GetSnapKeyTimesToKeys() const;
	/** Sets whether or not to snap keys to other keys. */
	void SetSnapKeyTimesToKeys(bool InbSnapKeyTimesToKeys);

	/** Gets whether or not to snap sections to the interval. */
	bool GetSnapSectionTimesToInterval() const;
	/** Sets whether or not to snap sections to the interval. */
	void SetSnapSectionTimesToInterval(bool InbSnapSectionTimesToInterval);

	/** Gets whether or not to snap sections to other sections. */
	bool GetSnapSectionTimesToSections() const;
	/** sets whether or not to snap sections to other sections. */
	void SetSnapSectionTimesToSections( bool InbSnapSectionTimesToSections );

	/** Gets whether or not to snap the play time to the interval while scrubbing. */
	bool GetSnapPlayTimeToInterval() const;
	/** Sets whether or not to snap the play time to the interval while scrubbing. */
	void SetSnapPlayTimeToInterval(bool InbSnapPlayTimeToInterval);

	/** Gets the snapping interval for curve values. */
	float GetCurveValueSnapInterval() const;
	/** Sets the snapping interval for curve values. */
	void SetCurveValueSnapInterval(float InCurveValueSnapInterval);

	/** Gets whether or not to snap curve values to the interval. */
	bool GetSnapCurveValueToInterval() const;
	/** Sets whether or not to snap curve values to the interval. */
	void SetSnapCurveValueToInterval(bool InbSnapCurveValueToInterval);

	/** Gets whether or not the 'Clean View' is enabled. In 'Clean View' mode only global tracks are displayed when no filter is applied. */
	bool GetIsUsingCleanView() const;
	/** Sets whether or not the 'Clean View' is enabled. In 'Clean View' mode only global tracks are displayed when no filter is applied. */
	void SetIsUsingCleanView(bool InbIsUsingCleanView);

	/** Gets whether or not the curve editor should be shown. */
	bool GetShowCurveEditor() const;
	/** Sets whether or not the curve editor should be shown. */
	void SetShowCurveEditor(bool InbShowCurveEditor);

	/** Gets whether or not to show curve tool tips in the curve editor. */
	bool GetShowCurveEditorCurveToolTips() const;
	/** Sets whether or not to show curve tool tips in the curve editor. */
	void SetShowCurveEditorCurveToolTips(bool InbShowCurveEditorCurveToolTips);

	/** Gets the current curve visibility. */
	ESequencerCurveVisibility::Type GetCurveVisibility() const;
	/** Sets the current curve visibility. */
	void SetCurveVisibility(ESequencerCurveVisibility::Type InCurveVisibility);

	/** Snaps a time value in seconds to the currently selected interval. */
	float SnapTimeToInterval(float InTimeValue) const;

	/** Gets the multicast delegate which is run whenever the CurveVisibility value changes. */
	FOnCurveVisibilityChanged* GetOnCurveVisibilityChanged();

protected:
	UPROPERTY( config )
	bool bIsSnapEnabled;

	UPROPERTY( config )
	float TimeSnapInterval;

	UPROPERTY( config )
	bool bSnapKeyTimesToInterval;

	UPROPERTY( config )
	bool bSnapKeyTimesToKeys;

	UPROPERTY( config )
	bool bSnapSectionTimesToInterval;

	UPROPERTY( config )
	bool bSnapSectionTimesToSections;

	UPROPERTY( config )
	bool bSnapPlayTimeToInterval;

	UPROPERTY( config )
	float CurveValueSnapInterval;

	UPROPERTY( config )
	bool bSnapCurveValueToInterval;

	UPROPERTY( config )
	bool bIsUsingCleanView;

	UPROPERTY( config )
	bool bShowCurveEditor;

	UPROPERTY( config )
	bool bShowCurveEditorCurveToolTips;

	UPROPERTY( config )
	TEnumAsByte<ESequencerCurveVisibility::Type> CurveVisibility;

	FOnCurveVisibilityChanged OnCurveVisibilityChanged;
};