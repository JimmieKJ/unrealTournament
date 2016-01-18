// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

/*-----------------------------------------------------------------------------
	UNameProperty.
-----------------------------------------------------------------------------*/

void UNameProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	FName Temp = *(FName*)PropertyValue;
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += (Temp == NAME_None) 
			? TEXT("FName()") 
			: FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *(Temp.ToString().ReplaceCharWithEscapedChar()));
	}
	else if( !(PortFlags & PPF_Delimited) )
	{
		ValueStr += Temp.ToString();
	}
	else if ( Temp != NAME_None )
	{
		ValueStr += FString::Printf( TEXT("\"%s\""), *Temp.ToString() );
	}
}
const TCHAR* UNameProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	if (!(PortFlags & PPF_Delimited))
	{
		*(FName*)Data = FName(Buffer, FNAME_Add, true);

		// in order to indicate that the value was successfully imported, advance the buffer past the last character that was imported
		Buffer += FCString::Strlen(Buffer);
	}
	else
	{
		FString Temp;
		Buffer = UPropertyHelpers::ReadToken(Buffer, Temp, true);
		if (!Buffer)
			return NULL;

		*(FName*)Data = FName(*Temp, FNAME_Add, true);
	}
	return Buffer;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UNameProperty, UProperty,
	{
	}
);

