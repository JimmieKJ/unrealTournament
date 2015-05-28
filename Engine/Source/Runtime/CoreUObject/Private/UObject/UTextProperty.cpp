// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "UObject/UTextProperty.h"
#include "PropertyHelper.h"

bool UTextProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	const TCppType ValueA = GetPropertyValue(A);
	if ( B )
	{
		const TCppType ValueB = GetPropertyValue(B);

		if ( ValueA.IsCultureInvariant() != ValueB.IsCultureInvariant() || ValueA.IsTransient() != ValueB.IsTransient() )
		{
			//A culture variant text is never equal to a culture invariant text
			//A transient text is never equal to a non-transient text
			return false;
		}
		
		if ( ValueA.IsCultureInvariant() == ValueB.IsCultureInvariant() || ValueA.IsTransient() == ValueB.IsTransient() )
		{
			//Culture invariant text don't have a namespace/key so we compare the source string
			//Transient text don't have a namespace/key or source so we compare the display string
			return FTextInspector::GetDisplayString(ValueA) == FTextInspector::GetDisplayString(ValueB);
		}

		if (GIsEditor)
		{
			return FTextInspector::GetSourceString(ValueA)->Compare(*FTextInspector::GetSourceString(ValueB), ESearchCase::CaseSensitive) == 0;
		}
		else
		{
			return	FTextInspector::GetNamespace(ValueA)	==	FTextInspector::GetNamespace(ValueB) &&
					FTextInspector::GetKey(ValueA)			==	FTextInspector::GetKey(ValueB);
		}
	}

	return FTextInspector::GetDisplayString(ValueA).IsEmpty();
}

void UTextProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	const TCppType PropertyValue = GetPropertyValue(Value);
	if ( Ar.IsSaving() && Ar.IsPersistent() && PropertyValue.IsTransient() )
	{
		// Convert to a string and back, to get it set as an invariant message instead of transient. If it stayed transient, error messages recursively expand
		FString ErrorMessage = FText::Format( FText::SerializationFailureError, FText::FromString( FTextInspector::GetDisplayString(PropertyValue) ) ).ToString();
		UE_LOG( LogProperty, Warning, TEXT("%s"), *ErrorMessage);
		SetPropertyValue(Value, FText::FromString(ErrorMessage));
	}

	Ar << *GetPropertyValuePtr(Value);
}

void UTextProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	const FText& TextValue = GetPropertyValue(PropertyValue);
	const FString& StringValue = FTextInspector::GetDisplayString(TextValue);
	if( !(PortFlags & PPF_Delimited) )
	{
		ValueStr += StringValue;
	}
	else if ( StringValue.Len() > 0 )
	{
		ValueStr += FString::Printf( TEXT("\"%s\""), *(StringValue.ReplaceCharWithEscapedChar()) );
	}
}

const TCHAR* UTextProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	FText* Text = GetPropertyValuePtr(Data);

	FString BufferAsString;
	if( !(PortFlags & PPF_Delimited) )
	{
		BufferAsString = FString(Buffer);

		// in order to indicate that the value was successfully imported, advance the buffer past the last character that was imported
		Buffer += FCString::Strlen(Buffer);
	}
	else
	{
		// require quoted string here
		if (*Buffer != TCHAR('"'))
		{
			ErrorText->Logf(TEXT("Missing opening '\"' in string property value: %s"), Buffer);
			return NULL;
		}

		const TCHAR* Start = Buffer;
		FString Temp;
		Buffer = UPropertyHelpers::ReadToken(Buffer, Temp);

		if (Buffer == NULL)
		{
			return NULL;
		}

		if (Buffer > Start && Buffer[-1] != TCHAR('"'))
		{
			ErrorText->Logf(TEXT("Missing terminating '\"' in string property value: %s"), Start);
			return NULL;
		}

		BufferAsString = FString(Temp);
	}

	*Text = FText::FromString(FString(BufferAsString));

	return Buffer;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UTextProperty, UProperty,
{
}
);