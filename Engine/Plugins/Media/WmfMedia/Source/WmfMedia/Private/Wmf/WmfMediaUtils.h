// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


namespace WmfMedia
{
	/** 
	 * Convert a FOURCC code to string.
	 *
	 * @param Fourcc The code to convert.
	 * @return The corresponding string.
	 */
	FString FourccToString(unsigned long Fourcc);

	/**
	 * Convert a Windows GUID to string.
	 *
	 * @param Guid The GUID to convert.
	 * @return The corresponding string.
	 */
	FString GuidToString(const GUID& Guid);

	/**
	 * Convert a media major type to string.
	 *
	 * @param MajorType The major type GUID to convert.
	 * @return The corresponding string.
	 */
	FString MajorTypeToString(const GUID& MajorType);

	/**
	 * Convert an WMF HRESULT code to string.
	 *
	 * @param Result The result code to convert.
	 * @return The corresponding string.
	 */
	FString ResultToString(HRESULT Result);

	/**
	 * Convert a WMF session event to string.
	 *
	 * @param Event The event code to convert.
	 * @return The corresponding string.
	 */
	FString SessionEventToString(MediaEventType Event);

	/**
	 * Convert a media sub-type to string.
	 *
	 * @param SubType The sub-type GUID to convert.
	 * @return The corresponding string.
	 */
	FString SubTypeToString(const GUID& SubType);
}


#include "HideWindowsPlatformTypes.h"
