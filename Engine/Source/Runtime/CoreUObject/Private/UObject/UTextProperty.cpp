// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	Ar << *GetPropertyValuePtr(Value);
}

void UTextProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	const FText& TextValue = GetPropertyValue(PropertyValue);

	if (PortFlags & PPF_ExportCpp)
	{
		ValueStr += GenerateCppCodeForTextValue(TextValue, FString());
	}
	else if (PortFlags & PPF_PropertyWindow)
	{
		if (PortFlags & PPF_Delimited)
		{
			ValueStr += TEXT("\"");
			ValueStr += TextValue.ToString();
			ValueStr += TEXT("\"");
		}
		else
		{
			ValueStr += TextValue.ToString();
		}
	}
	else
	{
		FTextStringHelper::WriteToString(ValueStr, TextValue, !!(PortFlags & PPF_Delimited));
	}
}

const TCHAR* UTextProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	FText* TextPtr = GetPropertyValuePtr(Data);

	int32 NumCharsRead = 0;
	if (FTextStringHelper::ReadFromString(Buffer, *TextPtr, nullptr, &NumCharsRead, !!(PortFlags & PPF_Delimited)))
	{
		Buffer += NumCharsRead;
		return Buffer;
	}
	
	return nullptr;
}

FString UTextProperty::GenerateCppCodeForTextValue(const FText& InValue, const FString& Indent)
{
	FString CppCode;

	const FString& StringValue = FTextInspector::GetDisplayString(InValue);

	if (InValue.IsEmpty())
	{
		CppCode += TEXT("FText::GetEmpty()");
	}
	else if (InValue.IsCultureInvariant())
	{
		// Produces FText::AsCultureInvariant(TEXT("..."))
		CppCode += TEXT("FText::AsCultureInvariant(");
		CppCode += UStrProperty::ExportCppHardcodedText(StringValue, Indent + TEXT("\t"));
		CppCode += TEXT(")");
	}
	else
	{
		bool bIsLocalized = false;
		FString Namespace;
		FString Key;
		const FString* SourceString = FTextInspector::GetSourceString(InValue);

		if (SourceString && InValue.ShouldGatherForLocalization())
		{
			bIsLocalized = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(FTextInspector::GetSharedDisplayString(InValue), Namespace, Key);
		}

		if (bIsLocalized)
		{
			// Produces FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(TEXT("..."), TEXT("..."), TEXT("..."))
			CppCode += TEXT("FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(\n");

			CppCode += Indent;
			CppCode += TEXT("\t");
			CppCode += UStrProperty::ExportCppHardcodedText(*SourceString, Indent + TEXT("\t\t"));
			CppCode += TEXT(", /* Literal Text */\n");

			CppCode += Indent;
			CppCode += TEXT("\t");
			CppCode += UStrProperty::ExportCppHardcodedText(Namespace, Indent + TEXT("\t\t"));
			CppCode += TEXT(", /* Namespace */\n");

			CppCode += Indent;
			CppCode += TEXT("\t");
			CppCode += UStrProperty::ExportCppHardcodedText(Key, Indent + TEXT("\t\t"));
			CppCode += TEXT(" /* Key */\n");

			CppCode += Indent;
			CppCode += TEXT("\t)");
		}
		else
		{
			// Produces FText::FromString(TEXT("..."))
			CppCode += TEXT("FText::FromString(");
			CppCode += UStrProperty::ExportCppHardcodedText(StringValue, Indent + TEXT("\t"));
			CppCode += TEXT(")");
		}
	}

	return CppCode;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UTextProperty, UProperty,
{
}
);
