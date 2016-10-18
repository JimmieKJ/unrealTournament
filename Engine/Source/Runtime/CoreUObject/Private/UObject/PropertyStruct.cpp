// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "Archive.h"
#include "PropertyHelper.h"
#include "LinkerPlaceholderBase.h" // for FScopedPlaceholderPropertyTracker

static inline void PreloadInnerStructMembers(UStructProperty* StructProperty)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	uint32 PropagatedLoadFlags = 0;
	if (FLinkerLoad* Linker = StructProperty->GetLinker())
	{
		PropagatedLoadFlags |= (Linker->LoadFlags & LOAD_DeferDependencyLoads);
	}

	if (UScriptStruct* Struct = StructProperty->Struct)
	{
		if (FLinkerLoad* StructLinker = Struct->GetLinker())
		{
			TGuardValue<uint32> LoadFlagGuard(StructLinker->LoadFlags, StructLinker->LoadFlags | PropagatedLoadFlags);
			Struct->RecursivelyPreload();
		}
	}
#else // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	StructProperty->Struct->RecursivelyPreload();
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

/*-----------------------------------------------------------------------------
	UStructProperty.
-----------------------------------------------------------------------------*/

UStructProperty::UStructProperty(ECppProperty, int32 InOffset, uint64 InFlags, UScriptStruct* InStruct)
	: UProperty(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags)
	, Struct(InStruct)
{
	ElementSize = Struct->PropertiesSize;
}

UStructProperty::UStructProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, UScriptStruct* InStruct )
	:	UProperty( ObjectInitializer, EC_CppProperty, InOffset, InFlags )
	,	Struct( InStruct )
{
	ElementSize = Struct->PropertiesSize;
}

int32 UStructProperty::GetMinAlignment() const
{
	return Struct->GetMinAlignment();
}

void UStructProperty::LinkInternal(FArchive& Ar)
{
	// We potentially have to preload the property itself here, if we were the inner of an array property
	if(HasAnyFlags(RF_NeedLoad))
	{
		GetLinker()->Preload(this);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Struct == NULL)
	{
		UE_LOG(LogProperty, Error, TEXT("Struct type unknown for property '%s'; perhaps the USTRUCT() was renamed or deleted?"), *GetFullName());
	}
#endif

	// Preload is required here in order to load the value of Struct->PropertiesSize
	Ar.Preload(Struct);
	if ( !ensure(Struct) )
	{
		Struct = GetFallbackStruct();
	}
	PreloadInnerStructMembers(this);
	
	ElementSize = Align(Struct->PropertiesSize, Struct->GetMinAlignment());
	if (Struct->StructFlags & STRUCT_IsPlainOldData) // if there is nothing to construct or the struct is known to be memcpy-able, then allow memcpy
	{
		PropertyFlags |= CPF_IsPlainOldData;
	}
	if (Struct->StructFlags & STRUCT_NoDestructor)
	{
		PropertyFlags |= CPF_NoDestructor;
	}
	if (Struct->StructFlags & STRUCT_ZeroConstructor)
	{
		PropertyFlags |= CPF_ZeroConstructor;
	}
}

bool UStructProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	return Struct->CompareScriptStruct(A, B, PortFlags);
}

bool UStructProperty::UseBinaryOrNativeSerialization(const FArchive& Ar) const
{
	check(Struct);

	const bool bUseBinarySerialization = Struct->UseBinarySerialization(Ar);
	const bool bUseNativeSerialization = Struct->UseNativeSerialization();
	return bUseBinarySerialization || bUseNativeSerialization;
}

void UStructProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	check(Struct);

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	FScopedPlaceholderPropertyTracker ImportPropertyTracker(this);
#endif

	Struct->SerializeItem(Ar, Value, Defaults);
}

