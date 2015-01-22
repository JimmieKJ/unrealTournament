// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "CompositeFont.h"

FFontData::FFontData()
	: FontFilename()
	, BulkDataPtr(nullptr)
	, Hinting(EFontHinting::Default)
	, FontData_DEPRECATED()
{
}

FFontData::FFontData(FString InFontFilename, const UFontBulkData* const InBulkData, const EFontHinting InHinting)
	: FontFilename(MoveTemp(InFontFilename))
	, BulkDataPtr(InBulkData)
	, Hinting(InHinting)
	, FontData_DEPRECATED()
{
}

void FFontData::SetFont(FString InFontFilename, const UFontBulkData* const InBulkData)
{
	FontFilename = MoveTemp(InFontFilename);
	BulkDataPtr = InBulkData;
}

bool FFontData::operator==(const FFontData& Other) const
{
	return BulkDataPtr == Other.BulkDataPtr && Hinting == Other.Hinting;
}

bool FFontData::operator!=(const FFontData& Other) const
{
	return !(*this == Other);
}

void FStandaloneCompositeFont::AddReferencedObjects(FReferenceCollector& Collector)
{
	for(FTypefaceEntry& TypefaceEntry : DefaultTypeface.Fonts)
	{
		Collector.AddReferencedObject(TypefaceEntry.Font.BulkDataPtr);
	}

	for(FCompositeSubFont& SubFont : SubTypefaces)
	{
		for(FTypefaceEntry& TypefaceEntry : SubFont.Typeface.Fonts)
		{
			Collector.AddReferencedObject(TypefaceEntry.Font.BulkDataPtr);
		}
	}
}
