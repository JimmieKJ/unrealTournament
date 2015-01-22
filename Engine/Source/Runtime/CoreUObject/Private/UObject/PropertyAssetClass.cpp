// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

/*-----------------------------------------------------------------------------
	UAssetClassProperty.
-----------------------------------------------------------------------------*/

FString UAssetClassProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	return FString::Printf(TEXT("TAssetSubclassOf<%s%s> "),MetaClass->GetPrefixCPP(),*MetaClass->GetName());
}
FString UAssetClassProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("TAssetSubclassOf<%s%s> "),MetaClass->GetPrefixCPP(),*MetaClass->GetName());
	return TEXT("ASSETOBJECT");
}

void UAssetClassProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << MetaClass;

	if( !(MetaClass||HasAnyFlags(RF_ClassDefaultObject)) )
	{
		// If we failed to load the MetaClass and we're not a CDO, that means we relied on a class that has been removed or doesn't exist.
		// The most likely cause for this is either an incomplete recompile, or if content was migrated between games that had native class dependencies
		// that do not exist in this game.  We allow blueprint classes to continue, because compile on load will error out, and stub the class that was using it
		UClass* TestClass = dynamic_cast<UClass*>(GetOwnerStruct());
		if( TestClass && TestClass->HasAllClassFlags(CLASS_Native) && !TestClass->HasAllClassFlags(CLASS_NewerVersionExists) && (TestClass->GetOutermost() != GetTransientPackage()) )
		{
			checkf(false, TEXT("Class property tried to serialize a missing class.  Did you remove a native class and not fully recompile?"));
		}
	}
}

void UAssetClassProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UAssetClassProperty* This = CastChecked<UAssetClassProperty>(InThis);
	Collector.AddReferencedObject( This->MetaClass, This );
	Super::AddReferencedObjects( This, Collector );
}

bool UAssetClassProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && (MetaClass == ((UAssetClassProperty*)Other)->MetaClass);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UAssetClassProperty, UAssetObjectProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(UAssetClassProperty, MetaClass), TEXT("MetaClass"));
	}
);
