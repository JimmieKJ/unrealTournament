// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "TextHistory.h"

///////////////////////////////////////
// FTextHistory

/** Base class for all FText history types */

FTextHistory::FTextHistory()
	: Revision(FTextLocalizationManager::Get().GetHeadCultureRevision())
{

}

bool FTextHistory::IsOutOfDate()
{
	return Revision < FTextLocalizationManager::Get().GetHeadCultureRevision();
}

TSharedPtr< FString, ESPMode::ThreadSafe > FTextHistory::GetSourceString() const
{
	return NULL;
}

void FTextHistory::SerializeForDisplayString(FArchive& Ar, TSharedRef<FString, ESPMode::ThreadSafe>& InOutDisplayString)
{
	if(Ar.IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = INDEX_NONE;

		//When duplicating, the CDO is used as the template, then values for the instance are assigned.
		//If we don't duplicate the string, the CDO and the instance are both pointing at the same thing.
		//This would result in all subsequently duplicated objects stamping over formerly duplicated ones.
		InOutDisplayString = MakeShareable(new FString());
	}
}

void FTextHistory::Rebuild(TSharedRef< FString, ESPMode::ThreadSafe > InDisplayString)
{
	const bool bIsOutOfDate = IsOutOfDate();

	// FTextHistory_Base will never report being out-of-date, but we need to keep the history revision in sync
	// with the head culture regardless so that FTextSnapshot::IdenticalTo still works correctly
	Revision = FTextLocalizationManager::Get().GetHeadCultureRevision();

	if(bIsOutOfDate)
	{
		InDisplayString.Get() = FTextInspector::GetDisplayString(ToText(false));
	}
}

///////////////////////////////////////
// FTextHistory_Base

FTextHistory_Base::FTextHistory_Base(FString InSourceString)
	: SourceString(new FString( MoveTemp( InSourceString ) ))
{

}

FTextHistory_Base::FTextHistory_Base(TSharedPtr< FString, ESPMode::ThreadSafe > InSourceString)
	: SourceString(InSourceString)
{

}

FText FTextHistory_Base::ToText(bool bInAsSource) const
{
	if(bInAsSource)
	{
		if(SourceString.IsValid())
		{
			return FText::FromString(*SourceString.Get());
		}
		return FText::GetEmpty();
	}

	// This should never be called except to build as source
	check(0);

	return FText::GetEmpty();
}

TSharedPtr< FString, ESPMode::ThreadSafe > FTextHistory_Base::GetSourceString() const
{
	return SourceString;
}

void FTextHistory_Base::Serialize( FArchive& Ar )
{
	// If I serialize out the Namespace and Key HERE, then we can load it up.
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::Base;
		Ar << HistoryType;
	}
}

void FTextHistory_Base::SerializeForDisplayString(FArchive& Ar, TSharedRef<FString, ESPMode::ThreadSafe>& InOutDisplayString)
{
	if(Ar.IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = INDEX_NONE;

		FString Namespace;
		FString Key;
		FString SourceStringRaw;

		Ar << Namespace;
		Ar << Key;
		Ar << SourceStringRaw;

		// If there was a SourceString, store it in the member variable
		if(!SourceStringRaw.IsEmpty())
		{
			SourceString = MakeShareable(new FString(SourceStringRaw));
		}

		// Using the deserialized namespace and key, find the DisplayString.
		InOutDisplayString = FTextLocalizationManager::Get().GetString(Namespace, Key, SourceString.Get());
	}
	else if(Ar.IsSaving())
	{
		TSharedPtr< FString, ESPMode::ThreadSafe > Namespace;
		TSharedPtr< FString, ESPMode::ThreadSafe > Key;

		FTextLocalizationManager::Get().FindKeyNamespaceFromDisplayString(InOutDisplayString, Namespace, Key);

		// Serialize the Namespace, or an empty string if there is none
		if( Namespace.IsValid() )
		{
			Ar << *(Namespace);
		}
		else
		{
			FString Empty;
			Ar << Empty;
		}

		// Serialize the Key, if there is not one and this is in the editor, generate a new key.
		if( Key.IsValid() )
		{
			Ar << *(Key);
		}
		else
		{
			if(GIsEditor)
			{
				FString SerializedKey = FGuid::NewGuid().ToString();
				Ar << SerializedKey;
			}
			else
			{
				FString Empty;
				Ar << Empty;
			}
		}

		// Serialize the SourceString, or an empty string if there is none
		if( SourceString.IsValid() )
		{
			Ar << *(SourceString);
		}
		else
		{
			FString Empty;
			Ar << Empty;
		}
	}
}

