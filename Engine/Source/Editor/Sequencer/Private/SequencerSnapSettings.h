// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerSnapSettings.generated.h"

/** Serializable snapping options for sequencer. */
UCLASS(config=EditorUserSettings)
class USequencerSnapSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	/** Gets whether or not snapping is enabled. */
	bool GetIsSnapEnabled() const;
	/** Sets whether or not snapping is enabled. */
	void SetIsSnapEnabled(bool InbIsEnabled);

	/** Gets the time in seconds used for interval snapping. */
	float GetSnapInterval() const;
	/** Sets the time in seconds used for interval snapping. */
	void SetSnapInterval(float InSnapInterval);

	/** Gets whether or not to snap keys to the interval. */
	bool GetSnapKeysToInterval() const;
	/** Sets whether or not to snap keys to the interval. */
	void SetSnapKeysToInterval(bool InbSnapKeysToInterval);

	/** Gets whether or not to snap keys to other keys. */
	bool GetSnapKeysToKeys() const;
	/** Sets whether or not to snap keys to other keys. */
	void SetSnapKeysToKeys(bool InbSnapKeysToKeys);

	/** Gets whether or not to snap sections to the interval. */
	bool GetSnapSectionsToInterval() const;
	/** Sets whether or not to snap sections to the interval. */
	void SetSnapSectionsToInterval(bool InbSnapSectionsToInterval);

	/** Gets whether or not to snap sections to other sections. */
	bool GetSnapSectionsToSections() const;
	/** sets whether or not to snap sections to other sections. */
	void SetSnapSectionsToSections( bool InbSnapSectionsToSections );

	/** Gets whether or not to snap the play time to the interval while scrubbing. */
	bool GetSnapPlayTimeToInterval() const;
	/** Sets whether or not to snap the play time to the interval while scrubbing. */
	void SetSnapPlayTimeToInterval(bool InbSnapPlayTimeToInterval);

	/** Snaps a time value in seconds to the currently selected interval. */
	float SnapToInterval( float InValue ) const;

protected:
	UPROPERTY( config )
	bool bIsEnabled;

	UPROPERTY( config )
	float SnapInterval;

	UPROPERTY( config )
	bool bSnapKeysToInterval;

	UPROPERTY( config )
	bool bSnapKeysToKeys;

	UPROPERTY( config )
	bool bSnapSectionsToInterval;

	UPROPERTY( config )
	bool bSnapSectionsToSections;

	UPROPERTY( config )
	bool bSnapPlayTimeToInterval;
};