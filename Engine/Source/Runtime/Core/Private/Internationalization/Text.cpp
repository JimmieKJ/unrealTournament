// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "TextData.h"
#include "TextHistory.h"
#include "TextFormatter.h"
#include "TextNamespaceUtil.h"
#include "FastDecimalFormat.h"

#include "DebugSerializationFlags.h"
#include "EditorObjectVersion.h"

//DEFINE_STAT(STAT_TextFormat);

DECLARE_LOG_CATEGORY_EXTERN(LogText, Log, All);
DEFINE_LOG_CATEGORY(LogText);


#define LOCTEXT_NAMESPACE "Core.Text"

bool FTextInspector::ShouldGatherForLocalization(const FText& Text)
{
	return Text.ShouldGatherForLocalization();
}

TOptional<FString> FTextInspector::GetNamespace(const FText& Text)
{
	FTextDisplayStringPtr LocalizedString = Text.TextData->GetLocalizedString();
	if (LocalizedString.IsValid())
	{
		FString Namespace;
		FString Key;
		if (FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(LocalizedString.ToSharedRef(), Namespace, Key))
		{
			return Namespace;
		}
	}
	return TOptional<FString>();
}

TOptional<FString> FTextInspector::GetKey(const FText& Text)
{
	FTextDisplayStringPtr LocalizedString = Text.TextData->GetLocalizedString();
	if (LocalizedString.IsValid())
	{
		FString Namespace;
		FString Key;
		if (FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(LocalizedString.ToSharedRef(), Namespace, Key))
		{
			return Key;
		}
	}
	return TOptional<FString>();
}

const FString* FTextInspector::GetSourceString(const FText& Text)
{
	return &Text.GetSourceString();
}

const FString& FTextInspector::GetDisplayString(const FText& Text)
{
	return Text.TextData->GetDisplayString();
}

const FTextDisplayStringRef FTextInspector::GetSharedDisplayString(const FText& Text)
{
	// todo: calling PersistText here probably isn't the right thing to do, however it avoids having to make an external API change at this point
	Text.TextData->PersistText();
	return Text.TextData->GetLocalizedString().ToSharedRef();
}

uint32 FTextInspector::GetFlags(const FText& Text)
{
	return Text.Flags;
}

// These default values have been duplicated to the KismetTextLibrary functions for Blueprints. Please replicate any changes there!
FNumberFormattingOptions::FNumberFormattingOptions()
	: UseGrouping(true)
	, RoundingMode(ERoundingMode::HalfToEven)
	, MinimumIntegralDigits(1)
	, MaximumIntegralDigits(DBL_MAX_10_EXP + DBL_DIG + 1)
	, MinimumFractionalDigits(0)
	, MaximumFractionalDigits(3)
{

}

FArchive& operator<<(FArchive& Ar, FNumberFormattingOptions& Value)
{
	Ar << Value.UseGrouping;

	int8 RoundingModeInt8 = (int8)Value.RoundingMode;
	Ar << RoundingModeInt8;
	Value.RoundingMode = (ERoundingMode)RoundingModeInt8;

	Ar << Value.MinimumIntegralDigits;
	Ar << Value.MaximumIntegralDigits;
	Ar << Value.MinimumFractionalDigits;
	Ar << Value.MaximumFractionalDigits;

	return Ar;
}

uint32 GetTypeHash( const FNumberFormattingOptions& Key )
{
	uint32 Hash = 0;
	Hash = HashCombine(Hash, GetTypeHash(Key.UseGrouping));
	Hash = HashCombine(Hash, GetTypeHash(Key.RoundingMode));
	Hash = HashCombine(Hash, GetTypeHash(Key.MinimumIntegralDigits));
	Hash = HashCombine(Hash, GetTypeHash(Key.MaximumIntegralDigits));
	Hash = HashCombine(Hash, GetTypeHash(Key.MinimumFractionalDigits));
	Hash = HashCombine(Hash, GetTypeHash(Key.MaximumFractionalDigits));
	return Hash;
}

bool FNumberFormattingOptions::IsIdentical( const FNumberFormattingOptions& Other ) const
{
	return UseGrouping == Other.UseGrouping
		&& RoundingMode == Other.RoundingMode
		&& MinimumIntegralDigits == Other.MinimumIntegralDigits
		&& MaximumIntegralDigits == Other.MaximumIntegralDigits
		&& MinimumFractionalDigits == Other.MinimumFractionalDigits
		&& MaximumFractionalDigits == Other.MaximumFractionalDigits;
}

const FNumberFormattingOptions& FNumberFormattingOptions::DefaultWithGrouping()
{
	static const FNumberFormattingOptions Options = FNumberFormattingOptions().SetUseGrouping(true);
	return Options;
}

const FNumberFormattingOptions& FNumberFormattingOptions::DefaultNoGrouping()
{
	static const FNumberFormattingOptions Options = FNumberFormattingOptions().SetUseGrouping(false);
	return Options;
}

bool FText::bEnableErrorCheckingResults = ENABLE_TEXT_ERROR_CHECKING_RESULTS;
bool FText::bSuppressWarnings = false;

FText::FText()
	: TextData(GetEmpty().TextData)
	, Flags(0)
{
}

FText::FText( EInitToEmptyString )
	: TextData(new TLocalizedTextData<FTextHistory_Base>(MakeShareable(new FString())))
	, Flags(0)
{
}

#if PLATFORM_WINDOWS && defined(__clang__)
CORE_API const FText& FText::GetEmpty() // @todo clang: Workaround for missing symbol export
{
	static const FText StaticEmptyText = FText(FText::EInitToEmptyString::Value);
	return StaticEmptyText;
}
#endif

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FText::FText(const FText& Other) = default;
	FText::FText(FText&& Other) = default;
	FText& FText::operator=(const FText& Other) = default;
	FText& FText::operator=(FText&& Other) = default;
#else
FText::FText(const FText& Source)
	: TextData(Source.TextData)
	, Flags(Source.Flags)
{
}

FText::FText(FText&& Source)
	: TextData(MoveTemp(Source.TextData))
	, Flags(Source.Flags)
{
}

