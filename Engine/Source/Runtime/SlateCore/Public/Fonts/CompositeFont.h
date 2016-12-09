// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/GCObject.h"
#include "UObject/Class.h"
#include "CompositeFont.generated.h"

class UFontBulkData;

UENUM()
enum class EFontHinting : uint8
{
	/** Use the default hinting specified in the font. */
	Default,
	/** Force the use of an automatic hinting algorithm. */
	Auto,
	/** Force the use of an automatic light hinting algorithm, optimized for non-monochrome displays. */
	AutoLight,
	/** Force the use of an automatic hinting algorithm optimized for monochrome displays. */
	Monochrome,
	/** Do not use hinting. */
	None,
};

UENUM()
enum class EFontLoadingPolicy : uint8
{
	/** Pre-load the entire font into memory. This will consume more memory, however there will be zero file-IO when rendering glyphs within the font. */
	PreLoad,
	/** Stream the font from disk. This will consume less memory, however there will be file-IO when rendering glyphs, which may cause hitches under certain circumstances or on certain platforms. */
	Stream,
};

/** Payload data describing an individual font in a typeface. Keep this lean as it's also used as a key! */
USTRUCT()
struct SLATECORE_API FFontData
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor */
	FFontData();

#if WITH_EDITORONLY_DATA
	/** Construct the raw data from a font face asset */
	explicit FFontData(const UObject* const InFontFaceAsset);
#endif // WITH_EDITORONLY_DATA

	/** Construct the raw data from a filename and the font data attributes */
	FFontData(FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy);

	/** Construct the raw font data from a filename, and the data associated with that file */
	DEPRECATED(4.15, "FFontData no longer uses UFontBulkData. Update your code to provide just the filename.")
	FFontData(FString InFontFilename, const UFontBulkData* const InBulkData, const EFontHinting InHinting);

	/** Get the filename of the font to use. This may not actually exist on disk in editor builds and we should load the face buffer instead. */
	const FString& GetFontFilename() const;

	/** Get the hinting algorithm to use with the font. */
	EFontHinting GetHinting() const;

	/** Get the enum controlling how this font should be loaded at runtime. */
	EFontLoadingPolicy GetLoadingPolicy() const;

#if WITH_EDITORONLY_DATA
	/** Get the data buffer containing the data for the current font face. */
	bool GetFontFaceData(TArray<uint8>& OutFontFaceData) const;

	/** Get the font face asset used by this data (if any). */
	const UObject* GetFontFaceAsset() const;

	/** True if this object contains any legacy data that needs to be upgraded PostLoad by calling the functions below (in order). */
	bool HasLegacyData() const;

	/** Upgrade v1 font data to v2 bulk data. */
	void ConditionalUpgradeFontDataToBulkData(UObject* InOuter);

	/** Upgrade v2 bulk data to v3 font face. */
	void ConditionalUpgradeBulkDataToFontFace(UObject* InOuter, UClass* InFontFaceClass, const FName InFontFaceName);
#endif // WITH_EDITORONLY_DATA

	/** Check to see whether this font data is equal to the other font data */
	bool operator==(const FFontData& Other) const;

	/** Check to see whether this font data is not equal to the other font data */
	bool operator!=(const FFontData& Other) const;

	/** Get the type hash for this font data */
	friend inline uint32 GetTypeHash(const FFontData& Key)
	{
		uint32 KeyHash = 0;

#if WITH_EDITORONLY_DATA
		if (Key.FontFaceAsset)
		{
			KeyHash = HashCombine(KeyHash, GetTypeHash(Key.FontFaceAsset));
		}
		else
#endif
		{
			KeyHash = HashCombine(KeyHash, GetTypeHash(Key.FontFilename));
			KeyHash = HashCombine(KeyHash, GetTypeHash(Key.Hinting));
			KeyHash = HashCombine(KeyHash, GetTypeHash(Key.LoadingPolicy));
		}

		return KeyHash;
	}

	/** Handle serialization for this struct. */
	bool Serialize(FArchive& Ar);
	friend FArchive& operator<<(FArchive& Ar, FFontData& InFontData)
	{
		InFontData.Serialize(Ar);
		return Ar;
	}

	/** Called by FStandaloneCompositeFont to prevent our objects from being GCd */
	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	/**
	 * The filename of the font to use.
	 * This variable is ignored if we have a font face asset, and is set to the .ufont file in a cooked build.
	 */
	UPROPERTY()
	FString FontFilename;

	/**
	 * The hinting algorithm to use with the font.
	 * This variable is ignored if we have a font face asset, and is synchronized with the font face asset on load in a cooked build.
	 */
	UPROPERTY()
	EFontHinting Hinting;

	/**
	 * Enum controlling how this font should be loaded at runtime. See the enum for more explanations of the options.
	 * This variable is ignored if we have a font face asset, and is synchronized with the font face asset on load in a cooked build.
	 */
	UPROPERTY()
	EFontLoadingPolicy LoadingPolicy;

