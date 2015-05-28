// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

UFbxAnimSequenceImportData::UFbxAnimSequenceImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bImportCustomAttribute(true)
{
	
}

UFbxAnimSequenceImportData* UFbxAnimSequenceImportData::GetImportDataForAnimSequence(UAnimSequence* AnimSequence, UFbxAnimSequenceImportData* TemplateForCreation)
{
	check(AnimSequence);

	UFbxAnimSequenceImportData* ImportData = Cast<UFbxAnimSequenceImportData>(AnimSequence->AssetImportData);
	if ( !ImportData )
	{
		ImportData = NewObject<UFbxAnimSequenceImportData>(AnimSequence, NAME_None, RF_NoFlags, TemplateForCreation);

		// Try to preserve the source file path if possible
		if ( AnimSequence->AssetImportData != NULL )
		{
			ImportData->SourceFilePath = AnimSequence->AssetImportData->SourceFilePath;
			ImportData->SourceFileTimestamp = AnimSequence->AssetImportData->SourceFileTimestamp;
		}

		AnimSequence->AssetImportData = ImportData;
	}

	return ImportData;
}

bool UFbxAnimSequenceImportData::CanEditChange(const UProperty* InProperty) const
{
	bool bMutable = Super::CanEditChange(InProperty);
	UObject* Outer = GetOuter();
	if(Outer && bMutable)
	{
		// Let the FbxImportUi object handle the editability of our properties
		bMutable = Outer->CanEditChange(InProperty);
	}
	return bMutable;
}