FText& FText::operator=(const FText& Source)
{
	if (this != &Source)
	{
		TextData = Source.TextData;
		Flags = Source.Flags;
	}
	return *this;
}

FText& FText::operator=(FText&& Source)
{
	if (this != &Source)
	{
		TextData = MoveTemp(Source.TextData);
		Flags = Source.Flags;
	}
	return *this;
}
#endif

FText::FText( TSharedRef<ITextData, ESPMode::ThreadSafe> InTextData )
	: TextData(MoveTemp(InTextData))
	, Flags(0)
{
}

FText::FText( FString&& InSourceString )
	: TextData(new TGeneratedTextData<FTextHistory_Base>(FString(InSourceString)))
	, Flags(0)
{
	TextData->SetTextHistory(FTextHistory_Base(MoveTemp(InSourceString)));
}

FText::FText( FString&& InSourceString, FTextDisplayStringRef InDisplayString )
	: TextData(new TLocalizedTextData<FTextHistory_Base>(MoveTemp(InDisplayString)))
	, Flags(0)
{
	TextData->SetTextHistory(FTextHistory_Base(MoveTemp(InSourceString)));
}

FText::FText( FString&& InSourceString, const FString& InNamespace, const FString& InKey, uint32 InFlags )
	: TextData(new TLocalizedTextData<FTextHistory_Base>(FTextLocalizationManager::Get().GetDisplayString(InNamespace, InKey, &InSourceString)))
	, Flags(InFlags)
{
	TextData->SetTextHistory(FTextHistory_Base(MoveTemp(InSourceString)));
}

bool FText::IsEmpty() const
{
	return TextData->GetDisplayString().IsEmpty();
}

bool FText::IsEmptyOrWhitespace() const
{
	const FString& DisplayString = TextData->GetDisplayString();
	if (DisplayString.IsEmpty())
	{
		return true;
	}

	for( const TCHAR Character : DisplayString )
	{
		if (!IsWhitespace(Character))
		{
			return false;
		}
	}

	return true;
}

FText FText::TrimPreceding( const FText& InText )
{
	FString TrimmedString = InText.ToString();
	{
		int32 StartPos = 0;
		while ( StartPos < TrimmedString.Len() )
		{
			if( !FText::IsWhitespace( TrimmedString[StartPos] ) )
			{
				break;
			}

			++StartPos;
		}

		TrimmedString = TrimmedString.Right( TrimmedString.Len() - StartPos );
	}

	FText NewText = FText( MoveTemp( TrimmedString ) );

	if (!GIsEditor)
	{
		if( (NewText.Flags & ETextFlag::CultureInvariant) != 0 )
		{
			NewText.Flags |= ETextFlag::Transient;
		}
		else
		{
			NewText.Flags |= ETextFlag::CultureInvariant;
		}
	}

	return NewText;
}

FText FText::TrimTrailing( const FText& InText )
{
	FString TrimmedString = InText.ToString();
	{
		int32 EndPos = TrimmedString.Len() - 1;
		while( EndPos >= 0 )
		{
			if( !FText::IsWhitespace( TrimmedString[EndPos] ) )
			{
				break;
			}

			EndPos--;
		}

		TrimmedString = TrimmedString.Left( EndPos + 1 );
	}

	FText NewText = FText( MoveTemp ( TrimmedString ) );

	if (!GIsEditor)
	{
		if( (NewText.Flags & ETextFlag::CultureInvariant) != 0 )
		{
			NewText.Flags |= ETextFlag::Transient;
		}
		else
		{
			NewText.Flags |= ETextFlag::CultureInvariant;
		}
	}

	return NewText;
}

FText FText::TrimPrecedingAndTrailing( const FText& InText )
{
	FString TrimmedString = InText.ToString();
	{
		int32 StartPos = 0;
		while ( StartPos < TrimmedString.Len() )
		{
			if( !FText::IsWhitespace( TrimmedString[StartPos] ) )
			{
				break;
			}

			++StartPos;
		}

		int32 EndPos = TrimmedString.Len() - 1;
		while( EndPos >= 0 )
		{
			if( !FText::IsWhitespace( TrimmedString[EndPos] ) )
			{
				break;
			}

			--EndPos;
		}

		const int32 Len = (EndPos + 1) - StartPos;
		TrimmedString = TrimmedString.Mid( StartPos, Len );
	}

	FText NewText = FText( MoveTemp( TrimmedString ) );

	if (!GIsEditor)
	{
		if( (NewText.Flags & ETextFlag::CultureInvariant) != 0 )
		{
			NewText.Flags |= ETextFlag::Transient;
		}
		else
		{
			NewText.Flags |= ETextFlag::CultureInvariant;
		}
	}

	return NewText;
}

FText FText::Format(FTextFormat Fmt, FFormatArgumentValue v1)
{
	FFormatOrderedArguments Arguments;
	Arguments.Reserve(1);
	Arguments.Add(MoveTemp(v1));
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(Arguments), false, false);
}

FText FText::Format(FTextFormat Fmt, FFormatArgumentValue v1, FFormatArgumentValue v2)
{
	FFormatOrderedArguments Arguments;
	Arguments.Reserve(2);
	Arguments.Add(MoveTemp(v1));
	Arguments.Add(MoveTemp(v2));
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(Arguments), false, false);
}

FText FText::Format(FTextFormat Fmt, FFormatArgumentValue v1, FFormatArgumentValue v2, FFormatArgumentValue v3)
{
	FFormatOrderedArguments Arguments;
	Arguments.Reserve(3);
	Arguments.Add(MoveTemp(v1));
	Arguments.Add(MoveTemp(v2));
	Arguments.Add(MoveTemp(v3));
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(Arguments), false, false);
}

FText FText::Format(FTextFormat Fmt, FFormatArgumentValue v1, FFormatArgumentValue v2, FFormatArgumentValue v3, FFormatArgumentValue v4)
{
	FFormatOrderedArguments Arguments;
	Arguments.Reserve(4);
	Arguments.Add(MoveTemp(v1));
	Arguments.Add(MoveTemp(v2));
	Arguments.Add(MoveTemp(v3));
	Arguments.Add(MoveTemp(v4));
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(Arguments), false, false);
}