bool UStructProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	//------------------------------------------------
	//	Custom NetSerialization
	//------------------------------------------------
	if (Struct->StructFlags & STRUCT_NetSerializeNative)
	{
		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		check(CppStructOps); // else should not have STRUCT_NetSerializeNative
		check(!Struct->InheritedCppStructOps()); // else should not have STRUCT_NetSerializeNative
		bool bSuccess = true;
		bool bMapped = CppStructOps->NetSerialize(Ar, Map, bSuccess, Data);
		if (!bSuccess)
		{
			UE_LOG(LogProperty, Warning, TEXT("Native NetSerialize %s (%s) failed."), *GetFullName(), *Struct->GetFullName() );
		}
		return bMapped;
	}

	UE_LOG( LogProperty, Fatal, TEXT( "Deprecated code path" ) );

	return 1;
}

void UStructProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	static UScriptStruct* FallbackStruct = GetFallbackStruct();
	
	if (Ar.IsPersistent() && Ar.GetLinker() && Ar.IsLoading() && !Struct)
	{
		// It's necessary to solve circular dependency problems, when serializing the Struct causes linking of the Property.
		Struct = FallbackStruct;
	}

	Ar << Struct;
#if WITH_EDITOR
	if (Ar.IsPersistent() && Ar.GetLinker())
	{
		if (!Struct && Ar.IsLoading())
		{
			UE_LOG(LogProperty, Error, TEXT("UStructProperty::Serialize Loading: Property '%s'. Unknown structure."), *GetFullName());
			Struct = FallbackStruct;
		}
		else if ((FallbackStruct == Struct) && Ar.IsSaving())
		{
			UE_LOG(LogProperty, Error, TEXT("UStructProperty::Serialize Saving: Property '%s'. FallbackStruct structure."), *GetFullName());
		}
	}
#endif // WITH_EDITOR
	if (Struct)
	{
		PreloadInnerStructMembers(this);
	}
	else
	{
		ensure(true);
	}
}
void UStructProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UStructProperty* This = CastChecked<UStructProperty>(InThis);
	Collector.AddReferencedObject( This->Struct, This );
	Super::AddReferencedObjects( This, Collector );
}

#if HACK_HEADER_GENERATOR

bool UStructProperty::HasNoOpConstructor() const
{
	Struct->PrepareCppStructOps();
	UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
	if (CppStructOps && CppStructOps->HasNoopConstructor())
	{
		return true;
	}
	return false;
}

#endif

FString UStructProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	return FString::Printf(TEXT("F%s"), *Struct->GetName());
}

FString UStructProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("struct F%s;"), *Struct->GetName());
}

FString UStructProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = GetCPPType(NULL, CPPF_None);
	return TEXT("STRUCT");
}

void UStructProperty::UStructProperty_ExportTextItem(class UScriptStruct* InStruct, FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope)
{
	int32 Count=0;

	// if this struct is configured to be serialized as a unit, it must be exported as a unit as well.
	if ((InStruct->StructFlags&STRUCT_Atomic) != 0)
	{
		// change DefaultValue to match PropertyValue so that ExportText always exports this item
		DefaultValue = PropertyValue;
	}

	for (TFieldIterator<UProperty> It(InStruct); It; ++It)
	{
		if (It->ShouldPort(PortFlags))
		{
			for (int32 Index=0; Index<It->ArrayDim; Index++)
			{
				FString InnerValue;
				if (It->ExportText_InContainer(Index,InnerValue,PropertyValue,DefaultValue,Parent,PPF_Delimited | PortFlags, ExportRootScope))
				{
					Count++;
					if ( Count == 1 )
					{
						ValueStr += TEXT("(");
					}
					else
					{
						ValueStr += TEXT(",");
					}

					if( It->ArrayDim == 1 )
					{
						ValueStr += FString::Printf( TEXT("%s="), *It->GetName() );
					}
					else
					{
						ValueStr += FString::Printf( TEXT("%s[%i]="), *It->GetName(), Index );
					}
					ValueStr += InnerValue;
				}
			}
		}
	}

	if (Count > 0)
	{
		ValueStr += TEXT(")");
	}
	else
	{
		ValueStr += TEXT("()");
	}
}

void UStructProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	if (Struct->StructFlags & STRUCT_ExportTextItemNative)
	{
		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		check(CppStructOps); // else should not have STRUCT_ExportTextItemNative
		check(!Struct->InheritedCppStructOps()); // else should not have STRUCT_ExportTextItemNative
		if (CppStructOps->ExportTextItem(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope))
		{
			return;
		}
	}

	if (0 != (PortFlags & PPF_ExportCpp))
	{
		return;
	}

	UStructProperty_ExportTextItem(Struct, ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope);
} 

const TCHAR* UStructProperty::ImportText_Internal(const TCHAR* InBuffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText) const
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	FScopedPlaceholderPropertyTracker ImportPropertyTracker(this);
#endif 
	return ImportText_Static(Struct, GetName(), InBuffer, Data, PortFlags, Parent, ErrorText);
}

const TCHAR* UStructProperty::ImportText_Static(UScriptStruct* InStruct, const FString& Name, const TCHAR* InBuffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	auto Struct = InStruct;

	if (Struct->StructFlags & STRUCT_ImportTextItemNative)
	{
		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		check(CppStructOps); // else should not have STRUCT_ImportTextItemNative
		check(!Struct->InheritedCppStructOps()); // else should not have STRUCT_ImportTextItemNative
		if (CppStructOps->ImportTextItem(InBuffer, Data, PortFlags, Parent, ErrorText))
		{
			return InBuffer;
		}
	}
	
	TArray<FDefinedProperty> DefinedProperties;
	// this keeps track of the number of errors we've logged, so that we can add new lines when logging more than one error
	int32 ErrorCount = 0;
	const TCHAR* Buffer = InBuffer;
	if (*Buffer++ == TCHAR('('))
	{
		// Parse all properties.
		while (*Buffer != TCHAR(')'))
		{
			// parse and import the value
			Buffer = ImportSingleProperty(Buffer, Data, Struct, Parent, PortFlags | PPF_Delimited, ErrorText, DefinedProperties);

			// skip any remaining text before the next property value
			SkipWhitespace(Buffer);
			int32 SubCount = 0;
			while ( *Buffer && *Buffer != TCHAR('\r') && *Buffer != TCHAR('\n') &&
					(SubCount > 0 || *Buffer != TCHAR(')')) && (SubCount > 0 || *Buffer != TCHAR(',')) )
			{
				SkipWhitespace(Buffer);
				if (*Buffer == TCHAR('\"'))
				{
					do
					{
						Buffer++;
					} while (*Buffer && *Buffer != TCHAR('\"') && *Buffer != TCHAR('\n') && *Buffer != TCHAR('\r'));

					if (*Buffer != TCHAR('\"'))
					{
						ErrorText->Logf(TEXT("%sImportText (%s): Bad quoted string at: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Name, Buffer);
						return NULL;
					}
				}
				else if( *Buffer == TCHAR('(') )
				{
					SubCount++;
				}
				else if( *Buffer == TCHAR(')') )
				{
					SubCount--;
					if( SubCount < 0 )
					{
						ErrorText->Logf(TEXT("%sImportText (%s): Too many closing parenthesis in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Name, InBuffer);
						return NULL;
					}
				}
				Buffer++;
			}
			if( SubCount > 0 )
			{
				ErrorText->Logf(TEXT("%sImportText(%s): Not enough closing parenthesis in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Name, InBuffer);
				return NULL;
			}

			// Skip comma.
			if( *Buffer==TCHAR(',') )
			{
				// Skip comma.
				Buffer++;
			}
			else if( *Buffer!=TCHAR(')') )
			{
				ErrorText->Logf(TEXT("%sImportText (%s): Missing closing parenthesis: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Name, InBuffer);
				return NULL;
			}

			SkipWhitespace(Buffer);
		}

		// Skip trailing ')'.
		Buffer++;
	}
	else
	{
		ErrorText->Logf(TEXT("%sImportText (%s): Missing opening parenthesis: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Name, InBuffer);
		return NULL;
	}
	return Buffer;
}

void UStructProperty::CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
{
	Struct->CopyScriptStruct(Dest, Src, Count);
}

void UStructProperty::InitializeValueInternal( void* InDest ) const
{
	Struct->InitializeStruct(InDest, ArrayDim);
}

void UStructProperty::ClearValueInternal( void* Data ) const
{
	Struct->ClearScriptStruct(Data, 1); // clear only does one value
}

void UStructProperty::DestroyValueInternal( void* Dest ) const
{
	Struct->DestroyStruct(Dest, ArrayDim);
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the instanced object referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the instanced object referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UStructProperty::InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	for (int32 Index = 0; Index < ArrayDim; Index++)
	{
		Struct->InstanceSubobjectTemplates( (uint8*)Data + ElementSize * Index, DefaultData ? (uint8*)DefaultData + ElementSize * Index : NULL, Struct, Owner, InstanceGraph );
	}
}

bool UStructProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && (Struct == ((UStructProperty*)Other)->Struct);
}

bool UStructProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	bOutAdvanceProperty = false;

	auto CanSerializeFromStructWithDifferentName = [](const FArchive& InAr, const FPropertyTag& PropertyTag, const UStructProperty* StructProperty)
	{
		if (InAr.UE4Ver() < VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG)
		{
			// Old Implementation
			return StructProperty && !StructProperty->UseBinaryOrNativeSerialization(InAr);
		}
		return PropertyTag.StructGuid.IsValid() && StructProperty && StructProperty->Struct && (PropertyTag.StructGuid == StructProperty->Struct->GetCustomGuid());
	};

	if (Struct)
	{
		if ((Struct->StructFlags & STRUCT_SerializeFromMismatchedTag) && (Tag.Type != NAME_StructProperty || (Tag.StructName != Struct->GetFName())) && Struct->StructFlags & STRUCT_SerializeFromMismatchedTag)
		{
			UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
			check(CppStructOps && CppStructOps->HasSerializeFromMismatchedTag()); // else should not have STRUCT_SerializeFromMismatchedTag
			void* DestAddress = ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex);
			if (CppStructOps->SerializeFromMismatchedTag(Tag, Ar, DestAddress))
			{
				bOutAdvanceProperty = true;
			}			
			else
			{
				UE_LOG(LogClass, Warning, TEXT("SerializeFromMismatchedTag failed: Type mismatch in %s of %s - Previous (%s) Current(StructProperty) for package:  %s"), *Tag.Name.ToString(), *GetName(), *Tag.Type.ToString(), *Ar.GetArchiveName());
			}
			return true;
		}
		else if (Tag.Type == NAME_StructProperty && Tag.StructName != Struct->GetFName() && !CanSerializeFromStructWithDifferentName(Ar, Tag, this))
		{
			//handle Vector -> Vector4 upgrades here because using the SerializeFromMismatchedTag system would cause a dependency from Core -> CoreUObject
			if (Tag.StructName == NAME_Vector && Struct->GetFName() == NAME_Vector4)
			{
				void* DestAddress = ContainerPtrToValuePtr<void>(Data, Tag.ArrayIndex);
				FVector OldValue;
				Ar << OldValue;

				//only set X/Y/Z.  The W should already have been set to the property specific default and we don't want to trash it by forcing 0 or 1.
				FVector4* DestValue = (FVector4*)DestAddress;				
				DestValue->X = OldValue.X;
				DestValue->Y = OldValue.Y;
				DestValue->Z = OldValue.Z;
			}
			else
			{
				UE_LOG(LogClass, Warning, TEXT("Property %s of %s has a struct type mismatch (tag %s != prop %s) in package:  %s. If that struct got renamed, add an entry to ActiveStructRedirects."),
					*Tag.Name.ToString(), *GetName(), *Tag.StructName.ToString(), *Struct->GetName(), *Ar.GetArchiveName());
			}

			return true;
		}
	}
	return false;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UStructProperty, UProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(UStructProperty, Struct), TEXT("Struct"));
	}
);
