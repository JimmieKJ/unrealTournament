// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU
#include "ICUText.h"
#else
#include "LegacyText.h"
#endif

#include "TextHistory.h"

//DEFINE_LOG_CATEGORY(LogText);

//DEFINE_STAT(STAT_TextFormat);

#define LOCTEXT_NAMESPACE "Core.Text"

FArchive& operator<<( FArchive& Ar, FFormatArgumentData& Value )
{
	Ar << Value.ArgumentName;
	Ar << Value.ArgumentValue;
	return Ar;
}

bool FTextInspector::ShouldGatherForLocalization(const FText& Text)
{
	return Text.ShouldGatherForLocalization();
}

TOptional<FString> FTextInspector::GetNamespace(const FText& Text)
{
	FString Namespace;
	FString Key;
	const bool WasNamespaceAndKeyFound = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(Text.DisplayString, Namespace, Key);
	return WasNamespaceAndKeyFound ? TOptional<FString>(Namespace) : TOptional<FString>();
}

TOptional<FString> FTextInspector::GetKey(const FText& Text)
{
	FString Namespace;
	FString Key;
	const bool WasNamespaceAndKeyFound = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(Text.DisplayString, Namespace, Key);
	return WasNamespaceAndKeyFound ? TOptional<FString>(Key) : TOptional<FString>();
}

const FString* FTextInspector::GetSourceString(const FText& Text)
{
	return Text.GetSourceString().Get();
}

const FString& FTextInspector::GetDisplayString(const FText& Text)
{
	return Text.DisplayString.Get();
}

const FTextDisplayStringRef FTextInspector::GetSharedDisplayString(const FText& Text)
{
	return Text.DisplayString;
}

int32 FTextInspector::GetFlags(const FText& Text)
{
	return Text.Flags;
}

FFormatArgumentValue::FFormatArgumentValue()
	: Type(EFormatArgumentType::Int)
{
	IntValue = 0;
};


FFormatArgumentValue::FFormatArgumentValue(const int Value)
	: Type(EFormatArgumentType::Int)
{
	IntValue = Value;
}

FFormatArgumentValue::FFormatArgumentValue(const unsigned int Value)
	: Type(EFormatArgumentType::UInt)
{
	UIntValue = Value;
}

FFormatArgumentValue::FFormatArgumentValue(const int64 Value)
	: Type(EFormatArgumentType::Int)
{
	IntValue = Value;
}

FFormatArgumentValue::FFormatArgumentValue(const uint64 Value)
	: Type(EFormatArgumentType::UInt)
{
	UIntValue = Value;
}

FFormatArgumentValue::FFormatArgumentValue(const float Value)
	: Type(EFormatArgumentType::Float)
{
	FloatValue = Value;
}

FFormatArgumentValue::FFormatArgumentValue(const double Value)
	: Type(EFormatArgumentType::Double)
{
	DoubleValue = Value;
}

FFormatArgumentValue::FFormatArgumentValue(const FText& Value)
	: Type(EFormatArgumentType::Text)
{
	TextValue = new FText(Value);
}

FFormatArgumentValue::FFormatArgumentValue(const FFormatArgumentValue& Source)
	: Type(Source.Type)
{
	switch(Type)
	{
	case EFormatArgumentType::Int:
		{
			IntValue = Source.IntValue;
		}
		break;
	case EFormatArgumentType::UInt:
		{
			UIntValue = Source.UIntValue;
		}
		break;
	case EFormatArgumentType::Float:
		{
			FloatValue = Source.FloatValue;
		}
	case EFormatArgumentType::Double:
		{
			DoubleValue = Source.DoubleValue;
		}
		break;
	case EFormatArgumentType::Text:
		{
			TextValue = new FText(*Source.TextValue);
		}
		break;
	}
}

FFormatArgumentValue::~FFormatArgumentValue()
{
	if(Type == EFormatArgumentType::Text)
	{
		delete TextValue;
	}
}

FArchive& operator<<( FArchive& Ar, FFormatArgumentValue& Value )
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
				Value.TextValue = new FText;
			}
			Ar << *Value.TextValue;
			break;
		}
	}
	
	return Ar;
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

