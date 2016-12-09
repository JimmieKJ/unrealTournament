// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Fonts/CompositeFont.h"
#include "UObject/EditorObjectVersion.h"
#include "Fonts/FontFaceInterface.h"
#include "Templates/Casts.h"
#include "Fonts/FontBulkData.h"

FFontData::FFontData()
	: FontFilename()
	, Hinting(EFontHinting::Default)
	, LoadingPolicy(EFontLoadingPolicy::PreLoad)
#if WITH_EDITORONLY_DATA
	, FontFaceAsset(nullptr)
	, BulkDataPtr_DEPRECATED(nullptr)
	, FontData_DEPRECATED()
#endif // WITH_EDITORONLY_DATA
{
}

#if WITH_EDITORONLY_DATA
FFontData::FFontData(const UObject* const InFontFaceAsset)
	: FontFilename()
	, Hinting(EFontHinting::Default)
	, LoadingPolicy(EFontLoadingPolicy::PreLoad)
	, FontFaceAsset(InFontFaceAsset)
	, BulkDataPtr_DEPRECATED(nullptr)
	, FontData_DEPRECATED()
{
	if (FontFaceAsset)
	{
		CastChecked<const IFontFaceInterface>(FontFaceAsset);
	}
}
#endif // WITH_EDITORONLY_DATA

FFontData::FFontData(FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy)
	: FontFilename(MoveTemp(InFontFilename))
	, Hinting(InHinting)
	, LoadingPolicy(InLoadingPolicy)
#if WITH_EDITORONLY_DATA
	, FontFaceAsset(nullptr)
	, BulkDataPtr_DEPRECATED(nullptr)
	, FontData_DEPRECATED()
#endif // WITH_EDITORONLY_DATA
{
}

FFontData::FFontData(FString InFontFilename, const UFontBulkData* const InBulkData, const EFontHinting InHinting)
	: FontFilename(MoveTemp(InFontFilename))
	, Hinting(InHinting)
	, LoadingPolicy(EFontLoadingPolicy::PreLoad)
#if WITH_EDITORONLY_DATA
	, FontFaceAsset(nullptr)
	, BulkDataPtr_DEPRECATED(nullptr)
	, FontData_DEPRECATED()
#endif // WITH_EDITORONLY_DATA
{
}

const FString& FFontData::GetFontFilename() const
{
#if WITH_EDITORONLY_DATA
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		return FontFace->GetFontFilename();
	}
#endif // WITH_EDITORONLY_DATA
	return FontFilename;
}

EFontHinting FFontData::GetHinting() const
{
#if WITH_EDITORONLY_DATA
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		return FontFace->GetHinting();
	}
#endif // WITH_EDITORONLY_DATA
	return Hinting;
}

EFontLoadingPolicy FFontData::GetLoadingPolicy() const
{
#if WITH_EDITORONLY_DATA
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		return FontFace->GetLoadingPolicy();
	}
#endif // WITH_EDITORONLY_DATA
	return LoadingPolicy;
}

#if WITH_EDITORONLY_DATA
bool FFontData::GetFontFaceData(TArray<uint8>& OutFontFaceData) const
{
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		OutFontFaceData = FontFace->GetFontFaceData();
		return true;
	}
	return false;
}

const UObject* FFontData::GetFontFaceAsset() const
{
	return FontFaceAsset;
}

bool FFontData::HasLegacyData() const
{
	return FontData_DEPRECATED.Num() > 0 || BulkDataPtr_DEPRECATED;
}

void FFontData::ConditionalUpgradeFontDataToBulkData(UObject* InOuter)
{
	if (FontData_DEPRECATED.Num() > 0)
	{
		UFontBulkData* NewBulkData = NewObject<UFontBulkData>(InOuter);
		BulkDataPtr_DEPRECATED = NewBulkData;

		NewBulkData->Initialize(FontData_DEPRECATED.GetData(), FontData_DEPRECATED.Num());
		FontData_DEPRECATED.Empty();
	}
}

