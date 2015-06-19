// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

/*-----------------------------------------------------------------------------
	UWeakObjectProperty.
-----------------------------------------------------------------------------*/

FString UWeakObjectProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	if (PropertyFlags & CPF_AutoWeak)
	{
		return FString::Printf( TEXT("TAutoWeakObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName() );
	}
	return FString::Printf( TEXT("TWeakObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName() );
}

FString UWeakObjectProperty::GetCPPTypeForwardDeclaration() const
{
	return FString::Printf(TEXT("class %s%s;"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
}

FString UWeakObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	if (PropertyFlags & CPF_AutoWeak)
	{
		ExtendedTypeText = FString::Printf(TEXT("TAutoWeakObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
		return TEXT("AUTOWEAKOBJECT");
	}
	ExtendedTypeText = FString::Printf(TEXT("TWeakObjectPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("WEAKOBJECT");
}

void UWeakObjectProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	UObject* ObjectValue = GetObjectPropertyValue(Value);
	Ar << *(FWeakObjectPtr*)Value;
	if ((Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences()) && ObjectValue != GetObjectPropertyValue(Value))
	{
		CheckValidObject(Value);
	}
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UWeakObjectProperty, UObjectPropertyBase,
	{
	}
);

