// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class UMovieSceneSection;

DECLARE_DELEGATE_TwoParams(FOnEnumSelectionChanged, int32 /*Selection*/, ESelectInfo::Type /*SelectionType*/);

class MOVIESCENETOOLS_API MovieSceneToolHelpers
{
public:

	/**
	 * Trim section at the given time
	 *
	 * @param Sections The sections to trim
	 * @param Time	The time at which to trim
	 * @param bTrimLeft Trim left or trim right
	 */
	static void TrimSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time, bool bTrimLeft);

	/**
	 * Splits sections at the given time
	 *
	 * @param Sections The sections to split
	 * @param Time	The time at which to split
	 */
	static void SplitSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time);

	/**
	 * Parse a shot name into its components.
	 *
	 * @param ShotName The shot name to parse
	 * @param ShotPrefix The parsed shot prefix
	 * @param ShotNumber The parsed shot number
	 * @param TakeNumber The parsed take number
	 * &return Whether the shot name was parsed successfully
	 */
	static bool ParseShotName(const FString& ShotName, FString& ShotPrefix, uint32& ShotNumber, uint32& TakeNumber);

	/**
	 * Compose a shot name given its components.
	 *
	 * @param ShotPrefix The shot prefix to use
	 * @param ShotNumber The shot number to use
	 * @param TakeNumber The take number to use
	 * @return The composed shot name
	 */
	static FString ComposeShotName(const FString& ShotPrefix, uint32 ShotNumber, uint32 TakeNumber);

	/**
	 * Generate a new shot name
	 *
	 * @param AllSections All the sections in the given shot track
	 * @param Time The time to generate the new shot name at
	 * @return The new shot name
	 */
	static FString GenerateNewShotName(const TArray<UMovieSceneSection*>& AllSections, float Time);

	/**
	 * Gather takes - level sequence assets that have the same shot prefix and shot number in the same asset path (directory)
	 * 
	 * @param Section The section to gather takes from
	 * @param TakeNumbers The gathered take numbers
	 * @param CurrentTakeNumber The current take number of the section
	 */
	static void GatherTakes(const UMovieSceneSection* Section, TArray<uint32>& TakeNumbers, uint32& CurrentTakeNumber);


	/**
	 * Get the asset associated with the take number
	 *
	 * @param Section The section to gather the take from
	 * @param TakeNumber The take number to get
	 * @return The asset
	 */
	static UObject* GetTake(const UMovieSceneSection* Section, uint32 TakeNumber);

	/**
	 * Generate a combobox for editing enum values
	 *
	 * @param Enum The enum to make the combobox from
	 * @param CurrentValue The current value to display
	 * @param OnSelectionChanged Delegate fired when selection is changed
	 * @return The new widget
	 */
	static TSharedRef<SWidget> MakeEnumComboBox(const UEnum* Enum, TAttribute<int32> CurrentValue, FOnEnumSelectionChanged OnSelectionChanged, TAttribute<TOptional<uint8>> InIntermediateValue);
};
