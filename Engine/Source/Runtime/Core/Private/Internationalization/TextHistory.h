// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

enum class ETextHistoryType
{
	Base = 0,
	NamedFormat,
	OrderedFormat,
	ArgumentFormat,
	AsNumber,
	AsPercent,
	AsCurrency,
	AsDate,
	AsTime,
	AsDateTime

	// Add new enum types at the end only! They are serialized by index.
};

/** Base interface class for all FText history types */
class CORE_API FTextHistory
{
public:
	friend class FTextSnapshot;

	FTextHistory();

	virtual ~FTextHistory() {};

	/** Rebuilds the FText from the hierarchical history, the result should be in the current locale */
	virtual FText ToText(bool bInAsSource) const = 0;
	
	/** Serializes the history to/from an FArchive */
	virtual void Serialize(FArchive& Ar) = 0;

	/** Serializes data needed to get the FText's DisplayString */
	virtual void SerializeForDisplayString(FArchive& Ar, FTextDisplayStringRef& InOutDisplayString);

	/** Returns TRUE if the Revision is out of date */
	virtual bool IsOutOfDate();

	/** Returns the source string managed by the history (if any). */
	virtual TSharedPtr< FString, ESPMode::ThreadSafe > GetSourceString() const;

	/** Trace the history of this Text until we find the base Texts it was comprised from */
	virtual void GetSourceTextsFromFormatHistory(FText Text, TArray<FText>& OutSourceTexts) const;

	/** Will rebuild the display string if out of date. */
	void Rebuild(TSharedRef< FString, ESPMode::ThreadSafe > InDisplayString);

protected:
	/** Revision index of this history, rebuilds when it is out of sync with the FTextLocalizationManager */
	int32 Revision;
};

/** No complexity to it, just holds the source string. */
class CORE_API FTextHistory_Base : public FTextHistory
{
public:
	FTextHistory_Base() {};
	FTextHistory_Base(FString InSourceString);
	FTextHistory_Base(TSharedPtr< FString, ESPMode::ThreadSafe > InSourceString);


	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void SerializeForDisplayString(FArchive& Ar, FTextDisplayStringRef& InOutDisplayString) override;
	virtual bool IsOutOfDate() override { return false; }
	virtual TSharedPtr< FString, ESPMode::ThreadSafe > GetSourceString() const override;
	// End FTextHistory interface

private:
	/** The source string for an FText */
	FTextDisplayStringPtr SourceString;
};

/** Handles history for FText::Format when passing named arguments */
class CORE_API FTextHistory_NamedFormat : public FTextHistory
{
public:
	FTextHistory_NamedFormat() {};
	FTextHistory_NamedFormat(const FText& InSourceText, const FFormatNamedArguments& InArguments);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetSourceTextsFromFormatHistory(FText, TArray<FText>& OutSourceTexts) const override;
	// End FTextHistory interface

private:
	/** The pattern used to format the text */
	FText SourceText;
	/** Arguments to replace in the pattern string */
	FFormatNamedArguments Arguments;
};

/** Handles history for FText::Format when passing ordered arguments */
class CORE_API FTextHistory_OrderedFormat : public FTextHistory
{
public:
	FTextHistory_OrderedFormat() {};
	FTextHistory_OrderedFormat(const FText& InSourceText, const FFormatOrderedArguments& InArguments);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetSourceTextsFromFormatHistory(FText, TArray<FText>& OutSourceTexts) const override;
	// End FTextHistory interface

private:
	/** The pattern used to format the text */
	FText SourceText;
	/** Arguments to replace in the pattern string */
	FFormatOrderedArguments Arguments;
};

/** Handles history for FText::Format when passing raw argument data */
class CORE_API FTextHistory_ArgumentDataFormat : public FTextHistory
{
public:
	FTextHistory_ArgumentDataFormat() {};
	FTextHistory_ArgumentDataFormat(const FText& InSourceText, const TArray< struct FFormatArgumentData >& InArguments);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetSourceTextsFromFormatHistory(FText, TArray<FText>& OutSourceTexts) const override;
	// End FTextHistory interface

private:
	/** The pattern used to format the text */
	FText SourceText;
	/** Arguments to replace in the pattern string */
	TArray< struct FFormatArgumentData > Arguments;
};