void FText::GetFormatPatternParameters(const FTextFormat& Fmt, TArray<FString>& ParameterNames)
{
	return Fmt.GetFormatArgumentNames(ParameterNames);
}

FText FText::Format(FTextFormat Fmt, FFormatNamedArguments InArguments)
{
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(InArguments), false, false);
}

FText FText::Format(FTextFormat Fmt, FFormatOrderedArguments InArguments)
{
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(InArguments), false, false);
}

FText FText::Format(FTextFormat Fmt, TArray< FFormatArgumentData > InArguments)
{
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(InArguments), false, false);
}

FText FText::FormatNamedImpl(FTextFormat&& Fmt, FFormatNamedArguments&& InArguments)
{
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(InArguments), false, false);
}

FText FText::FormatOrderedImpl(FTextFormat&& Fmt, FFormatOrderedArguments&& InArguments)
{
	return FTextFormatter::Format(MoveTemp(Fmt), MoveTemp(InArguments), false, false);
}

/**
* Generate an FText that represents the passed number in the passed culture
*/

// on some platforms (PS4) int64_t is a typedef of long.  However, UE4 typedefs int64 as long long.  Since these are distinct types, and ICU only has a constructor for int64_t, casting to int64 causes a compile error from ambiguous function call, 
// so cast to int64_t's where appropriate here to avoid problems.

#define DEF_ASNUMBER_CAST(T1, T2) FText FText::AsNumber(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture) { return FText::AsNumberTemplate<T1, T2>(Val, Options, TargetCulture); }
#define DEF_ASNUMBER(T) DEF_ASNUMBER_CAST(T, T)
DEF_ASNUMBER(float)
DEF_ASNUMBER(double)
DEF_ASNUMBER(int8)
DEF_ASNUMBER(int16)
DEF_ASNUMBER(int32)
DEF_ASNUMBER_CAST(int64, int64_t)
DEF_ASNUMBER(uint8)
DEF_ASNUMBER(uint16)
DEF_ASNUMBER_CAST(uint32, int64_t)
DEF_ASNUMBER_CAST(uint64, int64_t)
#undef DEF_ASNUMBER
#undef DEF_ASNUMBER_CAST

template<typename T1, typename T2>
FText FText::AsNumberTemplate(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetDecimalNumberFormattingRules();
	const FNumberFormattingOptions& FormattingOptions = (Options) ? *Options : FormattingRules.CultureDefaultFormattingOptions;
	FString NativeString = FastDecimalFormat::NumberToString(Val, FormattingRules, FormattingOptions);

	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsNumber>(MoveTemp(NativeString), FTextHistory_AsNumber(Val, Options, TargetCulture))));
}

/**
 * Generate an FText that represents the passed number as currency in the current culture
 */
#define DEF_ASCURRENCY_CAST(T1, T2) FText FText::AsCurrency(T1 Val, const FString& CurrencyCode, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture) { return FText::AsCurrencyTemplate<T1, T2>(Val, CurrencyCode, Options, TargetCulture); }
#define DEF_ASCURRENCY(T) DEF_ASCURRENCY_CAST(T, T)
DEF_ASCURRENCY(float)
	DEF_ASCURRENCY(double)
	DEF_ASCURRENCY(int8)
	DEF_ASCURRENCY(int16)
	DEF_ASCURRENCY(int32)
	DEF_ASCURRENCY_CAST(int64, int64_t)
	DEF_ASCURRENCY(uint8)
	DEF_ASCURRENCY(uint16)
	DEF_ASCURRENCY_CAST(uint32, int64_t)
	DEF_ASCURRENCY_CAST(uint64, int64_t)
#undef DEF_ASCURRENCY
#undef DEF_ASCURRENCY_CAST

template<typename T1, typename T2>
FText FText::AsCurrencyTemplate(T1 Val, const FString& CurrencyCode, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(CurrencyCode);
	const FNumberFormattingOptions& FormattingOptions = (Options) ? *Options : FormattingRules.CultureDefaultFormattingOptions;
	FString NativeString = FastDecimalFormat::NumberToString(Val, FormattingRules, FormattingOptions);

	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsCurrency>(MoveTemp(NativeString), FTextHistory_AsCurrency(Val, CurrencyCode, Options, TargetCulture))));
}

FText FText::AsCurrencyBase(int64 BaseVal, const FString& CurrencyCode, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(CurrencyCode);
	const FNumberFormattingOptions& FormattingOptions = FormattingRules.CultureDefaultFormattingOptions;
	double Val = static_cast<double>(BaseVal) / FMath::Pow(10.0f, FormattingOptions.MaximumFractionalDigits);
	FString NativeString = FastDecimalFormat::NumberToString(Val, FormattingRules, FormattingOptions);

	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsCurrency>(MoveTemp(NativeString), FTextHistory_AsCurrency(Val, CurrencyCode, nullptr, TargetCulture))));
}

/**
* Generate an FText that represents the passed number as a percentage in the current culture
*/

#define DEF_ASPERCENT_CAST(T1, T2) FText FText::AsPercent(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture) { return FText::AsPercentTemplate<T1, T2>(Val, Options, TargetCulture); }
#define DEF_ASPERCENT(T) DEF_ASPERCENT_CAST(T, T)
DEF_ASPERCENT(double)
DEF_ASPERCENT(float)
#undef DEF_ASPERCENT
#undef DEF_ASPERCENT_CAST

template<typename T1, typename T2>
FText FText::AsPercentTemplate(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetPercentFormattingRules();
	const FNumberFormattingOptions& FormattingOptions = (Options) ? *Options : FormattingRules.CultureDefaultFormattingOptions;
	FString NativeString = FastDecimalFormat::NumberToString(Val * static_cast<T1>(100), FormattingRules, FormattingOptions);

	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsPercent>(MoveTemp(NativeString), FTextHistory_AsPercent(Val, Options, TargetCulture))));
}

