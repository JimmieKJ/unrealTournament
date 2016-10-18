// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU
#include "ICUUtilities.h"
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(push)
	#pragma warning(disable:28251)
	#pragma warning(disable:28252)
	#pragma warning(disable:28253)
#endif
	#include <unicode/ucnv.h>
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(pop)
#endif

namespace ICUUtilities
{
	FStringConverter::FStringConverter()
		: ICUConverter(nullptr)
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		ICUConverter = ucnv_open(FPlatformString::GetEncodingName(), &ICUStatus);
		check(U_SUCCESS(ICUStatus));
	}

	FStringConverter::~FStringConverter()
	{
		ucnv_close(ICUConverter);
	}

	void FStringConverter::ConvertString(const FString& Source, icu::UnicodeString& Destination, const bool ShouldNullTerminate)
	{
		ConvertString(*Source, 0, Source.Len(), Destination, ShouldNullTerminate);
	}

	void FStringConverter::ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, icu::UnicodeString& Destination, const bool ShouldNullTerminate)
	{
		if (SourceLen > 0)
		{
			UErrorCode ICUStatus = U_ZERO_ERROR;

			ucnv_reset(ICUConverter);

			// Get the internal buffer of the string, we're going to use it as scratch space
			const int32_t DestinationCapacityUChars = SourceLen * 2;
			UChar* InternalStringBuffer = Destination.getBuffer(DestinationCapacityUChars);

			// Perform the conversion into the string buffer
			const int32_t SourceSizeBytes = SourceLen * sizeof(TCHAR);
			const int32_t DestinationLength = ucnv_toUChars(ICUConverter, InternalStringBuffer, DestinationCapacityUChars, reinterpret_cast<const char*>(Source + SourceStartIndex), SourceSizeBytes, &ICUStatus);

			// Optionally null terminate the string
			if (ShouldNullTerminate)
			{
				InternalStringBuffer[DestinationLength] = 0;
			}

			// Size it back down to the correct size and release our lock on the string buffer
			Destination.releaseBuffer(DestinationLength);

			check(U_SUCCESS(ICUStatus));
		}
		else
		{
			Destination.remove();
		}
	}

	icu::UnicodeString FStringConverter::ConvertString(const FString& Source, const bool ShouldNullTerminate)
	{
		icu::UnicodeString Destination;
		ConvertString(Source, Destination, ShouldNullTerminate);
		return Destination;
	}

	icu::UnicodeString FStringConverter::ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, const bool ShouldNullTerminate)
	{
		icu::UnicodeString Destination;
		ConvertString(Source, SourceStartIndex, SourceLen, Destination, ShouldNullTerminate);
		return Destination;
	}

	void FStringConverter::ConvertString(const icu::UnicodeString& Source, FString& Destination)
	{
		if (Source.length() > 0)
		{
			UErrorCode ICUStatus = U_ZERO_ERROR;

			ucnv_reset(ICUConverter);
			
			// Get the internal buffer of the string, we're going to use it as scratch space
			TArray<TCHAR>& InternalStringBuffer = Destination.GetCharArray();
				
			// Work out the maximum size required and resize the buffer so it can hold enough data
			const int32_t DestinationCapacityBytes = UCNV_GET_MAX_BYTES_FOR_STRING(Source.length(), ucnv_getMaxCharSize(ICUConverter));
			const int32 DestinationCapacityTCHARs = DestinationCapacityBytes / sizeof(TCHAR);
			InternalStringBuffer.SetNumUninitialized(DestinationCapacityTCHARs);

			// Perform the conversion into the string buffer, and then null terminate the FString and size it back down to the correct size
			const int32_t DestinationSizeBytes = ucnv_fromUChars(ICUConverter, reinterpret_cast<char*>(InternalStringBuffer.GetData()), DestinationCapacityBytes, Source.getBuffer(), Source.length(), &ICUStatus);
			const int32 DestinationSizeTCHARs = DestinationSizeBytes / sizeof(TCHAR);
			InternalStringBuffer[DestinationSizeTCHARs] = 0;
			InternalStringBuffer.SetNum(DestinationSizeTCHARs + 1, /*bAllowShrinking*/false); // the array size includes null

			check(U_SUCCESS(ICUStatus));
		}
		else
		{
			Destination.Empty();
		}
	}

	FString FStringConverter::ConvertString(const icu::UnicodeString& Source)
	{
		FString Destination;
		ConvertString(Source, Destination);
		return Destination;
	}

	void ConvertString(const FString& Source, icu::UnicodeString& Destination, const bool ShouldNullTerminate)
	{
		if (Source.Len() > 0)
		{
			FStringConverter StringConverter;
			StringConverter.ConvertString(Source, Destination, ShouldNullTerminate);
		}
		else
		{
			Destination.remove();
		}
	}

	void ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, icu::UnicodeString& Destination, const bool ShouldNullTerminate)
	{
		if (SourceLen > 0)
		{
			FStringConverter StringConverter;
			StringConverter.ConvertString(Source, SourceStartIndex, SourceLen, Destination, ShouldNullTerminate);
		}
		else
		{
			Destination.remove();
		}
	}

	icu::UnicodeString ConvertString(const FString& Source, const bool ShouldNullTerminate)
	{
		icu::UnicodeString Destination;
		ConvertString(Source, Destination, ShouldNullTerminate);
		return Destination;
	}

	icu::UnicodeString ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, const bool ShouldNullTerminate)
	{
		icu::UnicodeString Destination;
		ConvertString(Source, SourceStartIndex, SourceLen, Destination, ShouldNullTerminate);
		return Destination;
	}

	void ConvertString(const icu::UnicodeString& Source, FString& Destination)
	{
		if (Source.length() > 0)
		{
			FStringConverter StringConverter;
			StringConverter.ConvertString(Source, Destination);
		}
		else
		{
			Destination.Empty();
		}
	}

	FString ConvertString(const icu::UnicodeString& Source)
	{
		FString Destination;
		ConvertString(Source, Destination);
		return Destination;
	}
}
#endif