void FFontData::ConditionalUpgradeBulkDataToFontFace(UObject* InOuter, UClass* InFontFaceClass, const FName InFontFaceName)
{
	if (BulkDataPtr_DEPRECATED)
	{
		int32 RawBulkDataSizeBytes = 0;
		const void* RawBulkData = BulkDataPtr_DEPRECATED->Lock(RawBulkDataSizeBytes);
		if (RawBulkDataSizeBytes > 0)
		{
			UObject* NewFontFaceAsset = NewObject<UObject>(InOuter, InFontFaceClass, InFontFaceName);
			FontFaceAsset = NewFontFaceAsset;

			IFontFaceInterface* NewFontFace = CastChecked<IFontFaceInterface>(NewFontFaceAsset);
			NewFontFace->InitializeFromBulkData(FontFilename, Hinting, RawBulkData, RawBulkDataSizeBytes);
		}
		BulkDataPtr_DEPRECATED->Unlock();
		BulkDataPtr_DEPRECATED = nullptr;
	}
}
#endif // WITH_EDITORONLY_DATA

bool FFontData::operator==(const FFontData& Other) const
{
#if WITH_EDITORONLY_DATA
	if (FontFaceAsset != Other.FontFaceAsset)
	{
		// Using different assets
		return false;
	}
	if (FontFaceAsset && FontFaceAsset == Other.FontFaceAsset)
	{
		// Using the same asset
		return true;
	}
#endif // WITH_EDITORONLY_DATA

	// Compare inline properties
	return FontFilename == Other.FontFilename
		&& Hinting == Other.Hinting
		&& LoadingPolicy == Other.LoadingPolicy;
}

bool FFontData::operator!=(const FFontData& Other) const
{
	return !(*this == Other);
}

bool FFontData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	if (Ar.CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::AddedFontFaceAssets)
	{
		// Too old, so use the default serialization
		return false;
	}

	bool bIsCooked = Ar.IsCooking();
	Ar << bIsCooked;

	if (bIsCooked)
	{
		// Cooked data uses a more compact format, and doesn't persist the font face asset when loading
		UObject* LocalFontFaceAsset = nullptr;
#if WITH_EDITORONLY_DATA
		LocalFontFaceAsset = const_cast<UObject*>(FontFaceAsset);
#endif // WITH_EDITORONLY_DATA
		Ar << LocalFontFaceAsset;
		
		if (LocalFontFaceAsset)
		{
			if (Ar.IsLoading())
			{
				IFontFaceInterface* LocalFontFace = CastChecked<IFontFaceInterface>(LocalFontFaceAsset);

				// Set the other properties from the font face asset
				FontFilename = LocalFontFace->GetCookedFilename();
				Hinting = LocalFontFace->GetHinting();
				LoadingPolicy = LocalFontFace->GetLoadingPolicy();
			}
		}
		else
		{
			// Only need to serialize the other properties when we lack a font face asset
			Ar << FontFilename;
			Ar << Hinting;
			Ar << LoadingPolicy;
		}
	}
	else
	{
#if WITH_EDITORONLY_DATA
		// Need to make sure the font face asset is tracked as a reference for cooking, despite being in an editor-only property
		if (Ar.IsObjectReferenceCollector())
		{
			UObject* LocalFontFaceAsset = const_cast<UObject*>(FontFaceAsset);
			Ar << LocalFontFaceAsset;
		}
#endif // WITH_EDITORONLY_DATA

		// Uncooked data uses the standard struct serialization to avoid versioning on editor-only data
		UScriptStruct* FontDataStruct = StaticStruct();
		if (FontDataStruct->UseBinarySerialization(Ar))
		{
			FontDataStruct->SerializeBin(Ar, (uint8*)this);
		}
		else
		{
			FontDataStruct->SerializeTaggedProperties(Ar, (uint8*)this, FontDataStruct, nullptr);
		}
	}

	return true;
}

void FFontData::AddReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_EDITORONLY_DATA
	Collector.AddReferencedObject(FontFaceAsset);
	Collector.AddReferencedObject(BulkDataPtr_DEPRECATED);
#endif // WITH_EDITORONLY_DATA
}

void FStandaloneCompositeFont::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FTypefaceEntry& TypefaceEntry : DefaultTypeface.Fonts)
	{
		TypefaceEntry.Font.AddReferencedObjects(Collector);
	}

	for (FCompositeSubFont& SubFont : SubTypefaces)
	{
		for (FTypefaceEntry& TypefaceEntry : SubFont.Typeface.Fonts)
		{
			TypefaceEntry.Font.AddReferencedObjects(Collector);
		}
	}
}