FText FText::AsMemory(uint64 NumBytes, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FFormatNamedArguments Args;

	if (NumBytes < 1024)
	{
		Args.Add( TEXT("Number"), FText::AsNumber( NumBytes, Options, TargetCulture) );
		Args.Add( TEXT("Unit"), FText::FromString( FString( TEXT("B") ) ) );
		return FText::Format( NSLOCTEXT("Internationalization", "ComputerMemoryFormatting", "{Number} {Unit}"), Args );
	}

	static const TCHAR* Prefixes = TEXT("kMGTPEZY");
	int32 Prefix = 0;

	for (; NumBytes > 1024 * 1024; NumBytes >>= 10)
	{
		++Prefix;
	}

	const double MemorySizeAsDouble = (double)NumBytes / 1024.0;
	Args.Add( TEXT("Number"), FText::AsNumber( MemorySizeAsDouble, Options, TargetCulture) );
	Args.Add( TEXT("Unit"), FText::FromString( FString( 1, &Prefixes[Prefix] ) + TEXT("B") ) );
	return FText::Format( NSLOCTEXT("Internationalization", "ComputerMemoryFormatting", "{Number} {Unit}"), Args);
}

FString FText::GetInvariantTimeZone()
{
	return TEXT("Etc/Unknown");
}

bool FText::FindText( const FString& Namespace, const FString& Key, FText& OutText, const FString* const SourceString )
{
	FTextDisplayStringPtr FoundString = FTextLocalizationManager::Get().FindDisplayString( Namespace, Key, SourceString );

	if ( FoundString.IsValid() )
	{
		OutText = FText( SourceString ? FString(*SourceString) : FString(), FoundString.ToSharedRef() );
		return true;
	}

	return false;
}

void FText::SerializeText(FArchive& Ar, FText& Value)
{
	//When duplicating, the CDO is used as the template, then values for the instance are assigned.
	//If we don't duplicate the string, the CDO and the instance are both pointing at the same thing.
	//This would result in all subsequently duplicated objects stamping over formerly duplicated ones.

	// Older FText's stored their "SourceString", that is now stored in a history class so move it there
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FTEXT_HISTORY)
	{
		FString SourceStringToImplantIntoHistory;
		Ar << SourceStringToImplantIntoHistory;

		FTextDisplayStringPtr DisplayString;

		// Namespaces and keys are no longer stored in the FText, we need to read them in and discard
		if (Ar.UE4Ver() >= VER_UE4_ADDED_NAMESPACE_AND_KEY_DATA_TO_FTEXT)
		{
			FString Namespace;
			FString Key;

			Ar << Namespace;
			Ar << Key;

			// Get the DisplayString using the namespace, key, and source string.
			DisplayString = FTextLocalizationManager::Get().GetDisplayString(Namespace, Key, &SourceStringToImplantIntoHistory);
		}
		else
		{
			DisplayString = MakeShareable(new FString());
		}

		check(DisplayString.IsValid());
		Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_Base>(DisplayString.ToSharedRef(), FTextHistory_Base(MoveTemp(SourceStringToImplantIntoHistory))));
	}

#if WITH_EDITOR
	if (Ar.IsCooking() && Ar.IsSaving() && Ar.IsPersistent() && (Ar.GetDebugSerializationFlags() & DSF_EnableCookerWarnings))
	{
		if (!!(Value.Flags & ETextFlag::ConvertedProperty))
		{
			UE_LOG(LogText, Warning, TEXT("Saving FText \"%s\" which has been converted at load time please resave source package %s to avoid determinisitic cook and localization issues."), *Value.ToString(), *Ar.GetArchiveName());
		}
		else if (!!(Value.Flags & ETextFlag::InitializedFromString))
		{
			UE_LOG(LogText, Warning, TEXT("Saving FText \"%s\" which has been initialized from FString at cook time resave of source package %s may fix issue."), *Value.ToString(), *Ar.GetArchiveName())
		}
	}
#endif

	const int32 OriginalFlags = Value.Flags;

	if(Ar.IsSaving())
	{
		Value.TextData->PersistText(); // We always need to do this when saving so that we can save the history correctly
		if(Ar.IsPersistent())
		{
			Value.Flags &= ~(ETextFlag::ConvertedProperty | ETextFlag::InitializedFromString); // Remove conversion flag before saving.
		}
	}
	Ar << Value.Flags;

	if (Ar.IsLoading() && Ar.ArIsPersistent)
	{
		Value.Flags &= ~(ETextFlag::ConvertedProperty | ETextFlag::InitializedFromString); // Remove conversion flag before saving.
	}

	if (Ar.IsSaving())
	{
		Value.Flags = OriginalFlags;
	}

	if (Ar.UE4Ver() >= VER_UE4_FTEXT_HISTORY)
	{
		bool bSerializeHistory = true;

		if (Ar.IsSaving())
		{
			// Skip the history for empty texts
			bSerializeHistory = !Value.IsEmpty();

			if (!bSerializeHistory)
			{
				int8 NoHistory = INDEX_NONE;
				Ar << NoHistory;
			}
		}
		else if (Ar.IsLoading())
		{
			// The type is serialized during the serialization of the history, during deserialization we need to deserialize it and create the correct history
			int8 HistoryType = INDEX_NONE;
			Ar << HistoryType;

			// Create the history class based on the serialized type
			switch((ETextHistoryType)HistoryType)
			{
			case ETextHistoryType::Base:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_Base>());
					break;
				}
			case ETextHistoryType::NamedFormat:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_NamedFormat>());
					break;
				}
			case ETextHistoryType::OrderedFormat:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_OrderedFormat>());
					break;
				}
			case ETextHistoryType::ArgumentFormat:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_ArgumentDataFormat>());
					break;
				}
			case ETextHistoryType::AsNumber:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_AsNumber>());
					break;
				}
			case ETextHistoryType::AsPercent:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_AsPercent>());
					break;
				}
			case ETextHistoryType::AsCurrency:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_AsCurrency>());
					break;
				}
			case ETextHistoryType::AsDate:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_AsDate>());
					break;
				}
			case ETextHistoryType::AsTime:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_AsTime>());
					break;
				}
			case ETextHistoryType::AsDateTime:
				{
					Value.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_AsDateTime>());
					break;
				}
			default:
				{
					bSerializeHistory = false;
					Value.TextData = FText::GetEmpty().TextData;
				}
			}
		}

		if(bSerializeHistory)
		{
			FTextHistory& MutableTextHistory = Value.TextData->GetMutableTextHistory();
			MutableTextHistory.Serialize(Ar);
			MutableTextHistory.SerializeForDisplayString(Ar, Value.TextData->GetMutableLocalizedString());
		}
	}

	if(Ar.IsLoading())
	{
		Value.Rebuild();
	}

	// Validate
	//if( Ar.IsLoading() && Ar.IsPersistent() && !Value.Key.IsValid() )
	//{
	//	UE_LOG( LogText, Error, TEXT("Loaded an FText from a persistent archive but lacking a key (Namespace:%s, Source:%s)."), Value.Namespace.Get() ? **Value.Namespace : TEXT(""), Value.SourceString.Get() ? **Value.SourceString : TEXT("") );
	//}

	if( Value.ShouldGatherForLocalization() )
	{
		Ar.ThisRequiresLocalizationGather();
	}
}