/** Base class for managing formatting FText's from: AsNumber, AsPercent, and AsCurrency. Manages data serialization of these history events */
class CORE_API FTextHistory_FormatNumber : public FTextHistory
{
public:
	FTextHistory_FormatNumber();
	FTextHistory_FormatNumber(const FFormatArgumentValue& InSourceValue, const FNumberFormattingOptions* const InFormatOptions, const FCulturePtr InTargetCulture);

	~FTextHistory_FormatNumber();

	// Begin FTextHistory interface
	virtual void Serialize(FArchive& Ar) override;
	// End FTextHistory interface
protected:
	/** The source value to format from */
	FFormatArgumentValue SourceValue;
	/** All the formatting options available to format using. This can be NULL */
	FNumberFormattingOptions* FormatOptions;
	/** The culture to format using */
	FCulturePtr TargetCulture;
};

/**  Handles history for formatting using AsNumber */
class CORE_API FTextHistory_AsNumber : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsNumber() {};
	FTextHistory_AsNumber(const FFormatArgumentValue& InSourceValue, const FNumberFormattingOptions* const InFormatOptions, const FCulturePtr InTargetCulture);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End FTextHistory interface
};

/**  Handles history for formatting using AsPercent */
class CORE_API FTextHistory_AsPercent : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsPercent() {};
	FTextHistory_AsPercent(const FFormatArgumentValue& InSourceValue, const FNumberFormattingOptions* const InFormatOptions, const FCulturePtr InTargetCulture);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End FTextHistory interface
};

/**  Handles history for formatting using AsCurrency */
class CORE_API FTextHistory_AsCurrency : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsCurrency() {};
	FTextHistory_AsCurrency(const FFormatArgumentValue& InSourceValue, const FString& CurrencyCode, const FNumberFormattingOptions* const InFormatOptions, const FCulturePtr InTargetCulture);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End FTextHistory interface

private:
	/** The currency used to format the number. */
	FString CurrencyCode;
};

/**  Handles history for formatting using AsDate */
class CORE_API FTextHistory_AsDate : public FTextHistory
{
public:
	FTextHistory_AsDate() {};
	FTextHistory_AsDate(const FDateTime& InSourceDateTime, const EDateTimeStyle::Type InDateStyle, const FString& InTimeZone, const FCulturePtr InTargetCulture);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End FTextHistory interface

private:
	/** The source date structure to format */
	FDateTime SourceDateTime;
	/** Style to format the date using */
	EDateTimeStyle::Type DateStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the date in */
	FCulturePtr TargetCulture;
};

/**  Handles history for formatting using AsTime */
class CORE_API FTextHistory_AsTime : public FTextHistory
{
public:
	FTextHistory_AsTime() {};
	FTextHistory_AsTime(const FDateTime& InSourceDateTime, const EDateTimeStyle::Type InTimeStyle, const FString& InTimeZone, const FCulturePtr InTargetCulture);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End FTextHistory interface

private:
	/** The source time structure to format */
	FDateTime SourceDateTime;
	/** Style to format the time using */
	EDateTimeStyle::Type TimeStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the time in */
	FCulturePtr TargetCulture;
};

/**  Handles history for formatting using AsDateTime */
class CORE_API FTextHistory_AsDateTime : public FTextHistory
{
public:
	FTextHistory_AsDateTime() {};
	FTextHistory_AsDateTime(const FDateTime& InSourceDateTime, const EDateTimeStyle::Type InDateStyle, const EDateTimeStyle::Type InTimeStyle, const FString& InTimeZone, const FCulturePtr InTargetCulture);

	// Begin FTextHistory interface
	virtual FText ToText(bool bInAsSource) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End FTextHistory interfaces

private:
	/** The source date and time structure to format */
	FDateTime SourceDateTime;
	/** Style to format the date using */
	EDateTimeStyle::Type DateStyle;
	/** Style to format the time using */
	EDateTimeStyle::Type TimeStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the time in */
	FCulturePtr TargetCulture;
};