///////////////////////////////////////
// FTextHistory_NamedFormat

FTextHistory_NamedFormat::FTextHistory_NamedFormat(const FText& InSourceText, const FFormatNamedArguments& InArguments)
	: SourceText(InSourceText)
	, Arguments(InArguments)
{

}

FText FTextHistory_NamedFormat::ToText(bool bInAsSource) const
{
	return FText::FormatInternal(SourceText, Arguments, true, bInAsSource);
}

void FTextHistory_NamedFormat::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::NamedFormat;
		Ar << HistoryType;
	}

	Ar << SourceText;
	Ar << Arguments;
}

///////////////////////////////////////
// FTextHistory_OrderedFormat

FTextHistory_OrderedFormat::FTextHistory_OrderedFormat(const FText& InSourceText, const FFormatOrderedArguments& InArguments)
	: SourceText(InSourceText)
	, Arguments(InArguments)
{

}

FText FTextHistory_OrderedFormat::ToText(bool bInAsSource) const
{
	return FText::FormatInternal(SourceText, Arguments, true, bInAsSource);
}

void FTextHistory_OrderedFormat::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::OrderedFormat;
		Ar << HistoryType;
	}

	Ar << SourceText;
	Ar << Arguments;
}

///////////////////////////////////////
// FTextHistory_ArgumentDataFormat

FTextHistory_ArgumentDataFormat::FTextHistory_ArgumentDataFormat(const FText& InSourceText, const TArray< struct FFormatArgumentData >& InArguments)
	: SourceText(InSourceText)
	, Arguments(InArguments)
{

}

FText FTextHistory_ArgumentDataFormat::ToText(bool bInAsSource) const
{
	return FText::FormatInternal(SourceText, Arguments, true, bInAsSource);
}

void FTextHistory_ArgumentDataFormat::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::ArgumentFormat;
		Ar << HistoryType;
	}

	Ar << SourceText;
	Ar << Arguments;
}

///////////////////////////////////////
// FTextHistory_FormatNumber

FTextHistory_FormatNumber::FTextHistory_FormatNumber()
	: FormatOptions(nullptr)
{
}

FTextHistory_FormatNumber::FTextHistory_FormatNumber(const FFormatArgumentValue& InSourceValue, const FNumberFormattingOptions* const InFormatOptions, const FCulturePtr InTargetCulture)
	: SourceValue(InSourceValue)
	, FormatOptions(nullptr)
	, TargetCulture(InTargetCulture)
{
	if(InFormatOptions)
	{
		FormatOptions = new FNumberFormattingOptions;
		(*FormatOptions) = *InFormatOptions;
	}
}

FTextHistory_FormatNumber::~FTextHistory_FormatNumber()
{
	delete FormatOptions;
}

void FTextHistory_FormatNumber::Serialize(FArchive& Ar)
{
	Ar << SourceValue;

	bool bHasFormatOptions = FormatOptions != nullptr;
	Ar << bHasFormatOptions;

	if(bHasFormatOptions)
	{
		if(Ar.IsLoading())
		{
			FormatOptions = new FNumberFormattingOptions;
		}
		CA_SUPPRESS(6011)
		Ar << *FormatOptions;
	}

	if(Ar.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Ar << CultureName;
	}
	else if(Ar.IsLoading())
	{
		FString CultureName;
		Ar << CultureName;

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

///////////////////////////////////////
// FTextHistory_AsNumber

FTextHistory_AsNumber::FTextHistory_AsNumber(const FFormatArgumentValue& InSourceValue, const FNumberFormattingOptions* const InFormatOptions, const FCulturePtr InTargetCulture)
	: FTextHistory_FormatNumber(InSourceValue, InFormatOptions, InTargetCulture)
{
}

FText FTextHistory_AsNumber::ToText(bool bInAsSource) const
{
	TSharedPtr< FCulture, ESPMode::ThreadSafe > CurrentCulture = bInAsSource? FInternationalization::Get().GetInvariantCulture() : TargetCulture;

	switch(SourceValue.Type)
	{
	case EFormatArgumentType::UInt:
		{
			return FText::AsNumber(SourceValue.UIntValue, FormatOptions, CurrentCulture);
		}
	case EFormatArgumentType::Int:
		{
			return FText::AsNumber(SourceValue.IntValue, FormatOptions, CurrentCulture);
		}
	case EFormatArgumentType::Float:
		{
			return FText::AsNumber(SourceValue.FloatValue, FormatOptions, CurrentCulture);
		}
	case EFormatArgumentType::Double:
		{
			return FText::AsNumber(SourceValue.DoubleValue, FormatOptions, CurrentCulture);
		}
	default:
		{
			// Should never reach this point
			check(0);
		}
	}
	return FText();
}

void FTextHistory_AsNumber::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsNumber;
		Ar << HistoryType;
	}

	FTextHistory_FormatNumber::Serialize(Ar);
}