#if WITH_EDITOR
FText FText::ChangeKey( const FString& Namespace, const FString& Key, const FText& Text )
{
	return FText(FString(*Text.TextData->GetTextHistory().GetSourceString()), Namespace, Key, Text.Flags);
}
#endif

FText FText::CreateNumericalText(TSharedRef<ITextData, ESPMode::ThreadSafe> InTextData)
{
	FText NewText = FText(MoveTemp(InTextData));
	if (!GIsEditor)
	{
		NewText.Flags |= ETextFlag::Transient;
	}
	return NewText;
}

FText FText::CreateChronologicalText(TSharedRef<ITextData, ESPMode::ThreadSafe> InTextData)
{
	FText NewText = FText(MoveTemp(InTextData));
	if (!GIsEditor)
	{
		NewText.Flags |= ETextFlag::Transient;
	}
	return NewText;
}

FText FText::FromName( const FName& Val) 
{
	return FText::FromString(Val.ToString());
}

FText FText::FromString( const FString& String )
{
	FText NewText = FText( FString(String) );

	if (!GIsEditor)
	{
		NewText.Flags |= ETextFlag::CultureInvariant;
	}
	NewText.Flags |= ETextFlag::InitializedFromString;

	return NewText;
}

FText FText::FromString( FString&& String )
{
	FText NewText = FText( MoveTemp(String) );

	if (!GIsEditor)
	{
		NewText.Flags |= ETextFlag::CultureInvariant;
	}
	NewText.Flags |= ETextFlag::InitializedFromString;

	return NewText;
}

FText FText::AsCultureInvariant( const FString& String )
{
	FText NewText = FText( FString(String) );
	NewText.Flags |= ETextFlag::CultureInvariant;

	return NewText;
}

FText FText::AsCultureInvariant( FString&& String )
{
	FText NewText = FText( MoveTemp(String) );
	NewText.Flags |= ETextFlag::CultureInvariant;

	return NewText;
}

FText FText::AsCultureInvariant( FText Text )
{
	FText NewText = FText( MoveTemp(Text) );
	NewText.Flags |= ETextFlag::CultureInvariant;

	return NewText;
}

const FString& FText::ToString() const
{
	Rebuild();

	return TextData->GetDisplayString();
}

FString FText::BuildSourceString() const
{
	return TextData->GetTextHistory().ToText(true).ToString();
}

bool FText::IsNumeric() const
{
	return TextData->GetDisplayString().IsNumeric();
}

void FText::Rebuild() const
{
	FTextHistory& MutableTextHistory = TextData->GetMutableTextHistory();
	if (MutableTextHistory.IsOutOfDate())
	{
		// Need to persist the text before the rebuild so that we have a valid localized string pointer
		TextData->PersistText();
		MutableTextHistory.Rebuild(TextData->GetLocalizedString().ToSharedRef());
	}
}

bool FText::IsTransient() const
{
	return (Flags & ETextFlag::Transient) != 0;
}

bool FText::IsCultureInvariant() const
{
	return (Flags & ETextFlag::CultureInvariant) != 0;
}

bool FText::ShouldGatherForLocalization() const
{
	const FString& SourceString = GetSourceString();

	auto IsAllWhitespace = [](const FString& String) -> bool
	{
		for(int32 i = 0; i < String.Len(); ++i)
		{
			if( !FText::IsWhitespace( String[i] ) )
			{
				return false;
			}
		}
		return true;
	};

	return !((Flags & ETextFlag::CultureInvariant) || (Flags & ETextFlag::Transient)) && !SourceString.IsEmpty() && !IsAllWhitespace(SourceString);
}

const FString& FText::GetSourceString() const
{
	const FString* SourceString = TextData->GetTextHistory().GetSourceString();
	if(SourceString)
	{
		return *SourceString;
	}

	return TextData->GetDisplayString();
}

bool FText::IdenticalTo( const FText& Other ) const
{
	// If both instances point to the same data or localized string, then both instances are considered identical.
	// This is fast as it skips a lexical compare, however it can also return false for two instances that have identical strings, but in different pointers.
	// For instance, this method will return false for two FText objects created from FText::FromString("Wooble") as they each have unique, non-shared instances.
	return TextData == Other.TextData || TextData->GetLocalizedString() == Other.TextData->GetLocalizedString();
}

void FText::GetSourceTextsFromFormatHistory(TArray<FText>& OutSourceTexts) const
{
	TextData->GetTextHistory().GetSourceTextsFromFormatHistory(*this, OutSourceTexts);
}

FArchive& operator<<(FArchive& Ar, FFormatArgumentValue& Value)
{
	int8 TypeAsInt8 = Value.Type;
	Ar << TypeAsInt8;
	Value.Type = (EFormatArgumentType::Type)TypeAsInt8;

	switch(Value.Type)
	{
	case EFormatArgumentType::Double:
		{
			Ar << Value.DoubleValue;
			break;
		}
	case EFormatArgumentType::Float:
		{
			Ar << Value.FloatValue;
			break;
		}
	case EFormatArgumentType::Int:
		{
			Ar << Value.IntValue;
			break;
		}
	case EFormatArgumentType::UInt:
		{
			Ar << Value.UIntValue;
			break;
		}
	case EFormatArgumentType::Text:
		{
			if(Ar.IsLoading())
			{
				Value.TextValue = FText();
			}
			Ar << Value.TextValue.GetValue();
			break;
		}
	}
	
	return Ar;
}

