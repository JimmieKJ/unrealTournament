// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU
#include "ICUUtilities.h"
#include <unicode/ucnv.h>

namespace ICUUtilities
{
	void ConvertString(const FString& Source, icu::UnicodeString& Destination, const bool ShouldNullTerminate)
	{
		if( Source.Len() )
		{
			const char* const SourceEncoding = FPlatformString::GetEncodingName();
	
			UErrorCode ICUStatus = U_ZERO_ERROR;
		
			UConverter* ICUConverter = ucnv_open(SourceEncoding, &ICUStatus);
			if( U_SUCCESS(ICUStatus) ) 
			{
				// Get the internal buffer of the string, we're going to use it as scratch space
				const int32_t DestinationCapacityUChars = Source.Len() * 2;
				UChar* InternalStringBuffer = Destination.getBuffer(DestinationCapacityUChars);

				// Perform the conversion into the string buffer
				const int32_t SourceSizeBytes = Source.Len() * sizeof(TCHAR);
				const int32_t DestinationLength = ucnv_toUChars(ICUConverter, InternalStringBuffer, DestinationCapacityUChars, reinterpret_cast<const char*>(*Source), SourceSizeBytes, &ICUStatus);

				// Optionally null terminate the string
				if( ShouldNullTerminate )
				{
					InternalStringBuffer[DestinationLength] = 0;
				}

				// Size it back down to the correct size and release our lock on the string buffer
				Destination.releaseBuffer(DestinationLength);

				ucnv_close(ICUConverter);
			}

			check( U_SUCCESS(ICUStatus) );
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

	void ConvertString(const icu::UnicodeString& Source, FString& Destination)
	{
		if( Source.length() )
		{
			const char* const DestinationEncoding = FPlatformString::GetEncodingName();

			UErrorCode ICUStatus = U_ZERO_ERROR;

			UConverter* ICUConverter = ucnv_open(DestinationEncoding, &ICUStatus);
			if( U_SUCCESS(ICUStatus) ) 
			{
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

				ucnv_close(ICUConverter);
			}

			check( U_SUCCESS(ICUStatus) );
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