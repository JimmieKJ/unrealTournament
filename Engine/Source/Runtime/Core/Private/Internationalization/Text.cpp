// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU
#include "ICUText.h"
#else
#include "LegacyText.h"
#endif

#include "TextData.h"
#include "TextHistory.h"

#include "DebugSerializationFlags.h"

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
	: TextData(GetEmpty().TextData)
	, Flags(0)
{
}

FText::FText( EInitToEmptyString )
	: TextData(new TLocalizedTextData<FTextHistory_Base>(MakeShareable(new FString())))
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

FText::FText( FString InSourceString )
	: TextData(new TGeneratedTextData<FTextHistory_Base>(InSourceString))
	, Flags(0)
{
	TextData->SetTextHistory(FTextHistory_Base(MoveTemp(InSourceString)));
}

FText::FText( FString InSourceString, const FString& InNamespace, const FString& InKey, uint32 InFlags )
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

FText FText::Format(FText Fmt, FText v1)
{
	FFormatOrderedArguments Arguments;
	Arguments.Add(MoveTemp(v1));
	return FText::Format(MoveTemp(Fmt), MoveTemp(Arguments));
}

FText FText::Format(FText Fmt, FText v1, FText v2)
{
	FFormatOrderedArguments Arguments;
	Arguments.Add(MoveTemp(v1));
	Arguments.Add(MoveTemp(v2));
	return FText::Format(MoveTemp(Fmt), MoveTemp(Arguments));
}

FText FText::Format(FText Fmt, FText v1, FText v2, FText v3)
{
	FFormatOrderedArguments Arguments;
	Arguments.Add(MoveTemp(v1));
	Arguments.Add(MoveTemp(v2));
	Arguments.Add(MoveTemp(v3));
	return FText::Format(MoveTemp(Fmt), MoveTemp(Arguments));
}

FText FText::Format(FText Fmt, FText v1, FText v2, FText v3, FText v4)
{
	FFormatOrderedArguments Arguments;
	Arguments.Add(MoveTemp(v1));
	Arguments.Add(MoveTemp(v2));
	Arguments.Add(MoveTemp(v3));
	Arguments.Add(MoveTemp(v4));
	return FText::Format(MoveTemp(Fmt), MoveTemp(Arguments));
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
	struct FArgumentName
	{
		FArgumentName(const TCHAR* InNamePtr, const int32 InNameLen)
			: NamePtr(InNamePtr)
			, NameLen(InNameLen)
		{
		}

		TOptional<int32> AsIndex() const
		{
			int32 Index = 0;
			for (int32 NameOffset = 0; NameOffset < NameLen; ++NameOffset)
			{
				const TCHAR C = *(NamePtr + NameOffset);

				if (C >= '0' && C <= '9')
				{
					Index *= 10;
					Index += C - '0';
				}
				else
				{
					return TOptional<int32>();
				}
			}
			return Index;
		}

		const TCHAR* NamePtr;
		int32 NameLen;
	};

	typedef TFunctionRef<const FFormatArgumentValue*(const FArgumentName&, int32)> FGetArgumentValue;

	static int32 EstimateArgumentValueLength(const FFormatArgumentValue& ArgumentValue)
	{
		switch(ArgumentValue.GetType())
		{
		case EFormatArgumentType::Text:
			return ArgumentValue.GetTextValue().ToString().Len();
		case EFormatArgumentType::Int:
		case EFormatArgumentType::UInt:
		case EFormatArgumentType::Float:
		case EFormatArgumentType::Double:
			return 20;
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

	static FString Format(const FText& Pattern, const int32 EstimatedArgumentValuesLength, FGetArgumentValue GetArgumentValue, bool bInRebuildText, bool bInRebuildAsSource)
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

		int32 ArgumentNameIndex = INDEX_NONE;
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
								if (ArgumentNameIndex == INDEX_NONE)
								{
									// Can happen for empty argument blocks
									ArgumentNameIndex = i;
								}

								const FArgumentName ArgumentName = FArgumentName(&PatternString[ArgumentNameIndex], i - ArgumentNameIndex);
								const FFormatArgumentValue* const PossibleArgumentValue = GetArgumentValue(ArgumentName, ArgumentNumber++);
								if( PossibleArgumentValue )
								{
									const FFormatArgumentValue& ArgumentValue = *PossibleArgumentValue;
									switch(ArgumentValue.GetType())
									{
									case EFormatArgumentType::Text:
										{
											const FText& TextValue = ArgumentValue.GetTextValue();

											// When doing a rebuild, all FText arguments need to be rebuilt during the Format
											if( bInRebuildText )
											{
												TextValue.Rebuild();
											}

											ResultString += bInRebuildAsSource ? TextValue.BuildSourceString() : TextValue.ToString();
										}
										break;
									case EFormatArgumentType::Int:
										{
											ResultString += FText::AsNumber(ArgumentValue.GetIntValue()).ToString();
										}
										break;
									case EFormatArgumentType::UInt:
										{
											ResultString += FText::AsNumber(ArgumentValue.GetUIntValue()).ToString();
										}
										break;
									case EFormatArgumentType::Float:
										{
											ResultString += FText::AsNumber(ArgumentValue.GetFloatValue()).ToString();
										}
										break;
									case EFormatArgumentType::Double:
										{
											ResultString += FText::AsNumber(ArgumentValue.GetDoubleValue()).ToString();
										}
										break;
									}
								}
								else
								{
									ResultString.AppendChar('{');
									ResultString.AppendChars(ArgumentName.NamePtr, ArgumentName.NameLen);
									ResultString.AppendChar('}');
								}
								ArgumentNameIndex = INDEX_NONE;
								BlockState = EBlockState::None;
							}
							break;
						default:
							{
								if (ArgumentNameIndex == INDEX_NONE)
								{
									ArgumentNameIndex = i;
								}
							}
							break;
						}
					}
					break;
				}
			}
		}

		return ResultString;
	}
};

