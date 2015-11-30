// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNumericDropDown.h"

#define LOCTEXT_NAMESPACE "SequencerHelpers"


class SequencerHelpers
{
public:
	/**
	 * Gets the key areas from the requested node
	 */
	static void GetAllKeyAreas(TSharedPtr<FSequencerDisplayNode> DisplayNode, TSet<TSharedPtr<IKeyArea>>& KeyAreas);

	/**
	* Get descendant nodes
	*/
	static void GetDescendantNodes(TSharedRef<FSequencerDisplayNode> DisplayNode, TSet<TSharedRef<FSequencerDisplayNode> >& Nodes);

	/**
	* Gets all sections from the requested node
	*/
	static void GetAllSections(TSharedRef<FSequencerDisplayNode> DisplayNode, TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections);

	/**
	* Convert time to frame
	*/
	static int32 TimeToFrame(float Time, float FrameRate);

	/**
	* Convert frame to time
	*/
	static float FrameToTime(int32 Frame, float FrameRate);
};


class SequencerSnapValues
{
public:
	SequencerSnapValues() {}

	static const TArray<SNumericDropDown<float>::FNamedValue>& GetSnapValues()
	{
		static TArray<SNumericDropDown<float>::FNamedValue> SnapValues;
		static bool SnapValuesInitialized = false;

		if (!SnapValuesInitialized)
		{
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.001f, LOCTEXT( "Snap_OneThousandth", "0.001" ), LOCTEXT( "SnapDescription_OneThousandth", "Set snap to 1/1000th" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.01f, LOCTEXT( "Snap_OneHundredth", "0.01" ), LOCTEXT( "SnapDescription_OneHundredth", "Set snap to 1/100th" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.1f, LOCTEXT( "Snap_OneTenth", "0.1" ), LOCTEXT( "SnapDescription_OneTenth", "Set snap to 1/10th" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 1.0f, LOCTEXT( "Snap_One", "1" ), LOCTEXT( "SnapDescription_One", "Set snap to 1" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 10.0f, LOCTEXT( "Snap_Ten", "10" ), LOCTEXT( "SnapDescription_Ten", "Set snap to 10" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 100.0f, LOCTEXT( "Snap_OneHundred", "100" ), LOCTEXT( "SnapDescription_OneHundred", "Set snap to 100" ) ) );

			SnapValuesInitialized = true;
		}

		return SnapValues;
	}

	static const TArray<SNumericDropDown<float>::FNamedValue>& GetSecondsSnapValues()
	{
		static TArray<SNumericDropDown<float>::FNamedValue> SecondsSnapValues;
		static bool SecondsSnapValuesInitialized = false;

		if (!SecondsSnapValuesInitialized)
		{
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.001f, LOCTEXT( "Snap_OneThousandthSeconds", "0.001s" ), LOCTEXT( "SnapDescription_OneThousandthSeconds", "Set snap to 1/1000th of a second" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.01f, LOCTEXT( "Snap_OneHundredthSeconds", "0.01s" ), LOCTEXT( "SnapDescription_OneHundredthSeconds", "Set snap to 1/100th of a second" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.1f, LOCTEXT( "Snap_OneTenthSeconds", "0.1s" ), LOCTEXT( "SnapDescription_OneTenthSeconds", "Set snap to 1/10th of a second" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1.0f, LOCTEXT( "Snap_OneSeconds", "1s" ), LOCTEXT( "SnapDescription_OneSeconds", "Set snap to 1 second" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 10.0f, LOCTEXT( "Snap_TenSeconds", "10s" ), LOCTEXT( "SnapDescription_TenSeconds", "Set snap to 10 seconds" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 100.0f, LOCTEXT( "Snap_OneHundredSeconds", "100s" ), LOCTEXT( "SnapDescription_OneHundredSeconds", "Set snap to 100 seconds" ) ) );

			SecondsSnapValuesInitialized = true;
		}

		return SecondsSnapValues;
	}

	static const TArray<SNumericDropDown<float>::FNamedValue>& GetFrameRateSnapValues()
	{
		static TArray<SNumericDropDown<float>::FNamedValue> FrameRateSnapValues;
		static bool FrameRateSnapValuesInitialized = false;

		if (!FrameRateSnapValuesInitialized)
		{
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.066667f, LOCTEXT( "Snap_15Fps", "15 fps" ), LOCTEXT( "SnapDescription_15Fps", "Set snap to 15 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.041667f, LOCTEXT( "Snap_24Fps", "24 fps (film)" ), LOCTEXT( "SnapDescription_24Fps", "Set snap to 24 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.04f, LOCTEXT( "Snap_25Fps", "25 fps (PAL/25)" ), LOCTEXT( "SnapDescription_25Fps", "Set snap to 25 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.033367, LOCTEXT( "Snap_29.97Fps", "29.97 fps (NTSC/30)" ), LOCTEXT( "SnapDescription_29.97Fps", "Set snap to 29.97 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.033334f, LOCTEXT( "Snap_30Fps", "30 fps" ), LOCTEXT( "SnapDescription_30Fps", "Set snap to 30 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.020834f, LOCTEXT( "Snap_48Fps", "48 fps" ), LOCTEXT( "SnapDescription_48Fps", "Set snap to 48 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.02f, LOCTEXT( "Snap_50Fps", "50 fps (PAL/50)" ), LOCTEXT( "SnapDescription_50Fps", "Set snap to 50 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.016683f, LOCTEXT( "Snap_50.94Fps", "50.94 fps (NTSC/60)" ), LOCTEXT( "SnapDescription_50.94Fps", "Set snap to 50.94 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.016667f, LOCTEXT( "Snap_60Fps", "60 fps" ), LOCTEXT( "SnapDescription_60Fps", "Set snap to 60 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.008334f, LOCTEXT( "Snap_120Fps", "120 fps" ), LOCTEXT( "SnapDescription_120Fps", "Set snap to 120 fps" ) ) );

			FrameRateSnapValuesInitialized = true;
		}

		return FrameRateSnapValues;
	}

	static const TArray<SNumericDropDown<float>::FNamedValue>& GetTimeSnapValues()
	{
		static TArray<SNumericDropDown<float>::FNamedValue> TimeSnapValues;
		static bool TimeSnapValuesInitialized = false;
		if (!TimeSnapValuesInitialized)
		{
			TimeSnapValues.Append(GetSecondsSnapValues());
			TimeSnapValues.Append(GetFrameRateSnapValues());

			TimeSnapValuesInitialized = true;
		}
		return TimeSnapValues;
	}

	static bool IsTimeSnapIntervalFrameRate(float InFrameRate)
	{
		for (auto FrameRateSnapValue : GetFrameRateSnapValues())
		{
			if (FMath::IsNearlyEqual(InFrameRate, FrameRateSnapValue.GetValue()))
			{
				return true;
			}
		}
		return false;
	}
};


#undef LOCTEXT_NAMESPACE