///////////////////////////////////////
// FTextHistory_AsPercent

FTextHistory_AsPercent::FTextHistory_AsPercent(const FFormatArgumentValue& InSourceValue, const FNumberFormattingOptions* const InFormatOptions, const FCulturePtr InTargetCulture)
	: FTextHistory_FormatNumber(InSourceValue, InFormatOptions, InTargetCulture)
{
}

FText FTextHistory_AsPercent::ToText(bool bInAsSource) const
{
	TSharedPtr< FCulture, ESPMode::ThreadSafe > CurrentCulture = bInAsSource? FInternationalization::Get().GetInvariantCulture() : TargetCulture;

	switch(SourceValue.Type)
	{
	case EFormatArgumentType::Float:
		{
			return FText::AsPercent(SourceValue.FloatValue, FormatOptions, CurrentCulture);
		}
	case EFormatArgumentType::Double:
		{
			return FText::AsPercent(SourceValue.DoubleValue, FormatOptions, CurrentCulture);
		}
	default:
		{
			// Should never reach this point
			check(0);
		}
	}
	return FText();
}

void FTextHistory_AsPercent::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsPercent;
		Ar << HistoryType;
	}

	FTextHistory_FormatNumber::Serialize(Ar);
}

///////////////////////////////////////
// FTextHistory_AsCurrency

FTextHistory_AsCurrency::FTextHistory_AsCurrency(const FFormatArgumentValue& InSourceValue, const FString& InCurrencyCode, const FNumberFormattingOptions* const InFormatOptions, const FCulturePtr InTargetCulture)
: FTextHistory_FormatNumber(InSourceValue, InFormatOptions, InTargetCulture)
, CurrencyCode(InCurrencyCode)
{
}

FText FTextHistory_AsCurrency::ToText(bool bInAsSource) const
{
	TSharedPtr< FCulture, ESPMode::ThreadSafe > CurrentCulture = bInAsSource? FInternationalization::Get().GetInvariantCulture() : TargetCulture;

	switch(SourceValue.Type)
	{
	case EFormatArgumentType::UInt:
		{
			return FText::AsCurrency(SourceValue.UIntValue, CurrencyCode, FormatOptions, CurrentCulture);
		}
	case EFormatArgumentType::Int:
		{
			return FText::AsCurrency(SourceValue.IntValue, CurrencyCode, FormatOptions, CurrentCulture);
		}
	case EFormatArgumentType::Float:
		{
			return FText::AsCurrency(SourceValue.FloatValue, CurrencyCode, FormatOptions, CurrentCulture);
		}
	case EFormatArgumentType::Double:
		{
			return FText::AsCurrency(SourceValue.DoubleValue, CurrencyCode, FormatOptions, CurrentCulture);
		}
	default:
		{
			// Should never reach this point
			check(0);
		}
	}
	return FText();
}

void FTextHistory_AsCurrency::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsCurrency;
		Ar << HistoryType;
	}

	if (Ar.UE4Ver() >= VER_UE4_ADDED_CURRENCY_CODE_TO_FTEXT)
	{
		Ar << CurrencyCode;
	}

	FTextHistory_FormatNumber::Serialize(Ar);
}

///////////////////////////////////////
// FTextHistory_AsDate