#if WITH_EDITORONLY_DATA
	/**
	 * Font data v3. This points to a font face asset.
	 */
	UPROPERTY()
	const UObject* FontFaceAsset;

	/**
	 * Legacy font data v2. This used to be where font data was stored prior to font face assets. 
	 * This can be removed once we no longer support loading packages older than FEditorObjectVersion::AddedFontFaceAssets (as can UFontBulkData itself).
	 */
	UPROPERTY()
	const UFontBulkData* BulkDataPtr_DEPRECATED;

	/**
	 * Legacy font data v1. This used to be where font data was stored prior to font bulk data.
	 * This can be removed once we no longer support loading packages older than VER_UE4_SLATE_BULK_FONT_DATA.
	 */
	UPROPERTY()
	TArray<uint8> FontData_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};

template<>
struct TStructOpsTypeTraits<FFontData> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

/** A single entry in a typeface */
USTRUCT()
struct SLATECORE_API FTypefaceEntry
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor */
	FTypefaceEntry()
	{
	}

	/** Construct the entry from a name */
	FTypefaceEntry(const FName& InFontName)
		: Name(InFontName)
	{
	}

	/** Construct the entry from a filename and the font data attributes */
	FTypefaceEntry(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy)
		: Name(InFontName)
		, Font(MoveTemp(InFontFilename), InHinting, InLoadingPolicy)
	{
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/** Construct the entry from a filename, and the data associated with that file */
	DEPRECATED(4.15, "FTypefaceEntry no longer uses UFontBulkData. Update your code to provide just the filename.")
	FTypefaceEntry(const FName& InFontName, FString InFontFilename, const UFontBulkData* const InBulkData, const EFontHinting InHinting)
		: Name(InFontName)
		, Font(MoveTemp(InFontFilename), InBulkData, InHinting)
	{
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS; /** Do not remove this semi-colon; it exists to stop UHT failing to parse any properties that may follow this macro */

	/** Name used to identify this font within its typeface */
	UPROPERTY()
	FName Name;

	/** Raw font data for this font */
	UPROPERTY()
	FFontData Font;
};

/** Definition for a typeface (a family of fonts) */
USTRUCT()
struct SLATECORE_API FTypeface
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor */
	FTypeface()
	{
	}

	/** Convenience constructor for when your font family only contains a single font */
	FTypeface(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy)
	{
		Fonts.Emplace(InFontName, MoveTemp(InFontFilename), InHinting, InLoadingPolicy);
	}

	/** Convenience constructor for when your font family only contains a single font */
	DEPRECATED(4.15, "FTypeface no longer uses UFontBulkData. Update your code to provide just the filename.")
	FTypeface(const FName& InFontName, FString InFontFilename, const UFontBulkData* const InBulkData, const EFontHinting InHinting)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Fonts.Emplace(FTypefaceEntry(InFontName, MoveTemp(InFontFilename), InBulkData, InHinting));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Append a new font into this family */
	FTypeface& AppendFont(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy)
	{
		Fonts.Emplace(InFontName, MoveTemp(InFontFilename), InHinting, InLoadingPolicy);
		return *this;
	}

	/** Append a new font into this family */
	DEPRECATED(4.15, "FTypeface no longer uses UFontBulkData. Update your code to provide just the filename.")
	FTypeface& AppendFont(const FName& InFontName, FString InFontFilename, const UFontBulkData* const InBulkData, const EFontHinting InHinting = EFontHinting::Default)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Fonts.Emplace(FTypefaceEntry(InFontName, MoveTemp(InFontFilename), InBulkData, InHinting));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return *this;
	}

	/** The fonts contained within this family */
	UPROPERTY()
	TArray<FTypefaceEntry> Fonts;
};

