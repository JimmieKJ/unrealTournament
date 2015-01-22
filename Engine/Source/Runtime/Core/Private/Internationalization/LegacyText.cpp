// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if !UE_ENABLE_ICU
#include "Text.h"

bool FText::IsWhitespace( const TCHAR Char )
{
	return FChar::IsWhitespace(Char);
}

FText FText::AsDate( const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const FString& TimeZone, const FCulturePtr& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return FText::FromString( DateTime.ToString( TEXT("%Y.%m.%d") ) );
}

FText FText::AsTime( const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulturePtr& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return FText::FromString( DateTime.ToString( TEXT("%H.%M.%S") ) );
}

FText FText::AsTimespan( const FTimespan& Timespan, const FCulturePtr& TargetCulture)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FDateTime DateTime(Timespan.GetTicks());
	return FText::FromString( DateTime.ToString( TEXT("%H.%M.%S") ) );
}

FText FText::AsDateTime( const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulturePtr& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return FText::FromString(DateTime.ToString(TEXT("%Y.%m.%d-%H.%M.%S")));
}

FText FText::AsMemory( SIZE_T NumBytes, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FFormatNamedArguments Args;

	if (NumBytes < 1024)
	{
		Args.Add( TEXT("Number"), FText::AsNumber( (uint64)NumBytes, Options, TargetCulture) );
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

int32 FText::CompareTo( const FText& Other, const ETextComparisonLevel::Type ComparisonLevel ) const
{
	return FCString::Strcmp( *DisplayString.Get(), *Other.DisplayString.Get() );
}

int32 FText::CompareToCaseIgnored( const FText& Other ) const
{
	return FCString::Stricmp( *DisplayString.Get(), *Other.DisplayString.Get() );
}

bool FText::EqualTo( const FText& Other, const ETextComparisonLevel::Type ComparisonLevel ) const
{
	return FCString::Strcmp( *DisplayString.Get(), *Other.DisplayString.Get() ) == 0;
}

bool FText::EqualToCaseIgnored( const FText& Other ) const
{
	return CompareToCaseIgnored( Other ) == 0;
}

FText::FSortPredicate::FSortPredicate(const ETextComparisonLevel::Type ComparisonLevel)
{

}

bool FText::FSortPredicate::operator()(const FText& A, const FText& B) const
{
	return A.ToString() < B.ToString();
}

namespace
{
	namespace EEscapeState
	{
		enum Type
		{
			None,
			BeginEscaping,
			Escaping,
			EndEscaping
		};
	}

	namespace EBlockState
	{
		enum Type
		{
			None,
			InBlock
		};
	}
}

DECLARE_DELEGATE_RetVal_OneParam( const FFormatArgumentValue*, FGetArgumentValue, const FString );

class FLegacyTextHelper
{
public:
	static void EnumerateParameters(const FText& Pattern, TArray<FString>& ParameterNames)
	{
		const FString& PatternString = Pattern.ToString();

		EEscapeState::Type EscapeState = EEscapeState::None;
		EBlockState::Type BlockState = EBlockState::None;

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

	static FText Format(const FText& Pattern, FGetArgumentValue GetArgumentValue)
	{
		checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
		//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

		const FString& PatternString = Pattern.ToString();

		FText Result;
		FString& ResultString = Result.DisplayString.Get();

		EEscapeState::Type EscapeState = EEscapeState::None;
		EBlockState::Type BlockState = EBlockState::None;

		FString ArgumentName;

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
							
								const FFormatArgumentValue* const PossibleArgumentValue = GetArgumentValue.Execute(ArgumentName);
								if( PossibleArgumentValue )
								{
									FString ArgumentValueAsString;
									const FFormatArgumentValue& ArgumentValue = *PossibleArgumentValue;
									switch(ArgumentValue.Type)
									{
									case EFormatArgumentType::Text:
										{
											ArgumentValueAsString = ArgumentValue.TextValue->ToString();
										}
										break;
									case EFormatArgumentType::Int:
										{
											ArgumentValueAsString = FText::AsNumber(ArgumentValue.IntValue).ToString();
										}
										break;
									case EFormatArgumentType::UInt:
										{
											ArgumentValueAsString = FText::AsNumber(ArgumentValue.UIntValue).ToString();
										}
										break;
									case EFormatArgumentType::Float:
										{
											ArgumentValueAsString = FText::AsNumber(ArgumentValue.FloatValue).ToString();
										}
										break;
									case EFormatArgumentType::Double:
										{
											ArgumentValueAsString = FText::AsNumber(ArgumentValue.DoubleValue).ToString();
										}
										break;
									}
									ResultString += ArgumentValueAsString;
								}
								else
								{
									ResultString += FString("{") + ArgumentName + FString("}");
								}
								ArgumentName.Empty();
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

		if (!GIsEditor)
		{
			Result.Flags = Result.Flags | ETextFlag::Transient;
		}

		return Result;
	}
};

void FText::GetFormatPatternParameters(const FText& Pattern, TArray<FString>& ParameterNames)
{
	FLegacyTextHelper::EnumerateParameters(Pattern, ParameterNames);
}

FText FText::Format(const FText& Pattern, const FFormatNamedArguments& Arguments)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	struct FArgumentGetter
	{
		const FFormatNamedArguments& Arguments;
		FArgumentGetter(const FFormatNamedArguments& InArguments) : Arguments(InArguments) {}
		const FFormatArgumentValue* GetArgumentValue( const FString ArgumentName )
		{
			return Arguments.Find(ArgumentName);
		}
	};

	FArgumentGetter ArgumentGetter(Arguments);
	return FLegacyTextHelper::Format(Pattern, FGetArgumentValue::CreateRaw(&ArgumentGetter, &FArgumentGetter::GetArgumentValue));
}

FText FText::Format(const FText& Pattern, const FFormatOrderedArguments& Arguments)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	struct FArgumentGetter
	{
		const FFormatOrderedArguments& Arguments;
		FArgumentGetter(const FFormatOrderedArguments& InArguments) : Arguments(InArguments) {}
		const FFormatArgumentValue* GetArgumentValue( const FString ArgumentName )
		{
			int32 ArgumentIndex = INDEX_NONE;
			if( ArgumentName.IsNumeric() )
			{
				ArgumentIndex = FCString::Atoi(*ArgumentName);
			}
			return ArgumentIndex != INDEX_NONE && ArgumentIndex < Arguments.Num() ? &(Arguments[ArgumentIndex]) : NULL;
		}
	};

	FArgumentGetter ArgumentGetter(Arguments);
	return FLegacyTextHelper::Format(Pattern, FGetArgumentValue::CreateRaw(&ArgumentGetter, &FArgumentGetter::GetArgumentValue));
}

FText FText::Format(const FText& Pattern, const TArray< FFormatArgumentData > InArguments)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	//SCOPE_CYCLE_COUNTER( STAT_TextFormat );

	struct FArgumentGetter
	{
		const FFormatNamedArguments& Arguments;
		FArgumentGetter(const FFormatNamedArguments& InArguments) : Arguments(InArguments) {}
		const FFormatArgumentValue* GetArgumentValue( const FString ArgumentName )
		{
			return Arguments.Find(ArgumentName);
		}
	};

	FFormatNamedArguments FormatNamedArguments;
	for(auto It = InArguments.CreateConstIterator(); It; ++It)
	{
		FormatNamedArguments.Add(It->ArgumentName.ToString(), FFormatArgumentValue(It->ArgumentValue));
	}

	FArgumentGetter ArgumentGetter(FormatNamedArguments);
	return FLegacyTextHelper::Format(Pattern, FGetArgumentValue::CreateRaw(&ArgumentGetter, &FArgumentGetter::GetArgumentValue));
}

FText FText::FormatInternal(const FText& Pattern, const FFormatNamedArguments& Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	return Format(Pattern, Arguments);
}

FText FText::FormatInternal(const FText& Pattern, const FFormatOrderedArguments& Arguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	return Format(Pattern, Arguments);
}

FText FText::FormatInternal(const FText& Pattern, const TArray< struct FFormatArgumentData > InArguments, bool bInRebuildText, bool bInRebuildAsSource)
{
	return Format(Pattern, InArguments);
}

#endif