void FText::GetFormatPatternParameters(const FText& Pattern, TArray<FString>& ParameterNames)
{
	FTextFormatHelper::EnumerateParameters(Pattern, ParameterNames);
}

FText FText::Format(FText Pattern, FFormatNamedArguments Arguments)
{
	return FormatInternal(MoveTemp(Pattern), MoveTemp(Arguments), false, false);
}

FText FText::Format(FText Pattern, FFormatOrderedArguments Arguments)
{
	return FormatInternal(MoveTemp(Pattern), MoveTemp(Arguments), false, false);
}

FText FText::Format(FText Pattern, TArray< FFormatArgumentData > InArguments)
{
	return FormatInternal(MoveTemp(Pattern), MoveTemp(InArguments), false, false);
}

FText FText::FormatInternal(FText Pattern, FFormatNamedArguments Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	int32 EstimatedArgumentValuesLength = 0;
	for (const auto& Arg : Arguments)
	{
		EstimatedArgumentValuesLength += FTextFormatHelper::EstimateArgumentValueLength(Arg.Value);
	}

	auto GetArgumentValue = [&Arguments](const FTextFormatHelper::FArgumentName& ArgumentName, int32 ArgumentNumber) -> const FFormatArgumentValue*
	{
		for (const auto& Pair : Arguments)
		{
			if (ArgumentName.NameLen == Pair.Key.Len() && FCString::Strnicmp(ArgumentName.NamePtr, *Pair.Key, ArgumentName.NameLen) == 0)
			{
				return &Pair.Value;
			}
		}
		return nullptr;
	};

	FString ResultString = FTextFormatHelper::Format(Pattern, EstimatedArgumentValuesLength, FTextFormatHelper::FGetArgumentValue(GetArgumentValue), bInRebuildText, bInRebuildAsSource);
	
	FText Result = FText(MakeShareable(new TGeneratedTextData<FTextHistory_NamedFormat>(MoveTemp(ResultString), FTextHistory_NamedFormat(MoveTemp(Pattern), MoveTemp(Arguments)))));
	if (!GIsEditor)
	{
		Result.Flags |= ETextFlag::Transient;
	}
	return Result;
}

FText FText::FormatInternal(FText Pattern, FFormatOrderedArguments Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	int32 EstimatedArgumentValuesLength = 0;
	for (const auto& Arg : Arguments)
	{
		EstimatedArgumentValuesLength += FTextFormatHelper::EstimateArgumentValueLength(Arg);
	}

	auto GetArgumentValue = [&Arguments](const FTextFormatHelper::FArgumentName& ArgumentName, int32 ArgumentNumber) -> const FFormatArgumentValue*
	{
		TOptional<int32> ArgumentIndex = ArgumentName.AsIndex();
		if (!ArgumentIndex.IsSet())
		{
			// We failed to parse the argument name into a number...
			// We have existing code that is incorrectly using names in the format string when providing ordered arguments
			// ICU used to fallback to treating the index of the argument within the string as if it were the index specified 
			// by the argument name, so we need to emulate that behavior to avoid breaking some format operations
			ArgumentIndex = ArgumentNumber;
		}
		return Arguments.IsValidIndex(ArgumentIndex.GetValue()) ? &(Arguments[ArgumentIndex.GetValue()]) : nullptr;
	};

	FString ResultString = FTextFormatHelper::Format(Pattern, EstimatedArgumentValuesLength, FTextFormatHelper::FGetArgumentValue(GetArgumentValue), bInRebuildText, bInRebuildAsSource);

	FText Result = FText(MakeShareable(new TGeneratedTextData<FTextHistory_OrderedFormat>(MoveTemp(ResultString), FTextHistory_OrderedFormat(MoveTemp(Pattern), MoveTemp(Arguments)))));
	if (!GIsEditor)
	{
		Result.Flags |= ETextFlag::Transient;
	}
	return Result;
}