USTRUCT()
struct SLATECORE_API FCompositeSubFont
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor */
	FCompositeSubFont()
		: Typeface()
		, CharacterRanges()
		, ScalingFactor(1.0f)
	{
	}

	/** Typeface data for this sub-font */
	UPROPERTY()
	FTypeface Typeface;

	/** Array of character ranges for which this sub-font should be used */
	UPROPERTY()
	TArray<FInt32Range> CharacterRanges;

	/** Amount to scale this sub-font so that it better matches the size of the default font */
	UPROPERTY()
	float ScalingFactor;

#if WITH_EDITORONLY_DATA
	/** Name of this sub-font. Only used by the editor UI as a convenience to let you state the purpose of the font family. */
	UPROPERTY()
	FName EditorName;
#endif
};

USTRUCT()
struct SLATECORE_API FCompositeFont
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor */
	FCompositeFont()
		: DefaultTypeface()
		, SubTypefaces()
		, HistoryRevision(0)
	{
	}

	/** Convenience constructor for when your composite font only contains a single font */
	FCompositeFont(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy)
		: DefaultTypeface(InFontName, MoveTemp(InFontFilename), InHinting, InLoadingPolicy)
		, SubTypefaces()
		, HistoryRevision(0)
	{
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/** Convenience constructor for when your composite font only contains a single font */
	DEPRECATED(4.15, "FCompositeFont no longer uses UFontBulkData. Update your code to provide just the filename.")
	FCompositeFont(const FName& InFontName, FString InFontFilename, const UFontBulkData* const InBulkData, const EFontHinting InHinting = EFontHinting::Default)
		: DefaultTypeface(InFontName, MoveTemp(InFontFilename), InBulkData, InHinting)
		, SubTypefaces()
		, HistoryRevision(0)
	{
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS; /** Do not remove this semi-colon; it exists to stop UHT failing to parse any properties that may follow this macro */

	/** Call this when the composite font is changed after its initial setup - this allows various caches to update as required */
	void MakeDirty()
	{
		++HistoryRevision;
	}

	/** The default typeface that will be used when not overridden by a sub-typeface */
	UPROPERTY()
	FTypeface DefaultTypeface;

	/** Sub-typefaces to use for a specific set of characters */
	UPROPERTY()
	TArray<FCompositeSubFont> SubTypefaces;

	/** 
	 * Transient value containing the current history ID of this composite font
	 * This should be updated when the composite font is changed (which should happen infrequently as composite fonts are assumed to be mostly immutable) once they've been setup
	 */
	int32 HistoryRevision;
};

/**
 * A version of FCompositeFont that should be used when it's not being embedded within another UObject
 * This derives from FGCObject to ensure that the bulk data objects are referenced correctly 
 */
struct SLATECORE_API FStandaloneCompositeFont : public FCompositeFont, public FGCObject
{
	/** Default constructor */
	FStandaloneCompositeFont()
	{
	}

	/** Convenience constructor for when your composite font only contains a single font */
	FStandaloneCompositeFont(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy)
		: FCompositeFont(InFontName, MoveTemp(InFontFilename), InHinting, InLoadingPolicy)
	{
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/** Convenience constructor for when your composite font only contains a single font */
	DEPRECATED(4.15, "FStandaloneCompositeFont no longer uses UFontBulkData. Update your code to provide just the filename.")
	FStandaloneCompositeFont(const FName& InFontName, FString InFontFilename, const UFontBulkData* const InBulkData, const EFontHinting InHinting = EFontHinting::Default)
		: FCompositeFont(InFontName, MoveTemp(InFontFilename), InBulkData, InHinting)
	{
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS; /** Do not remove this semi-colon; it exists to stop UHT failing to parse any properties that may follow this macro */

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};
