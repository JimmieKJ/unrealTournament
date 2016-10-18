// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PropertyTag.h"

/*-----------------------------------------------------------------------------
FPropertyTag
-----------------------------------------------------------------------------*/

// Constructors.
FPropertyTag::FPropertyTag()
	: Type      (NAME_None)
	, BoolVal   (0)
	, Name      (NAME_None)
	, StructName(NAME_None)
	, EnumName  (NAME_None)
	, InnerType (NAME_None)
	, Size      (0)
	, ArrayIndex(INDEX_NONE)
	, SizeOffset(INDEX_NONE)
	, HasPropertyGuid(0)
{}

FPropertyTag::FPropertyTag( FArchive& InSaveAr, UProperty* Property, int32 InIndex, uint8* Value, uint8* Defaults )
	: Type      (Property->GetID())
	, BoolVal   (0)
	, Name      (Property->GetFName())
	, StructName(NAME_None)
	, EnumName  (NAME_None)
	, InnerType (NAME_None)
	, Size      (0)
	, ArrayIndex(InIndex)
	, SizeOffset(INDEX_NONE)
	, HasPropertyGuid(0)
{
	if (Property)
	{
		// Handle structs.
		if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
		{
			StructName = StructProperty->Struct->GetFName();
			StructGuid = StructProperty->Struct->GetCustomGuid();
		}
		else if (UByteProperty* ByteProp = Cast<UByteProperty>(Property))
		{
			if (ByteProp->Enum != NULL)
			{
				EnumName = ByteProp->Enum->GetFName();
			}
		}
		else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property))
		{
			InnerType = ArrayProp->Inner->GetID();
		}
		else if (UBoolProperty* Bool = Cast<UBoolProperty>(Property))
		{
			BoolVal = Bool->GetPropertyValue(Value);
		}
	}
}

// Set optional property guid
void FPropertyTag::SetPropertyGuid(const FGuid& InPropertyGuid)
{
	if (InPropertyGuid.IsValid())
	{
		PropertyGuid = InPropertyGuid;
		HasPropertyGuid = true;
	}
}

// Serializer.
FArchive& operator<<( FArchive& Ar, FPropertyTag& Tag )
{
	// Name.
	Ar << Tag.Name;
	if ((Tag.Name == NAME_None) || !Tag.Name.IsValid())
	{
		return Ar;
	}

	Ar << Tag.Type;
	if ( Ar.IsSaving() )
	{
		// remember the offset of the Size variable - UStruct::SerializeTaggedProperties will update it after the
		// property has been serialized.
		Tag.SizeOffset = Ar.Tell();
	}
	{
		FArchive::FScopeSetDebugSerializationFlags S(Ar, DSF_IgnoreDiff);
		Ar << Tag.Size << Tag.ArrayIndex;
	}
	// only need to serialize this for structs
	if (Tag.Type == NAME_StructProperty)
	{
		Ar << Tag.StructName;
		if (Ar.UE4Ver() >= VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG)
		{
			Ar << Tag.StructGuid;
		}
	}
	// only need to serialize this for bools
	else if (Tag.Type == NAME_BoolProperty)
	{
		Ar << Tag.BoolVal;
	}
	// only need to serialize this for bytes
	else if (Tag.Type == NAME_ByteProperty)
	{
		Ar << Tag.EnumName;
	}
	// only need to serialize this for arrays
	else if (Tag.Type == NAME_ArrayProperty)
	{
		if (Ar.UE4Ver() >= VAR_UE4_ARRAY_PROPERTY_INNER_TAGS)
		{
			Ar << Tag.InnerType;
		}
	}
	// Property tags to handle renamed blueprint properties effectively.
	if (Ar.UE4Ver() >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG)
	{
		Ar << Tag.HasPropertyGuid;
		if (Tag.HasPropertyGuid)
		{
			Ar << Tag.PropertyGuid;
		}
	}

	return Ar;
}

// Property serializer.
void FPropertyTag::SerializeTaggedProperty( FArchive& Ar, UProperty* Property, uint8* Value, uint8* Defaults )
{
	if (Property->GetClass() == UBoolProperty::StaticClass())
	{
		UBoolProperty* Bool = (UBoolProperty*)Property;
		if (Ar.IsLoading())
		{
			Bool->SetPropertyValue(Value, BoolVal != 0);
		}
	}
	else
	{
		FSerializedPropertyScope SerializedProperty(Ar, Property);
		Property->SerializeItem( Ar, Value, Defaults );
	}
}
