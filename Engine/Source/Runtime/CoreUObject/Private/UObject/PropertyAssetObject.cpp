// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/StringAssetReference.h"
#include "UObject/AssetPtr.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

/*-----------------------------------------------------------------------------
	UAssetObjectProperty.
-----------------------------------------------------------------------------*/

FString UAssetObjectProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerNativeTypeName) const
{
	ensure(!InnerNativeTypeName.IsEmpty());
	return FString::Printf(TEXT("TAssetPtr<%s>"), *InnerNativeTypeName);
}
FString UAssetObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("TAssetPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("ASSETOBJECT");
}

FString UAssetObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FName UAssetObjectProperty::GetID() const
{
	return NAME_AssetObjectProperty;
}

// this is always shallow, can't see that we would want it any other way
bool UAssetObjectProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	FAssetPtr ObjectA = A ? *((FAssetPtr*)A) : FAssetPtr();
	FAssetPtr ObjectB = B ? *((FAssetPtr*)B) : FAssetPtr();

	return ObjectA.GetUniqueID() == ObjectB.GetUniqueID();
}

void UAssetObjectProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want asset pointers to keep objects from being garbage collected
	// Allow persistent archives so they can keep track of string references. (e.g. FArchiveSaveTagImports)
	if( !Ar.IsObjectReferenceCollector() || Ar.IsModifyingWeakAndStrongReferences() || Ar.IsPersistent() )
	{
		FAssetPtr OldValue = *(FAssetPtr*)Value;
		Ar << *(FAssetPtr*)Value;

		if (Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences()) 
		{
			if (OldValue.GetUniqueID() != ((FAssetPtr*)Value)->GetUniqueID())
			{
				CheckValidObject(Value);
			}
		}
	}
}

void UAssetObjectProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	FAssetPtr& AssetPtr = *(FAssetPtr*)PropertyValue;

	FStringAssetReference ID;
	UObject *Object = AssetPtr.Get();

	if (Object)
	{
		// Use object in case name has changed.
		ID = FStringAssetReference(Object);
	}
	else
	{
		ID = AssetPtr.GetUniqueID();
	}

	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FStringAssetReference(TEXT(\"%s\"))"), *ID.ToString().ReplaceCharWithEscapedChar());
		return;
	}

	if (!ID.ToString().IsEmpty())
	{
		ValueStr += ID.ToString();
	}
	else
	{
		ValueStr += TEXT("None");
	}
}

const TCHAR* UAssetObjectProperty::ImportText_Internal( const TCHAR* InBuffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	FAssetPtr& AssetPtr = *(FAssetPtr*)Data;

	FString NewPath;
	const TCHAR* Buffer = InBuffer;
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken( Buffer, NewPath, 1 );
	if( !NewBuffer )
	{
		return NULL;
	}
	Buffer = NewBuffer;
	if( NewPath==TEXT("None") )
	{
		AssetPtr = NULL;
	}
	else
	{
		if( *Buffer == TCHAR('\'') )
		{
			// A ' token likely means we're looking at an asset string in the form "Texture2d'/Game/UI/HUD/Actions/Barrel'" and we need to read and append the path part
			// We have to skip over the first ' as UPropertyHelpers::ReadToken doesn't read single-quoted strings correctly, but does read a path correctly
			NewPath += *Buffer++; // Append the leading '
			NewBuffer = UPropertyHelpers::ReadToken( Buffer, NewPath, 1 );
			if( !NewBuffer )
			{
				return NULL;
			}
			Buffer = NewBuffer;
			if( *Buffer != TCHAR('\'') )
			{
				return NULL;
			}
			NewPath += *Buffer++; // Append the trailing '
		}
		FStringAssetReference ID(NewPath);
		AssetPtr = ID;
	}

	return Buffer;
}

bool UAssetObjectProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	bOutAdvanceProperty = true;

	if (Tag.Type == NAME_ObjectProperty || Tag.Type == NAME_ClassProperty)
	{
		// This property used to be a raw UObjectProperty Foo* but is now a TAssetPtr<Foo>
		UObject* PreviousValue = nullptr;
		Ar << PreviousValue;

		// now copy the value into the object's address space
		FAssetPtr PreviousValueAssetPtr(PreviousValue);
		SetPropertyValue_InContainer(Data, PreviousValueAssetPtr, Tag.ArrayIndex);

		return true;
	}
	else if (Tag.Type == NAME_StructProperty)
	{
		// This property used to be a FStringAssetReference but is now a TAssetPtr<Foo>
		FStringAssetReference PreviousValue;
		// explicitly call Serialize to ensure that the various delegates needed for cooking are fired
		PreviousValue.Serialize(Ar);

		// now copy the value into the object's address space
		FAssetPtr PreviousValueAssetPtr;
		PreviousValueAssetPtr = PreviousValue;
		SetPropertyValue_InContainer(Data, PreviousValueAssetPtr, Tag.ArrayIndex);

		return true;
	}

	return false;
}

UObject* UAssetObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return GetPropertyValue(PropertyValueAddress).Get();
}

void UAssetObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	SetPropertyValue(PropertyValueAddress, TCppType(Value));
}

bool UAssetObjectProperty::AllowCrossLevel() const
{
	return true;
}

uint32 UAssetObjectProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(GetPropertyValue(Src));
}

void UAssetObjectProperty::CopySingleValueToScriptVM(void* Dest, void const* Src) const
{
	CopySingleValue(Dest, Src);
}

void UAssetObjectProperty::CopyCompleteValueToScriptVM(void* Dest, void const* Src) const
{
	CopyCompleteValue(Dest, Src);
}

void UAssetObjectProperty::CopySingleValueFromScriptVM(void* Dest, void const* Src) const
{
	CopySingleValue(Dest, Src);
}

void UAssetObjectProperty::CopyCompleteValueFromScriptVM(void* Dest, void const* Src) const
{
	CopyCompleteValue(Dest, Src);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UAssetObjectProperty, UObjectPropertyBase,
	{
	}
);