FString FFormatArgumentValue::ToFormattedString(const bool bInRebuildText, const bool bInRebuildAsSource) const
{
	FString Result;
	ToFormattedString(bInRebuildText, bInRebuildAsSource, Result);
	return Result;
}

void FFormatArgumentValue::ToFormattedString(const bool bInRebuildText, const bool bInRebuildAsSource, FString& OutResult) const
{
	if (Type == EFormatArgumentType::Text)
	{
		const FText& LocalText = GetTextValue();

		// When doing a rebuild, all FText arguments need to be rebuilt during the Format
		if (bInRebuildText)
		{
			LocalText.Rebuild();
		}

		OutResult += (bInRebuildAsSource) ? LocalText.BuildSourceString() : LocalText.ToString();
	}
	else if (Type == EFormatArgumentType::Gender)
	{
		// Nothing to do
	}
	else
	{
		FInternationalization& I18N = FInternationalization::Get();
		checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
		const FCulture& Culture = *I18N.GetCurrentCulture();

		const FDecimalNumberFormattingRules& FormattingRules = Culture.GetDecimalNumberFormattingRules();
		const FNumberFormattingOptions& FormattingOptions = FormattingRules.CultureDefaultFormattingOptions;

		switch (Type)
		{
		case EFormatArgumentType::Int:
			FastDecimalFormat::NumberToString(IntValue, FormattingRules, FormattingOptions, OutResult);
			break;
		case EFormatArgumentType::UInt:
			FastDecimalFormat::NumberToString(UIntValue, FormattingRules, FormattingOptions, OutResult);
			break;
		case EFormatArgumentType::Float:
			FastDecimalFormat::NumberToString(FloatValue, FormattingRules, FormattingOptions, OutResult);
			break;
		case EFormatArgumentType::Double:
			FastDecimalFormat::NumberToString(DoubleValue, FormattingRules, FormattingOptions, OutResult);
			break;
		default:
			break;
		}
	}
}

void FFormatArgumentData::ResetValue()
{
	ArgumentValueType = EFormatArgumentType::Text;
	ArgumentValue = FText::GetEmpty();
	ArgumentValueInt = 0;
	ArgumentValueFloat = 0.0f;
	ArgumentValueGender = ETextGender::Masculine;
}

FArchive& operator<<(FArchive& Ar, FFormatArgumentData& Value)
{
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		// ArgumentName was changed to be FString rather than FText, so we need to convert older data to ensure serialization stays happy outside of UStruct::SerializeTaggedProperties.
		if (Ar.UE4Ver() >= VER_UE4_K2NODE_VAR_REFERENCEGUIDS) // There was no version bump for this change, but VER_UE4_K2NODE_VAR_REFERENCEGUIDS was made at almost the same time.
		{
			Ar << Value.ArgumentName;
		}
		else
		{
			FText TempValue;
			Ar << TempValue;
			Value.ArgumentName = TempValue.ToString();
		}
	}
	if (Ar.IsSaving())
	{
		Ar << Value.ArgumentName;
	}

	uint8 TypeAsByte = Value.ArgumentValueType;
	if (Ar.IsLoading())
	{
		Value.ResetValue();

		if (Ar.CustomVer(FEditorObjectVersion::GUID) >= FEditorObjectVersion::TextFormatArgumentDataIsVariant)
		{
			Ar << TypeAsByte;
		}
		else
		{
			// Old data was always text
			TypeAsByte = EFormatArgumentType::Text;
		}
	}
	else if (Ar.IsSaving())
	{
		Ar << TypeAsByte;
	}

	Value.ArgumentValueType = (EFormatArgumentType::Type)TypeAsByte;
	switch (Value.ArgumentValueType)
	{
	case EFormatArgumentType::Int:
		Ar << Value.ArgumentValueInt;
		break;
	case EFormatArgumentType::Float:
		Ar << Value.ArgumentValueFloat;
		break;
	case EFormatArgumentType::Text:
		Ar << Value.ArgumentValue;
		break;
	case EFormatArgumentType::Gender:
		Ar << (uint8&)Value.ArgumentValueGender;
		break;
	default:
		break;
	}

	return Ar;
}

FTextSnapshot::FTextSnapshot()
	: TextDataPtr()
	, GlobalHistoryRevision(0)
	, LocalHistoryRevision(0)
	, Flags(0)
{
}

FTextSnapshot::FTextSnapshot(const FText& InText)
	: TextDataPtr(InText.TextData)
	, GlobalHistoryRevision(InText.TextData->GetGlobalHistoryRevision())
	, LocalHistoryRevision(InText.TextData->GetLocalHistoryRevision())
	, Flags(InText.Flags)
{
}

bool FTextSnapshot::IdenticalTo(const FText& InText) const
{
	// Make sure the string is up-to-date with the current culture
	// (this usually happens when ToString() is called)
	InText.Rebuild();

	return TextDataPtr == InText.TextData 
		&& GlobalHistoryRevision == InText.TextData->GetGlobalHistoryRevision()
		&& LocalHistoryRevision == InText.TextData->GetLocalHistoryRevision()
		&& Flags == InText.Flags;
}

bool FTextSnapshot::IsDisplayStringEqualTo(const FText& InText) const
{
	// Make sure the string is up-to-date with the current culture
	// (this usually happens when ToString() is called)
	InText.Rebuild();

	// We have to assume that the display string has changed if the history of the text has changed
	// (due to a culture change), as we no longer have the old display string to compare against
	return GlobalHistoryRevision == InText.TextData->GetGlobalHistoryRevision()
		&& LocalHistoryRevision == InText.TextData->GetLocalHistoryRevision()
		&& TextDataPtr.IsValid() && TextDataPtr->GetDisplayString().Equals(InText.ToString(), ESearchCase::CaseSensitive);
}

