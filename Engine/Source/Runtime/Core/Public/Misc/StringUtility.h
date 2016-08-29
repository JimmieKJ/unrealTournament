// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Char.h"
#include "CString.h"

namespace StringUtility
{
	/**
	 * Unescapes a URI
	 *
	 * @param URLString an escaped string (e.g. File%20Name)
	 *
	 * @return un-escaped string (e.g. "File Name")
	 */
	FString CORE_API UnescapeURI(const FString& URLString);
}

