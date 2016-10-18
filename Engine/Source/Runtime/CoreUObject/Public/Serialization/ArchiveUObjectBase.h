// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*====================================================================================
	ArchiveUObjectBase.h: Implements the FArchiveUObject class for serializing UObjects
======================================================================================*/

#pragma once

/**
 * Base FArchive for serializing UObjects. Supports FLazyObjectPtr and FAssetPtr serialization.
 */
class COREUOBJECT_API FArchiveUObject : public FArchive
{
public:

	using FArchive::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<< (class FLazyObjectPtr& Value) override;
	virtual FArchive& operator<< (class FAssetPtr& Value) override;
	virtual FArchive& operator<< (struct FStringAssetReference& Value) override;
	//~ End FArchive Interface
};

class UProperty;

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