FScopedTextIdentityPreserver::FScopedTextIdentityPreserver(FText& InTextToPersist)
	: TextToPersist(InTextToPersist)
	, HadFoundNamespaceAndKey(false)
	, Flags(TextToPersist.Flags)
{
	// Empty display strings can't have a namespace or key.
	if (GIsEditor && !TextToPersist.TextData->GetDisplayString().IsEmpty())
	{
		// Save off namespace and key to be restored later.
		TextToPersist.TextData->PersistText();
		HadFoundNamespaceAndKey = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(TextToPersist.TextData->GetLocalizedString().ToSharedRef(), Namespace, Key);
	}
}

FScopedTextIdentityPreserver::~FScopedTextIdentityPreserver()
{
	// Never persist identities in non-editor situations
	// If we don't have a key, then the old identity wasn't valid and shouldn't be preserved.
	// Never persist identities for immutable (i.e. code LOCTEXT declared) text.
	if(GIsEditor && HadFoundNamespaceAndKey && (Flags & ETextFlag::Immutable) == 0)
	{
		// Get the text's new source string.
		const FString* SourceString = FTextInspector::GetSourceString(TextToPersist);

		// Without a source string, we can't possibly preserve the identity. If the text we're preserving identity for can't possibly have an identity anymore, this class shouldn't be used on this text.
		check(SourceString);

		// Create/update the display string instance for this identity in the text localization manager...
		const FTextDisplayStringRef DisplayString = FTextLocalizationManager::Get().GetDisplayString(Namespace, Key, SourceString);

		// ... and update the data on the text instance
		TextToPersist.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_Base>(MoveTemp(DisplayString), FTextHistory_Base(FString(*SourceString))));
	}
}

bool TextBiDi::IsControlCharacter(const TCHAR InChar)
{
	return InChar == TEXT('\u061C')  // ARABIC LETTER MARK
		|| InChar == TEXT('\u200E')  // LEFT-TO-RIGHT MARK
		|| InChar == TEXT('\u200F')  // RIGHT-TO-LEFT MARK
		|| InChar == TEXT('\u202A')  // LEFT-TO-RIGHT EMBEDDING
		|| InChar == TEXT('\u202B')  // RIGHT-TO-LEFT EMBEDDING
		|| InChar == TEXT('\u202C')  // POP DIRECTIONAL FORMATTING
		|| InChar == TEXT('\u202D')  // LEFT-TO-RIGHT OVERRIDE
		|| InChar == TEXT('\u202E')  // RIGHT-TO-LEFT OVERRIDE
		|| InChar == TEXT('\u2066')  // LEFT-TO-RIGHT ISOLATE
		|| InChar == TEXT('\u2067')  // RIGHT-TO-LEFT ISOLATE
		|| InChar == TEXT('\u2068')  // FIRST STRONG ISOLATE
		|| InChar == TEXT('\u2069'); // POP DIRECTIONAL ISOLATE
}

#define LOC_DEFINE_REGION
const FString FTextStringHelper::InvTextMarker = TEXT("INVTEXT");
const FString FTextStringHelper::NsLocTextMarker = TEXT("NSLOCTEXT");
const FString FTextStringHelper::LocTextMarker = TEXT("LOCTEXT");
#undef LOC_DEFINE_REGION

bool FTextStringHelper::ReadFromString_ComplexText(const TCHAR* Buffer, FText& OutValue, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, int32* OutNumCharsRead)
{
#define LOC_DEFINE_REGION
	const TCHAR* const Start = Buffer;

	auto ExtractQuotedString = [&](FString& OutStr) -> const TCHAR*
	{
		int32 CharsRead = 0;
		if (!FParse::QuotedString(Buffer, OutStr, &CharsRead))
		{
			return nullptr;
		}

		Buffer += CharsRead;
		return Buffer;
	};

	auto WalkToCharacter = [&](const TCHAR InChar) -> const TCHAR*
	{
		while (*Buffer && *Buffer != InChar && *Buffer != TCHAR('\n') && *Buffer != TCHAR('\r'))
		{
			++Buffer;
		}

		if (*Buffer != InChar)
		{
			return nullptr;
		}

		return Buffer;
	};

	#define EXTRACT_QUOTED_STRING(S)		\
		Buffer = ExtractQuotedString(S);	\
		if (!Buffer)						\
		{									\
			return false;					\
		}

	#define WALK_TO_CHARACTER(C)			\
		Buffer = WalkToCharacter(TCHAR(C));	\
		if (!Buffer)						\
		{									\
			return false;					\
		}

	if (FCString::Strstr(Buffer, *InvTextMarker))
	{
		// Parsing something of the form: INVTEXT("...")
		Buffer += InvTextMarker.Len();

		// Walk to the opening bracket
		WALK_TO_CHARACTER('(');

		// Walk to the opening quote, and then parse out the quoted string
		FString InvariantString;
		WALK_TO_CHARACTER('"');
		EXTRACT_QUOTED_STRING(InvariantString);

		// Walk to the closing bracket, and then move past it to indicate that the value was successfully imported
		WALK_TO_CHARACTER(')');
		++Buffer;

		OutValue = FText::AsCultureInvariant(MoveTemp(InvariantString));

		if (OutNumCharsRead)
		{
			*OutNumCharsRead = (Buffer - Start);
		}

		return true;
	}
	else if (FCString::Strstr(Buffer, *NsLocTextMarker))
	{
		// Parsing something of the form: NSLOCTEXT("...", "...", "...")
		Buffer += NsLocTextMarker.Len();

		// Walk to the opening bracket
		WALK_TO_CHARACTER('(');

		// Walk to the opening quote, and then parse out the quoted namespace
		FString NamespaceString;
		WALK_TO_CHARACTER('"');
		EXTRACT_QUOTED_STRING(NamespaceString);

		// Walk to the opening quote, and then parse out the quoted key
		FString KeyString;
		WALK_TO_CHARACTER('"');
		EXTRACT_QUOTED_STRING(KeyString);

		// Walk to the opening quote, and then parse out the quoted source string
		FString SourceString;
		WALK_TO_CHARACTER('"');
		EXTRACT_QUOTED_STRING(SourceString);

		// Walk to the closing bracket, and then move past it to indicate that the value was successfully imported
		WALK_TO_CHARACTER(')');
		++Buffer;

		if (KeyString.IsEmpty())
		{
			OutValue = FText::AsCultureInvariant(MoveTemp(SourceString));
		}
		else
		{
#if USE_STABLE_LOCALIZATION_KEYS
			if (GIsEditor && PackageNamespace && *PackageNamespace)
			{
				NamespaceString = TextNamespaceUtil::BuildFullNamespace(NamespaceString, PackageNamespace);
			}
#endif // USE_STABLE_LOCALIZATION_KEYS
			OutValue = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *NamespaceString, *KeyString);
		}

		if (OutNumCharsRead)
		{
			*OutNumCharsRead = (Buffer - Start);
		}

		return true;
	}
	else if (FCString::Strstr(Buffer, *LocTextMarker))
	{
		// Parsing something of the form: LOCTEXT("...", "...")
		// This only exists as people sometimes do this in config files. We assume an empty namespace should be used
		Buffer += LocTextMarker.Len();

		// Walk to the opening bracket
		WALK_TO_CHARACTER('(');

		// Walk to the opening quote, and then parse out the quoted key
		FString KeyString;
		WALK_TO_CHARACTER('"');
		EXTRACT_QUOTED_STRING(KeyString);

		// Walk to the opening quote, and then parse out the quoted source string
		FString SourceString;
		WALK_TO_CHARACTER('"');
		EXTRACT_QUOTED_STRING(SourceString);

		// Walk to the closing bracket, and then move past it to indicate that the value was successfully imported
		WALK_TO_CHARACTER(')');
		++Buffer;

		if (KeyString.IsEmpty())
		{
			OutValue = FText::AsCultureInvariant(MoveTemp(SourceString));
		}
		else
		{
#if USE_STABLE_LOCALIZATION_KEYS
			if (GIsEditor && PackageNamespace && *PackageNamespace)
			{
				const FString NamespaceString = TextNamespaceUtil::BuildFullNamespace((TextNamespace) ? TextNamespace : TEXT(""), PackageNamespace);
				OutValue = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *NamespaceString, *KeyString);
			}
			else
#endif // USE_STABLE_LOCALIZATION_KEYS
			{
				OutValue = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, (TextNamespace) ? TextNamespace : TEXT(""), *KeyString);
			}
		}

		if (OutNumCharsRead)
		{
			*OutNumCharsRead = (Buffer - Start);
		}

		return true;
	}

	#undef EXTRACT_QUOTED_STRING
	#undef WALK_TO_CHARACTER