//Some error text formats
const FText FText::UnusedArgumentsError = LOCTEXT("Error_UnusedArguments", "ERR: The following arguments were unused ({0}).");
const FText FText::CommentStartError = LOCTEXT("Error_CommentDoesntStartWithQMark", "ERR: The Comment for Arg Block {0} does not start with a '?'.");
const FText FText::TooFewArgsErrorFormat = LOCTEXT("Error_TooFewArgs", "ERR: There are too few arguments. Arg {0} is used in block {1} when {2} is the maximum arg index.");
const FText FText::VeryLargeArgumentNumberErrorText = LOCTEXT("Error_TooManyArgDigitsInBlock", "ERR: Arg Block {0} has a very large argument number. This is unlikely so it is possibley some other parsing error.");
const FText FText::NoArgIndexError = LOCTEXT("Error_NoDigitsAtStartOfBlock", "ERR: Arg Block {0} does not start with the index number of the argument that it references. An argument block must start with a positive integer index to the argument its referencing. 0...max.");
const FText FText::NoClosingBraceError = LOCTEXT("Error_NoClosingBrace", "ERR: Arg Block {0} does not have a closing brace.");
const FText FText::OpenBraceInBlock = LOCTEXT("Error_OpenBraceInBlock", "ERR: Arg Block {0} has an open brace inside it. Braces are not allowed in argument blocks.");
const FText FText::UnEscapedCloseBraceOutsideOfArgumentBlock = LOCTEXT("Error_UnEscapedCloseBraceOutsideOfArgumentBlock", "ERR: There is an un-escaped }} after Arg Block {0}.");
const FText FText::SerializationFailureError = LOCTEXT("Error_SerializationFailure", "ERR: Transient text cannot be serialized \"{0}\".");

FText::FText()
	: DisplayString( GetEmpty().DisplayString )
	, Flags(0)
{
}

FText::FText( EInitToEmptyString )
	: DisplayString( new FString() )
	, Flags(0)
{
}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FText::FText(const FText& Other) = default;
	FText::FText(FText&& Other) = default;
	FText& FText::operator=(const FText& Other) = default;
	FText& FText::operator=(FText&& Other) = default;
#else
FText::FText(const FText& Source)
	: DisplayString(Source.DisplayString)
	, History(Source.History)
	, Flags(Source.Flags)
{
}

FText::FText(FText&& Source)
	: DisplayString(MoveTemp(Source.DisplayString))
	, History(MoveTemp(Source.History))
	, Flags(MoveTemp(Source.Flags))
{
}

FText& FText::operator=(const FText& Source)
{
	DisplayString = Source.DisplayString;
	History = Source.History;
	Flags = Source.Flags;

	return *this;
}

FText& FText::operator=(FText&& Source)
{
	DisplayString = MoveTemp(Source.DisplayString);
	History = MoveTemp(Source.History);
	Flags = MoveTemp(Source.Flags);

	return *this;
}
#endif

FText::FText( FString InSourceString )
	: DisplayString( new FString( MoveTemp(InSourceString) ))
	, Flags(0)
{
	History = MakeShareable(new FTextHistory_Base(DisplayString));
}

FText::FText( FString InSourceString, FString InNamespace, FString InKey, int32 InFlags )
	: DisplayString( FTextLocalizationManager::Get().GetDisplayString(InNamespace, InKey, &InSourceString) )
	, Flags(InFlags)
{
	History = MakeShareable(new FTextHistory_Base(InSourceString));
}