FText FText::FormatInternal(FText Pattern, TArray< struct FFormatArgumentData > Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	int32 EstimatedArgumentValuesLength = 0;
	for (const auto& Arg : Arguments)
	{
		EstimatedArgumentValuesLength += FTextFormatHelper::EstimateArgumentValueLength(Arg.ArgumentValue);
	}

	FFormatArgumentValue TmpArgumentValue;
	FFormatArgumentValue* TmpArgumentValuePtr = &TmpArgumentValue; // Need to do this to avoid the error "address of stack memory associated with local variable 'TmpArgumentValue' returned"
	auto GetArgumentValue = [&Arguments, &TmpArgumentValuePtr](const FTextFormatHelper::FArgumentName& ArgumentName, int32 ArgumentNumber) -> const FFormatArgumentValue*
	{
		for (const auto& Arg : Arguments)
		{
			if (ArgumentName.NameLen == Arg.ArgumentName.Len() && FCString::Strnicmp(ArgumentName.NamePtr, *Arg.ArgumentName, ArgumentName.NameLen) == 0)
			{
				(*TmpArgumentValuePtr) = FFormatArgumentValue(Arg.ArgumentValue);
				return TmpArgumentValuePtr;
			}
		}
		return nullptr;
	};

	FString ResultString = FTextFormatHelper::Format(Pattern, EstimatedArgumentValuesLength, FTextFormatHelper::FGetArgumentValue(GetArgumentValue), bInRebuildText, bInRebuildAsSource);

	FText Result = FText(MakeShareable(new TGeneratedTextData<FTextHistory_ArgumentDataFormat>(MoveTemp(ResultString), FTextHistory_ArgumentDataFormat(MoveTemp(Pattern), MoveTemp(Arguments)))));
	if (!GIsEditor)
	{
		Result.Flags |= ETextFlag::Transient;
	}
	return Result;
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
	TSharedPtr< FString, ESPMode::ThreadSafe > FoundString = FTextLocalizationManager::Get().FindDisplayString( Namespace, Key, SourceString );

	if ( FoundString.IsValid() )
	{
		OutText = FText( SourceString ? *SourceString : FString(), Namespace, Key );
	}

	return FoundString.IsValid();
}

CORE_API FArchive& operator<<(FArchive& Ar, FText& Value)
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
			Value.Flags &= ~(ETextFlag::ConvertedProperty); // Remove conversion flag before saving.
		}
	}
	Ar << Value.Flags;

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

	return Ar;
}

#if WITH_EDITOR
FText FText::ChangeKey( FString Namespace, FString Key, const FText& Text )
{
	return FText( *Text.TextData->GetTextHistory().GetSourceString(), MoveTemp( Namespace ), MoveTemp( Key ), Text.Flags );
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

FTextSnapshot::FTextSnapshot()
	: TextDataPtr()
	, HistoryRevision(INDEX_NONE)
	, Flags(0)
{
}

FTextSnapshot::FTextSnapshot(const FText& InText)
	: TextDataPtr(InText.TextData)
	, HistoryRevision(InText.TextData->GetTextHistory().Revision)
	, Flags(InText.Flags)
{
}

bool FTextSnapshot::IdenticalTo(const FText& InText) const
{
	// Make sure the string is up-to-date with the current culture
	// (this usually happens when ToString() is called)
	InText.Rebuild();

	return TextDataPtr == InText.TextData 
		&& HistoryRevision == InText.TextData->GetTextHistory().Revision
		&& Flags == InText.Flags;;
}

bool FTextSnapshot::IsDisplayStringEqualTo(const FText& InText) const
{
	// Make sure the string is up-to-date with the current culture
	// (this usually happens when ToString() is called)
	InText.Rebuild();

	// We have to assume that the display string has changed if the history of the text has changed
	// (due to a culture change), as we no longer have the old display string to compare against
	return HistoryRevision == InText.TextData->GetTextHistory().Revision 
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
		TextToPersist.TextData = MakeShareable(new TLocalizedTextData<FTextHistory_Base>(MoveTemp(DisplayString), FTextHistory_Base(*SourceString)));
	}
}

#undef LOCTEXT_NAMESPACE
