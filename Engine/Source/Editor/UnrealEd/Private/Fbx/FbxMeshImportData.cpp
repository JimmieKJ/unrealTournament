// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "../Classes/Factories/FbxMeshImportData.h"

UFbxMeshImportData::UFbxMeshImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NormalImportMethod = FBXNIM_ComputeNormals;
	NormalGenerationMethod = EFBXNormalGenerationMethod::MikkTSpace;
}

bool UFbxMeshImportData::CanEditChange(const UProperty* InProperty) const
{
	bool bMutable = Super::CanEditChange(InProperty);
	UObject* Outer = GetOuter();
	if(Outer && bMutable)
	{
		// Let the parent object handle the editability of our properties
		bMutable = Outer->CanEditChange(InProperty);
	}

	static FName NormalGenerationMethod("NormalGenerationMethod");
	if( bMutable && InProperty->GetFName() == NormalGenerationMethod )
	{
		// Normal generation method is ignored if we are importing both tangents and normals
		return NormalImportMethod == FBXNIM_ImportNormalsAndTangents ? false : true;
	}


	return bMutable;
}