FTextHistory_AsDate::FTextHistory_AsDate(const FDateTime& InSourceDateTime, const EDateTimeStyle::Type InDateStyle, const FString& InTimeZone, const FCulturePtr InTargetCulture)
	: SourceDateTime(InSourceDateTime)
	, DateStyle(InDateStyle)
	, TimeZone(InTimeZone)
	, TargetCulture(InTargetCulture)
{
}

void FTextHistory_AsDate::Serialize(FArchive& Ar)
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsDate;
		Ar << HistoryType;
	}

	Ar << SourceDateTime;

	int8 DateStyleInt8 = (int8)DateStyle;
	Ar << DateStyleInt8;
	DateStyle = (EDateTimeStyle::Type)DateStyleInt8;

	if( Ar.UE4Ver() >= VER_UE4_FTEXT_HISTORY_DATE_TIMEZONE )
	{
		Ar << TimeZone;
	}

	if(Ar.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Ar << CultureName;
	}
	else if(Ar.IsLoading())
	{
		FString CultureName;
		Ar << CultureName;

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

FText FTextHistory_AsDate::ToText(bool bInAsSource) const
{
	TSharedPtr< FCulture, ESPMode::ThreadSafe > CurrentCulture = bInAsSource? FInternationalization::Get().GetInvariantCulture() : TargetCulture;

	return FText::AsDate(SourceDateTime, DateStyle, TimeZone, CurrentCulture);
}
///////////////////////////////////////
// FTextHistory_AsTime

FTextHistory_AsTime::FTextHistory_AsTime(const FDateTime& InSourceDateTime, const EDateTimeStyle::Type InTimeStyle, const FString& InTimeZone, const FCulturePtr InTargetCulture)
	: SourceDateTime(InSourceDateTime)
	, TimeStyle(InTimeStyle)
	, TimeZone(InTimeZone)
	, TargetCulture(InTargetCulture)
{
}

void FTextHistory_AsTime::Serialize(FArchive& Ar)
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsTime;
		Ar << HistoryType;
	}

	Ar << SourceDateTime;

	int8 TimeStyleInt8 = (int8)TimeStyle;
	Ar << TimeStyleInt8;
	TimeStyle = (EDateTimeStyle::Type)TimeStyleInt8;

	Ar << TimeZone;

	if(Ar.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Ar << CultureName;
	}
	else if(Ar.IsLoading())
	{
		FString CultureName;
		Ar << CultureName;

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

FText FTextHistory_AsTime::ToText(bool bInAsSource) const
{
	TSharedPtr< FCulture, ESPMode::ThreadSafe > CurrentCulture = bInAsSource? FInternationalization::Get().GetInvariantCulture() : TargetCulture;
	
	return FText::AsTime(SourceDateTime, TimeStyle, TimeZone, TargetCulture);
}

///////////////////////////////////////
// FTextHistory_AsDateTime

FTextHistory_AsDateTime::FTextHistory_AsDateTime(const FDateTime& InSourceDateTime, const EDateTimeStyle::Type InDateStyle, const EDateTimeStyle::Type InTimeStyle, const FString& InTimeZone, const FCulturePtr InTargetCulture)
	: SourceDateTime(InSourceDateTime)
	, DateStyle(InDateStyle)
	, TimeStyle(InTimeStyle)
	, TimeZone(InTimeZone)
	, TargetCulture(InTargetCulture)
{
}

void FTextHistory_AsDateTime::Serialize(FArchive& Ar)
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsDateTime;
		Ar << HistoryType;
	}

	Ar << SourceDateTime;

	int8 DateStyleInt8 = (int8)DateStyle;
	Ar << DateStyleInt8;
	DateStyle = (EDateTimeStyle::Type)DateStyleInt8;

	int8 TimeStyleInt8 = (int8)TimeStyle;
	Ar << TimeStyleInt8;
	TimeStyle = (EDateTimeStyle::Type)TimeStyleInt8;

	Ar << TimeZone;

	if(Ar.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Ar << CultureName;
	}
	else if(Ar.IsLoading())
	{
		FString CultureName;
		Ar << CultureName;

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

FText FTextHistory_AsDateTime::ToText(bool bInAsSource) const
{
	TSharedPtr< FCulture, ESPMode::ThreadSafe > CurrentCulture = bInAsSource? FInternationalization::Get().GetInvariantCulture() : TargetCulture;
	
	return FText::AsDateTime(SourceDateTime, DateStyle, TimeStyle, TimeZone, TargetCulture);
}