#undef LOC_DEFINE_REGION

	return false;
}

bool FTextStringHelper::ReadFromString(const TCHAR* Buffer, FText& OutValue, const TCHAR* TextNamespace, const TCHAR* PackageNamespace, int32* OutNumCharsRead, const bool bRequiresQuotes)
{
	const TCHAR* const Start = Buffer;

	while (FChar::IsWhitespace(*Buffer))
	{
		++Buffer;
	}

	// First, try and parse the text as a complex text export
	{
		int32 SubNumCharsRead = 0;
		if (FTextStringHelper::ReadFromString_ComplexText(Buffer, OutValue, TextNamespace, PackageNamespace, &SubNumCharsRead))
		{
			Buffer += SubNumCharsRead;
			if (OutNumCharsRead)
			{
				*OutNumCharsRead = (Buffer - Start);
			}
			return true;
		}
	}

	// This isn't special text, so just parse it from a string
	if (bRequiresQuotes)
	{
		// Parse out the quoted source string
		FString LiteralString;

		int32 SubNumCharsRead = 0;
		if (FParse::QuotedString(Buffer, LiteralString, &SubNumCharsRead))
		{
			OutValue = FText::FromString(MoveTemp(LiteralString));
			Buffer += SubNumCharsRead;
			if (OutNumCharsRead)
			{
				*OutNumCharsRead = (Buffer - Start);
			}
			return true;
		}

		return false;
	}
	else
	{
		FString LiteralString = Buffer;

		// In order to indicate that the value was successfully imported, advance the buffer past the last character that was imported
		Buffer += LiteralString.Len();

		OutValue = FText::FromString(MoveTemp(LiteralString));

		if (OutNumCharsRead)
		{
			*OutNumCharsRead = (Buffer - Start);
		}
		return true;
	}

	return false;
}

bool FTextStringHelper::WriteToString(FString& Buffer, const FText& Value, const bool bRequiresQuotes)
{
#define LOC_DEFINE_REGION
	const FString& StringValue = FTextInspector::GetDisplayString(Value);

	if (Value.IsCultureInvariant())
	{
		// Produces INVTEXT("...")
		Buffer += TEXT("INVTEXT(\"");
		Buffer += StringValue.ReplaceCharWithEscapedChar();
		Buffer += TEXT("\")");
	}
	else
	{
		bool bIsLocalized = false;
		FString Namespace;
		FString Key;
		const FString* SourceString = FTextInspector::GetSourceString(Value);

		if (SourceString && Value.ShouldGatherForLocalization())
		{
			bIsLocalized = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(FTextInspector::GetSharedDisplayString(Value), Namespace, Key);
		}

		if (bIsLocalized)
		{
			// Produces NSLOCTEXT("...", "...", "...")
			Buffer += TEXT("NSLOCTEXT(\"");
			Buffer += Namespace.ReplaceCharWithEscapedChar();
			Buffer += TEXT("\", \"");
			Buffer += Key.ReplaceCharWithEscapedChar();
			Buffer += TEXT("\", \"");
			Buffer += SourceString->ReplaceCharWithEscapedChar();
			Buffer += TEXT("\")");
		}
		else
		{
			if (bRequiresQuotes)
			{
				Buffer += TEXT("\"");
				Buffer += StringValue.ReplaceCharWithEscapedChar();
				Buffer += TEXT("\"");
			}
			else
			{
				Buffer += StringValue;
			}
		}
	}
#undef LOC_DEFINE_REGION

	return true;
}

bool FTextStringHelper::IsComplexText(const TCHAR* Buffer)
{
#define LOC_DEFINE_REGION
	return FCString::Strstr(Buffer, *InvTextMarker) || FCString::Strstr(Buffer, *NsLocTextMarker) || FCString::Strstr(Buffer, *LocTextMarker);
#undef LOC_DEFINE_REGION
}

#undef LOCTEXT_NAMESPACE
