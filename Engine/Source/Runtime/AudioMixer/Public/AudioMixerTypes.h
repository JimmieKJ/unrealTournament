// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Audio {

	namespace EAudioMixerPlatformApi
	{
		enum Type
		{
			Wasapi,
			XAudio2,
			Ngs2,
			CoreAudio,
			OpenAL,
			Html5,
			Null,
		};
	}

	namespace EAudioMixerStreamDataFormat
	{
		enum Type
		{
			Unknown,
			Float,
			Double,
			Int16,
			Int24,
			Int32,
			Unsupported
		};
	}

	namespace EAudioMixerChannel
	{
		/** Enumeration values represent sound file or speaker channel types. */
		enum Type
		{
			FrontLeft,
			FrontRight,
			FrontCenter,
			LowFrequency,
			BackLeft,
			BackRight,
			FrontLeftOfCenter,
			FrontRightOfCenter,
			BackCenter,
			SideLeft,
			SideRight,
			TopCenter,
			TopFrontLeft,
			TopFrontCenter,
			TopFrontRight,
			TopBackLeft,
			TopBackCenter,
			TopBackRight,
			Unused,
			ChannelTypeCount
		};

		static const int32 MaxSupportedChannel = EAudioMixerChannel::TopCenter;

		inline const TCHAR* ToString(EAudioMixerChannel::Type InType)
		{
			switch (InType)
			{
				case FrontLeft:				return TEXT("FrontLeft");
				case FrontRight:			return TEXT("FrontRight");
				case FrontCenter:			return TEXT("FrontCenter");
				case LowFrequency:			return TEXT("LowFrequency");
				case BackLeft:				return TEXT("BackLeft");
				case BackRight:				return TEXT("BackRight");
				case FrontLeftOfCenter:		return TEXT("FrontLeftOfCenter");
				case FrontRightOfCenter:	return TEXT("FrontRightOfCenter");
				case BackCenter:			return TEXT("BackCenter");
				case SideLeft:				return TEXT("SideLeft");
				case SideRight:				return TEXT("SideRight");
				case TopCenter:				return TEXT("TopCenter");
				case TopFrontLeft:			return TEXT("TopFrontLeft");
				case TopFrontCenter:		return TEXT("TopFrontCenter");
				case TopFrontRight:			return TEXT("TopFrontRight");
				case TopBackLeft:			return TEXT("TopBackLeft");
				case TopBackCenter:			return TEXT("TopBackCenter");
				case TopBackRight:			return TEXT("TopBackRight");

				default:
				checkf(false, TEXT("Unsupport channel type"));
				return TEXT("UNSUPPORTED");
			}
		}
	}

	/**
	 * EAudioOutputStreamState
	 * Specifies the state of the output audio stream.
	 */
	namespace EAudioOutputStreamState
	{
		enum Type
		{
			/* The audio stream is shutdown or not uninitialized. */
			Closed,
		
			/* The audio stream is open but not running. */
			Open,

			/** The audio stream is open but stopped. */
			Stopped,
		
			/** The audio output stream is stopping. */
			Stopping,

			/** The audio output stream is open and running. */
			Running,
		};
	}

}
