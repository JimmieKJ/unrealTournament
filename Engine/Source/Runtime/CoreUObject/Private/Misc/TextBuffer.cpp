// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextBuffer.cpp: UObject class for storing text
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "Misc/TextBuffer.h"

IMPLEMENT_CORE_INTRINSIC_CLASS(UTextBuffer, UObject, { });


/* UTextBuffer constructors
 *****************************************************************************/

UTextBuffer::UTextBuffer(const TCHAR* InText)
	: UObject(FObjectInitializer::Get())
	, Text(InText)
{}

UTextBuffer::UTextBuffer (const FObjectInitializer& ObjectInitializer, const TCHAR* InText)
	: UObject(ObjectInitializer)
	, Text(InText)
{}


/* FOutputDevice interface
 *****************************************************************************/

void UTextBuffer::Serialize (const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	Text += (TCHAR*)Data;
}


/* UObject interface
 *****************************************************************************/

void UTextBuffer::Serialize (FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Pos << Top << Text;
}