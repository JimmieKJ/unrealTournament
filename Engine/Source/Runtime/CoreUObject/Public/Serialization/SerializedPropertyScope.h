// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncPackage.h: Unreal async loading definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/** Helper class to set and restore serialized property on an archive */
class FSerializedPropertyScope
{
	FArchive& Ar;
	UProperty* Property;
	UProperty* OldProperty;
#if WITH_EDITORONLY_DATA
	void PushEditorOnlyProperty();
	void PopEditorOnlyProperty();
#endif
public:
	FSerializedPropertyScope(FArchive& InAr, UProperty* InProperty)
		: Ar(InAr)
		, Property(InProperty)
	{
		OldProperty = Ar.GetSerializedProperty();
		Ar.SetSerializedProperty(Property);
#if WITH_EDITORONLY_DATA
		PushEditorOnlyProperty();
#endif
	}
	~FSerializedPropertyScope()
	{
#if WITH_EDITORONLY_DATA
		PopEditorOnlyProperty();
#endif
		Ar.SetSerializedProperty(OldProperty);
	}
};
