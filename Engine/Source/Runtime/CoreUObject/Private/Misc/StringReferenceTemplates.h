// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Serializes from mismatched tag.
 *
 * @template_param TypePolicy The policy should provide two things:
 *	- GetTypeName() method that returns registered name for this property type,
 *	- typedef Type, which is a C++ type to serialize if property matched type name.
 * @param Tag Property tag to match type.
 * @param Ar Archive to serialize from.
 */
template <class TypePolicy>
bool SerializeFromMismatchedTagTemplate(FString& Output, const FPropertyTag& Tag, FArchive& Ar)
{
	if (Tag.Type == TypePolicy::GetTypeName())
	{
		typename TypePolicy::Type* ObjPtr = NULL;
		Ar << ObjPtr;
		if (ObjPtr)
		{
			Output = ObjPtr->GetPathName();
		}
		else
		{
			Output = FString();
		}
#if WITH_EDITOR
		if (Ar.IsLoading() && Ar.IsPersistent() && FCoreUObjectDelegates::StringAssetReferenceLoaded.IsBound())
		{
			FCoreUObjectDelegates::StringAssetReferenceLoaded.Execute(Output);
		}
#endif // WITH_EDITOR
		return true;
	}
	else if (Tag.Type == NAME_StrProperty)
	{
		FString String;
		Ar << String;

		Output = String;
#if WITH_EDITOR
		if (Ar.IsLoading() && Ar.IsPersistent() && FCoreUObjectDelegates::StringAssetReferenceLoaded.IsBound())
		{
			FCoreUObjectDelegates::StringAssetReferenceLoaded.Execute(Output);
		}
#endif // WITH_EDITOR
		return true;
	}
	return false;
}