bool FText::IsEmptyOrWhitespace() const
{
	if (DisplayString.Get().IsEmpty())
	{
		return true;
	}

	for( const TCHAR Character : DisplayString.Get() )
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
		if( (InText.Flags & (1 << ETextFlag::CultureInvariant)) != 0 )
		{
			NewText.Flags = NewText.Flags | ETextFlag::Transient;
		}
		else
		{
			NewText.Flags = NewText.Flags | ETextFlag::CultureInvariant;
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
		if( (InText.Flags & (1 << ETextFlag::CultureInvariant)) != 0 )
		{
			NewText.Flags = NewText.Flags & ETextFlag::Transient;
		}
		else
		{
			NewText.Flags = NewText.Flags & ETextFlag::CultureInvariant;
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
		if( (InText.Flags & (1 << ETextFlag::CultureInvariant)) != 0 )
		{
			NewText.Flags = NewText.Flags | ETextFlag::Transient;
		}
		else
		{
			NewText.Flags = NewText.Flags | ETextFlag::CultureInvariant;
		}
	}

	return NewText;
}

FText FText::Format(const FText& Fmt,const FText& v1)
{
	FFormatOrderedArguments Arguments;
	Arguments.Add(v1);
	return FText::Format(Fmt, Arguments);
}

FText FText::Format(const FText& Fmt,const FText& v1,const FText& v2)
{
	FFormatOrderedArguments Arguments;
	Arguments.Add(v1);
	Arguments.Add(v2);
	return FText::Format(Fmt, Arguments);
}

FText FText::Format(const FText& Fmt,const FText& v1,const FText& v2,const FText& v3)
{
	FFormatOrderedArguments Arguments;
	Arguments.Add(v1);
	Arguments.Add(v2);
	Arguments.Add(v3);
	return FText::Format(Fmt, Arguments);
}

FText FText::Format(const FText& Fmt,const FText& v1,const FText& v2,const FText& v3,const FText& v4)
{
	FFormatOrderedArguments Arguments;
	Arguments.Add(v1);
	Arguments.Add(v2);
	Arguments.Add(v3);
	Arguments.Add(v4);
	return FText::Format(Fmt, Arguments);
}

class FTextFormatHelper
{
private:
	enum class EEscapeState
	{
		None,
		BeginEscaping,
		Escaping,
		EndEscaping,
	};

	enum class EBlockState
	{
		None,
		InBlock,
	};

public:
	DECLARE_DELEGATE_RetVal_TwoParams( const FFormatArgumentValue*, FGetArgumentValue, const FString&, int32 );

	static int32 EstimateArgumentValueLength(const FFormatArgumentValue& ArgumentValue)
	{
		switch(ArgumentValue.Type)
		{
		case EFormatArgumentType::Text:
			return ArgumentValue.TextValue->ToString().Len();
		case EFormatArgumentType::Int:
		case EFormatArgumentType::UInt:
		case EFormatArgumentType::Float:
		case EFormatArgumentType::Double:
			return 16;
		default:
			break;
		}

		return 0;
	}

	static void EnumerateParameters(const FText& Pattern, TArray<FString>& ParameterNames)
	{
		const FString& PatternString = Pattern.ToString();

		EEscapeState EscapeState = EEscapeState::None;
		EBlockState BlockState = EBlockState::None;

		FString ParameterName;

		for(int32 i = 0; i < PatternString.Len(); ++i)
		{
			switch( EscapeState )
			{
			case EEscapeState::None:
				{
					switch( PatternString[i] )
					{
					case '`':	{ EscapeState = EEscapeState::BeginEscaping; } break;
					}
				}
				break;
			case EEscapeState::BeginEscaping:
				{
					switch( PatternString[i] )
					{
						// Only begin EEscapeState::Escaping if there's a syntax character.
					case '{':
					case '}':
						{
							EscapeState = EEscapeState::Escaping;
						}
						break;
						// Cancel beginning EEscapeState::Escaping if the escape is itself escaped.
					case '`':
						{
							EscapeState = EEscapeState::None;
						}
						break;
						// Cancel beginning EEscapeState::Escaping if not a syntax character.
					default:
						{
							EscapeState = EEscapeState::None;
						}
						break;
					}
				}
				break;
			case EEscapeState::Escaping:
				{
					switch( PatternString[i] )
					{
					case '`':	{ EscapeState = EEscapeState::EndEscaping; } break;
					}
				}
				break;
			case EEscapeState::EndEscaping:
				{
					switch( PatternString[i] )
					{
					case '`':	{ EscapeState = EEscapeState::Escaping; } break; // Cancel ending EEscapeState::Escaping if the escape is itself escaped, copy over escaped character.
					default:	{ EscapeState = EEscapeState::None; } break;
					}
				}
				break;
			}

			if(EscapeState == EEscapeState::None)
			{
				switch( BlockState )
				{
				case EBlockState::None:
					{
						switch( PatternString[i] )
						{
						case '{':
							{
								BlockState = EBlockState::InBlock;
							}
							break;
						default:
							{
							}
							break;
						}
					}
					break;
				case EBlockState::InBlock:
					{
						switch( PatternString[i] )
						{
						case '}':
							{
								/** The following does a case-sensitive "TArray::AddUnique" by first checking to see if
								the parameter is in the ParameterNames list (using a case-sensitive comparison) followed
								by adding it to the ParameterNames */

								bool bIsCaseSensitiveUnique = true;
								for(auto It = ParameterNames.CreateConstIterator(); It; ++It)
								{
									if(It->Equals(ParameterName))
									{
										bIsCaseSensitiveUnique = false;
									}
								}
								if(bIsCaseSensitiveUnique)
								{
									ParameterNames.Add( ParameterName );
								}
								ParameterName.Empty();
								BlockState = EBlockState::None;
							}
							break;
						default:
							{
								ParameterName += PatternString[i];
							}
							break;
						}
					}
					break;
				}
			}
		}
	}

	static FText Format(const FText& Pattern, const int32 EstimatedArgumentValuesLength, FGetArgumentValue GetArgumentValue, const TSharedPtr<FTextHistory, ESPMode::ThreadSafe>& InHistory, bool bInRebuildText, bool bInRebuildAsSource)
	{
		checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
		//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

		const FString& PatternString = bInRebuildAsSource ? Pattern.BuildSourceString() : Pattern.ToString();

		// Guesstimate the result string size to minimize reallocations (it's okay to be a little bit over)
		// We'll assume that each argument will be replaced once, plus a bit of slack
		FString ResultString;
		ResultString.Reserve(PatternString.Len() + EstimatedArgumentValuesLength + 8);

		EEscapeState EscapeState = EEscapeState::None;
		EBlockState BlockState = EBlockState::None;

		FString ArgumentName;
		int32 ArgumentNumber = 0;

		for(int32 i = 0; i < PatternString.Len(); ++i)
		{
			switch( EscapeState )
			{
			case EEscapeState::None:
				{
					switch( PatternString[i] )
					{
					case '`':	{ EscapeState = EEscapeState::BeginEscaping; } break;
					}
				}
				break;
			case EEscapeState::BeginEscaping:
				{
					switch( PatternString[i] )
					{
						// Only begin EEscapeState::Escaping if there's a syntax character.
					case '{':
					case '}':
						{
							EscapeState = EEscapeState::Escaping;
							ResultString += PatternString[i]; // Characters are escaped, copy over.
						}
						break;
						// Cancel beginning EEscapeState::Escaping if the escape is itself escaped.
					case '`':
						{
							EscapeState = EEscapeState::None;
						}
						break;
						// Cancel beginning EEscapeState::Escaping if not a syntax character.
					default:
						{
							EscapeState = EEscapeState::None;
							ResultString += '`'; // Insert previously ignored escape marker.
						}
						break;
					}
				}
				break;
			case EEscapeState::Escaping:
				{
					switch( PatternString[i] )
					{
					case '`':	{ EscapeState = EEscapeState::EndEscaping; } break;
					default:	{ ResultString += PatternString[i]; } break; // Characters are escaped, copy over.
					}
				}
				break;
			case EEscapeState::EndEscaping:
				{
					switch( PatternString[i] )
					{
					case '`':	{ EscapeState = EEscapeState::Escaping; ResultString += PatternString[i]; } break; // Cancel ending EEscapeState::Escaping if the escape is itself escaped, copy over escaped character.
					default:	{ EscapeState = EEscapeState::None; } break;
					}
				}
				break;
			}

			if(EscapeState == EEscapeState::None)
			{
				switch( BlockState )
				{
				case EBlockState::None:
					{
						switch( PatternString[i] )
						{
						case '{':
							{
								BlockState = EBlockState::InBlock;
							}
							break;
						default:
							{
								ResultString += PatternString[i]; // Copy over characters.
							}
							break;
						}
					}
					break;
				case EBlockState::InBlock:
					{
						switch( PatternString[i] )
						{
						case '}':
							{
							
								const FFormatArgumentValue* const PossibleArgumentValue = GetArgumentValue.Execute(ArgumentName, ArgumentNumber++);
								if( PossibleArgumentValue )
								{
									const FFormatArgumentValue& ArgumentValue = *PossibleArgumentValue;
									switch(ArgumentValue.Type)
									{
									case EFormatArgumentType::Text:
										{
											// When doing a rebuild, all FText arguments need to be rebuilt during the Format
											if( bInRebuildText )
											{
												ArgumentValue.TextValue->Rebuild();
											}

											ResultString += bInRebuildAsSource ? ArgumentValue.TextValue->BuildSourceString() : ArgumentValue.TextValue->ToString();
										}
										break;
									case EFormatArgumentType::Int:
										{
											ResultString += FText::AsNumber(ArgumentValue.IntValue).ToString();
										}
										break;
									case EFormatArgumentType::UInt:
										{
											ResultString += FText::AsNumber(ArgumentValue.UIntValue).ToString();
										}
										break;
									case EFormatArgumentType::Float:
										{
											ResultString += FText::AsNumber(ArgumentValue.FloatValue).ToString();
										}
										break;
									case EFormatArgumentType::Double:
										{
											ResultString += FText::AsNumber(ArgumentValue.DoubleValue).ToString();
										}
										break;
									}
								}
								else
								{
									ResultString += TEXT("{");
									ResultString += ArgumentName;
									ResultString += TEXT("}");
								}
								ArgumentName.Reset();
								BlockState = EBlockState::None;
							}
							break;
						default:
							{
								ArgumentName += PatternString[i];
							}
							break;
						}
					}
					break;
				}
			}
		}

		FText Result = FText(MoveTemp(ResultString));
		Result.History = InHistory;
		if (!GIsEditor)
		{
			Result.Flags = Result.Flags | ETextFlag::Transient;
		}
		return Result;
	}
};

void FText::GetFormatPatternParameters(const FText& Pattern, TArray<FString>& ParameterNames)
{
	FTextFormatHelper::EnumerateParameters(Pattern, ParameterNames);
}

FText FText::Format(const FText& Pattern, const FFormatNamedArguments& Arguments)
{
	return FormatInternal(Pattern, Arguments, false, false);
}

FText FText::Format(const FText& Pattern, const FFormatOrderedArguments& Arguments)
{
	return FormatInternal(Pattern, Arguments, false, false);
}

FText FText::Format(const FText& Pattern, const TArray< FFormatArgumentData > InArguments)
{
	return FormatInternal(Pattern, InArguments, false, false);
}

FText FText::FormatInternal(const FText& Pattern, const FFormatNamedArguments& Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	int32 EstimatedArgumentValuesLength = 0;
	for (const auto& Arg : Arguments)
	{
		EstimatedArgumentValuesLength += FTextFormatHelper::EstimateArgumentValueLength(Arg.Value);
	}

	auto GetArgumentValue = [&Arguments](const FString& ArgumentName, int32 ArgumentNumber) -> const FFormatArgumentValue*
	{
		return Arguments.Find(ArgumentName);
	};

	return FTextFormatHelper::Format(Pattern, EstimatedArgumentValuesLength, FTextFormatHelper::FGetArgumentValue::CreateLambda(GetArgumentValue), MakeShareable(new FTextHistory_NamedFormat(Pattern, Arguments)), bInRebuildText, bInRebuildAsSource);
}

FText FText::FormatInternal(const FText& Pattern, const FFormatOrderedArguments& Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	int32 EstimatedArgumentValuesLength = 0;
	for (const auto& Arg : Arguments)
	{
		EstimatedArgumentValuesLength += FTextFormatHelper::EstimateArgumentValueLength(Arg);
	}

	auto GetArgumentValue = [&Arguments](const FString& ArgumentName, int32 ArgumentNumber) -> const FFormatArgumentValue*
	{
		int32 ArgumentIndex = INDEX_NONE;
		if (!LexicalConversion::TryParseString(ArgumentIndex, *ArgumentName))
		{
			// We failed to parse the argument name into a number...
			// We have existing code that is incorrectly using names in the format string when providing ordered arguments
			// ICU used to fallback to treating the index of the argument within the string as if it were the index specified 
			// by the argument name, so we need to emulate that behavior to avoid breaking some format operations
			ArgumentIndex = ArgumentNumber;
		}
		return ArgumentIndex != INDEX_NONE && ArgumentIndex < Arguments.Num() ? &(Arguments[ArgumentIndex]) : nullptr;
	};

	return FTextFormatHelper::Format(Pattern, EstimatedArgumentValuesLength, FTextFormatHelper::FGetArgumentValue::CreateLambda(GetArgumentValue), MakeShareable(new FTextHistory_OrderedFormat(Pattern, Arguments)), bInRebuildText, bInRebuildAsSource);
}

FText FText::FormatInternal(const FText& Pattern, const TArray< struct FFormatArgumentData > Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	int32 EstimatedArgumentValuesLength = 0;
	FFormatNamedArguments FormatNamedArguments;
	for (const auto& Arg : Arguments)
	{
		EstimatedArgumentValuesLength += FTextFormatHelper::EstimateArgumentValueLength(Arg.ArgumentValue);
		FormatNamedArguments.Add(Arg.ArgumentName.ToString(), FFormatArgumentValue(Arg.ArgumentValue));
	}

	auto GetArgumentValue = [&FormatNamedArguments](const FString& ArgumentName, int32 ArgumentNumber) -> const FFormatArgumentValue*
	{
		return FormatNamedArguments.Find(ArgumentName);
	};

	return FTextFormatHelper::Format(Pattern, EstimatedArgumentValuesLength, FTextFormatHelper::FGetArgumentValue::CreateLambda(GetArgumentValue), MakeShareable(new FTextHistory_ArgumentDataFormat(Pattern, Arguments)), bInRebuildText, bInRebuildAsSource);
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

/**
* Generate an FText that represents the passed number as a percentage in the current culture
*/

#define DEF_ASPERCENT_CAST(T1, T2) FText FText::AsPercent(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture) { return FText::AsPercentTemplate<T1, T2>(Val, Options, TargetCulture); }
#define DEF_ASPERCENT(T) DEF_ASPERCENT_CAST(T, T)
DEF_ASPERCENT(double)
DEF_ASPERCENT(float)
#undef DEF_ASPERCENT
#undef DEF_ASPERCENT_CAST

FString FText::GetInvariantTimeZone()
{
	return TEXT("Etc/Unknown");
}

bool FText::FindText( const FString& Namespace, const FString& Key, FText& OutText, const FString* const SourceString )
{
	TSharedPtr< FString, ESPMode::ThreadSafe > FoundString = FTextLocalizationManager::Get().FindDisplayString( Namespace, Key );

	if ( FoundString.IsValid() )
	{
		OutText = FText( SourceString ? *SourceString : FString(), Namespace, Key );
	}

	return FoundString.IsValid();
}

CORE_API FArchive& operator<<( FArchive& Ar, FText& Value )
{
	//When duplicating, the CDO is used as the template, then values for the instance are assigned.
	//If we don't duplicate the string, the CDO and the instance are both pointing at the same thing.
	//This would result in all subsequently duplicated objects stamping over formerly duplicated ones.

	// Older FText's stored their "SourceString", that is now stored in a history class so move it there
	if(Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FTEXT_HISTORY)
	{
		FString SourceStringToImplantIntoHistory;
		Ar << SourceStringToImplantIntoHistory;

		Value.History = MakeShareable(new FTextHistory_Base(SourceStringToImplantIntoHistory));

		// Namespaces and keys are no longer stored in the FText, we need to read them in and discard
		if( Ar.UE4Ver() >= VER_UE4_ADDED_NAMESPACE_AND_KEY_DATA_TO_FTEXT )
		{
			FString Namespace;
			FString Key;

			Ar << Namespace;
			Ar << Key;

			// Get the DisplayString using the namespace, key, and source string.
			Value.DisplayString = FTextLocalizationManager::Get().GetDisplayString(Namespace, Key, &SourceStringToImplantIntoHistory);
		}
	}

	if(Ar.IsPersistent())
	{
		Value.Flags &= ~(ETextFlag::ConvertedProperty); // Remove conversion flag before saving.
	}
	Ar << Value.Flags;

	if( Ar.UE4Ver() >= VER_UE4_FTEXT_HISTORY )
	{
		if(Ar.IsSaving())
		{
			// If there is no history, mark it, otherwise the history will serialize it's type
			if(!Value.History.IsValid())
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
					Value.History = MakeShareable(new FTextHistory_Base);
					break;
				}
			case ETextHistoryType::NamedFormat:
				{
					Value.History = MakeShareable(new FTextHistory_NamedFormat);
					break;
				}
			case ETextHistoryType::OrderedFormat:
				{
					Value.History = MakeShareable(new FTextHistory_OrderedFormat);
					break;
				}
			case ETextHistoryType::ArgumentFormat:
				{
					Value.History = MakeShareable(new FTextHistory_ArgumentDataFormat);
					break;
				}
			case ETextHistoryType::AsNumber:
				{
					Value.History = MakeShareable(new FTextHistory_AsNumber);
					break;
				}
			case ETextHistoryType::AsPercent:
				{
					Value.History = MakeShareable(new FTextHistory_AsPercent);
					break;
				}
			case ETextHistoryType::AsCurrency:
				{
					Value.History = MakeShareable(new FTextHistory_AsCurrency);
					break;
				}
			case ETextHistoryType::AsDate:
				{
					Value.History = MakeShareable(new FTextHistory_AsDate);
					break;
				}
			case ETextHistoryType::AsTime:
				{
					Value.History = MakeShareable(new FTextHistory_AsTime);
					break;
				}
			case ETextHistoryType::AsDateTime:
				{
					Value.History = MakeShareable(new FTextHistory_AsDateTime);
					break;
				}
			default:
				{
					Value.History.Reset();
					Value.DisplayString = FText::GetEmpty().DisplayString;
				}
			}
		}

		if(Value.History.IsValid())
		{
			Value.History->Serialize(Ar);
			Value.History->SerializeForDisplayString(Ar, Value.DisplayString);
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

	return Ar;
}

#if WITH_EDITOR
FText FText::ChangeKey( FString Namespace, FString Key, const FText& Text )
{
	return FText( *Text.History->GetSourceString().Get(), MoveTemp( Namespace ), MoveTemp( Key ) );
}
#endif

FText FText::CreateNumericalText(FString InSourceString)
{
	FText NewText = FText( MoveTemp( InSourceString ) );
	if (!GIsEditor)
	{
		NewText.Flags |= ETextFlag::Transient;
	}
	return NewText;
}

FText FText::CreateChronologicalText(FString InSourceString)
{
	FText NewText = FText( MoveTemp( InSourceString ) );
	if (!GIsEditor)
	{
		NewText.Flags |= ETextFlag::Transient;
	}
	return NewText;
}

FText FText::FromName( const FName& Val) 
{
	FText NewText = FText( Val.ToString() );

	if (!GIsEditor)
	{
		NewText.Flags |= ETextFlag::CultureInvariant;
	}

	return NewText; 
}

FText FText::FromString( FString String )
{
	FText NewText = FText( MoveTemp(String) );

	if (!GIsEditor)
	{
		NewText.Flags |= ETextFlag::CultureInvariant;
	}

	return NewText;
}

FText FText::AsCultureInvariant( FString String )
{
	FText NewText = FText( String );
	NewText.Flags |= ETextFlag::CultureInvariant;

	return NewText;
}

FText FText::AsCultureInvariant( FText Text )
{
	FText NewText = FText( Text );
	NewText.Flags |= ETextFlag::CultureInvariant;

	return NewText;
}

const FString& FText::ToString() const
{
	Rebuild();

	return DisplayString.Get();
}

FString FText::BuildSourceString() const
{
	if(History.IsValid())
	{
		return History->ToText(true).ToString();
	}
	return DisplayString.Get();
}

void FText::Rebuild() const
{
	if(History.IsValid())
	{
		History->Rebuild(DisplayString);
	}
}

bool FText::ShouldGatherForLocalization() const
{
	auto SourceString = GetSourceString();

	auto IsAllWhitespace = [](FString& String) -> bool
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

	return !((Flags & ETextFlag::CultureInvariant) || (Flags & ETextFlag::Transient)) && SourceString.IsValid() && !SourceString->IsEmpty() && !IsAllWhitespace(*SourceString);
}

TSharedPtr< FString, ESPMode::ThreadSafe > FText::GetSourceString() const
{
	if(History.IsValid())
	{
		TSharedPtr< FString, ESPMode::ThreadSafe > SourceString = History->GetSourceString();
		if(SourceString.IsValid())
		{
			return SourceString;
		}
	}

	return DisplayString;
}

bool FText::IdenticalTo( const FText& Other ) const
{
	// If both instances point to the same string, then both instances are considered identical
	// This is fast as it skips a lexical compare, however it can also return true for two instances that have identical strings, but in different pointers
	return DisplayString == Other.DisplayString;
}

void FText::GetSourceTextsFromFormatHistory(TArray<FText>& OutSourceTexts) const
{
	History->GetSourceTextsFromFormatHistory(*(this), OutSourceTexts);
}

FTextSnapshot::FTextSnapshot()
	: DisplayStringPtr()
	, HistoryRevision(INDEX_NONE)
	, Flags(0)
{
}

FTextSnapshot::FTextSnapshot(const FText& InText)
	: DisplayStringPtr(InText.DisplayString)
	, HistoryRevision(InText.History.IsValid() ? InText.History->Revision : INDEX_NONE)
	, Flags(InText.Flags)
{
}

bool FTextSnapshot::IdenticalTo(const FText& InText) const
{
	// Make sure the string is up-to-date with the current culture
	// (this usually happens when ToString() is called)
	InText.Rebuild();

	const int32 InHistoryRevision = InText.History.IsValid() ? InText.History->Revision : INDEX_NONE;
	return DisplayStringPtr == InText.DisplayString
		&& HistoryRevision == InHistoryRevision
		&& Flags == InText.Flags;
}

bool FTextSnapshot::IsDisplayStringEqualTo(const FText& InText) const
{
	// Make sure the string is up-to-date with the current culture
	// (this usually happens when ToString() is called)
	InText.Rebuild();

	// We have to assume that the display string has changed if the history of the text has changed
	// (due to a culture change), as we no longer have the old display string to compare against
	const int32 InHistoryRevision = InText.History.IsValid() ? InText.History->Revision : INDEX_NONE;
	return HistoryRevision == InHistoryRevision && DisplayStringPtr.IsValid() && DisplayStringPtr->Equals(InText.ToString(), ESearchCase::CaseSensitive);
}

FScopedTextIdentityPreserver::FScopedTextIdentityPreserver(FText& InTextToPersist)
	: HadFoundNamespaceAndKey(false)
	, Flags(InTextToPersist.Flags)
	, TextToPersist(InTextToPersist)
{
	// Empty display strings can't have a namespace or key.
	if (GIsEditor && !InTextToPersist.DisplayString->IsEmpty())
	{
		// Save off namespace and key to be restored later.
		HadFoundNamespaceAndKey = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(InTextToPersist.DisplayString, Namespace, Key);
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

		// Make the history so that it can attempt find and serialize the identity for this source string when the history itself is serialized.
		TextToPersist.History = MakeShareable(new FTextHistory_Base(*SourceString));

		// Create/update the display string instance for this identity in the text localization manager...
		const FTextDisplayStringRef DisplayString = FTextLocalizationManager::Get().GetDisplayString(Namespace, Key, SourceString);

		// ... and set it so that the text's history's serialization will properly map this display string instance back to the identity we intended to preserve and restore.
		TextToPersist.DisplayString = DisplayString;
	}
}

#undef LOCTEXT_NAMESPACE
