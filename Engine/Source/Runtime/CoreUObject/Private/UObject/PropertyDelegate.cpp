// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PropertyHelper.h"
#include "LinkerPlaceholderFunction.h"

/*-----------------------------------------------------------------------------
	UDelegateProperty.
-----------------------------------------------------------------------------*/

void UDelegateProperty::InstanceSubobjects(void* Data, void const* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	for( int32 i=0; i<ArrayDim; i++ )
	{
		FScriptDelegate& DestDelegate = ((FScriptDelegate*)Data)[i];
		UObject* CurrentUObject = DestDelegate.GetUObject();

		if (CurrentUObject)
		{
			UObject *Template = NULL;

			if (DefaultData)
			{
				FScriptDelegate& DefaultDelegate = ((FScriptDelegate*)DefaultData)[i];
				Template = DefaultDelegate.GetUObject();
			}

			UObject* NewUObject = InstanceGraph->InstancePropertyValue(Template, CurrentUObject, Owner, HasAnyPropertyFlags(CPF_Transient), false, true);
			DestDelegate.BindUFunction(NewUObject, DestDelegate.GetFunctionName());
		}
	}
}

bool UDelegateProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	bool bResult = false;

	FScriptDelegate* DA = (FScriptDelegate*)A;
	FScriptDelegate* DB = (FScriptDelegate*)B;
	
	if( DB == NULL )
	{
		bResult = DA->GetFunctionName() == NAME_None;
	}
	else if ( DA->GetFunctionName() == DB->GetFunctionName() )
	{
		if ( DA->GetUObject() == DB->GetUObject() )
		{
			bResult = true;
		}
		else if	((DA->GetUObject() == NULL || DB->GetUObject() == NULL)
			&&	(PortFlags&PPF_DeltaComparison) != 0)
		{
			bResult = true;
		}
	}

	return bResult;
}


void UDelegateProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	Ar << *GetPropertyValuePtr(Value);
}


bool UDelegateProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	// Do not allow replication of delegates, as there is no way to make this secure (it allows the execution of any function in any object, on the remote client/server)
	return 1;
}


FString UDelegateProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	FString UnmangledFunctionName = SignatureFunction->GetName().LeftChop( FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX ).Len() );
	if (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_CustomTypeName))
	{
		UnmangledFunctionName += TEXT("__SinglecastDelegate");
	}
	return FString(TEXT("F")) + UnmangledFunctionName;
}

FString UDelegateProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

void UDelegateProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	FScriptDelegate* ScriptDelegate = (FScriptDelegate*)PropertyValue;
	check(ScriptDelegate != NULL);
	bool bDelegateHasValue = ScriptDelegate->GetFunctionName() != NAME_None;
	ValueStr += FString::Printf( TEXT("%s.%s"),
		ScriptDelegate->GetUObject() != NULL ? *ScriptDelegate->GetUObject()->GetName() : TEXT("(null)"),
		*ScriptDelegate->GetFunctionName().ToString() );
}


const TCHAR* UDelegateProperty::ImportText_Internal( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	return DelegatePropertyTools::ImportDelegateFromText( *(FScriptDelegate*)PropertyValue, SignatureFunction, Buffer, Parent, ErrorText );
}

void UDelegateProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << SignatureFunction;

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (Ar.IsLoading() || Ar.IsObjectReferenceCollector())
	{
		if (auto PlaceholderFunc = Cast<ULinkerPlaceholderFunction>(SignatureFunction))
		{
			PlaceholderFunc->AddReferencingProperty(this);
		}
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

bool UDelegateProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && (SignatureFunction == ((UDelegateProperty*)Other)->SignatureFunction);
}

void UDelegateProperty::BeginDestroy()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (auto PlaceholderFunc = Cast<ULinkerPlaceholderFunction>(SignatureFunction))
	{
		PlaceholderFunc->RemoveReferencingProperty(this);
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	Super::BeginDestroy();
}


IMPLEMENT_CORE_INTRINSIC_CLASS(UDelegateProperty, UProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(UDelegateProperty, SignatureFunction), TEXT("SignatureFunction"));
	}
);
