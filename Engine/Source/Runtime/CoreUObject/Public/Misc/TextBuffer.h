// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextBuffer.h: UObject class for storing text
=============================================================================*/

#pragma once

#include "ObjectBase.h"


/**
 * Implements an object that buffers text.
 *
 * The text is contiguous and, if of nonzero length, is terminated by a NULL at the very last position.
 */
class UTextBuffer
	: public UObject
	, public FOutputDevice
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UTextBuffer,UObject,0,CoreUObject,CASTCLASS_None,COREUOBJECT_API)

public:

	/**
	 * Creates and initializes a new text buffer.
	 *
	 * @param ObjectInitializer - Initialization properties.
	 * @param InText - The initial text.
	 */
	COREUOBJECT_API UTextBuffer (const FObjectInitializer& ObjectInitializer, const TCHAR* InText);


public:

	/**
	 * Gets the buffered text.
	 *
	 * @return The text.
	 */
	const FString& GetText () const
	{
		return Text;
	}


public:

	virtual void Serialize (FArchive& Ar);

	virtual void Serialize (const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category ) override;


private:

	int32 Pos, Top;

	// Holds the text.
	FString Text;
};