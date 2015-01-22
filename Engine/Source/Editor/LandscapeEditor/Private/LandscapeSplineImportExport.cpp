// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeSplineImportExport.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"

#define LOCTEXT_NAMESPACE "Landscape"

FLandscapeSplineTextObjectFactory::FLandscapeSplineTextObjectFactory(FFeedbackContext* InWarningContext /*= GWarn*/)
	: FCustomizableTextObjectFactory(InWarningContext)
{
}

TArray<UObject*> FLandscapeSplineTextObjectFactory::ImportSplines(UObject* InParent, const TCHAR* TextBuffer)
{
	if (FParse::Command(&TextBuffer, TEXT("BEGIN SPLINES")))
	{
		ProcessBuffer(InParent, RF_Transactional, TextBuffer);

		//FParse::Command(&TextBuffer, TEXT("END SPLINES"));
	}

	return OutObjects;
}

void FLandscapeSplineTextObjectFactory::ProcessConstructedObject(UObject* CreatedObject)
{
	OutObjects.Add(CreatedObject);

	CreatedObject->PostEditImport();
}

bool FLandscapeSplineTextObjectFactory::CanCreateClass(UClass* ObjectClass) const
{
	if (ObjectClass == ULandscapeSplineControlPoint::StaticClass() ||
		ObjectClass == ULandscapeSplineSegment::StaticClass())
	{
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
