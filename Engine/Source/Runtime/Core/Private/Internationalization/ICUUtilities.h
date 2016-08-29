// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_ICU
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(push)
	#pragma warning(disable:28251)
	#pragma warning(disable:28252)
	#pragma warning(disable:28253)
#endif
	#include "unicode/unistr.h"
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(pop)
#endif

namespace ICUUtilities
{
	/** 
	 * An object that can convert between FString and icu::UnicodeString
	 * Note: This object is not thread-safe.
	 */
	class FStringConverter
	{
	public:
		FStringConverter();
		~FStringConverter();

		/** Convert FString -> icu::UnicodeString */
		void ConvertString(const FString& Source, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
		void ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
		icu::UnicodeString ConvertString(const FString& Source, const bool ShouldNullTerminate = true);
		icu::UnicodeString ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, const bool ShouldNullTerminate = true);

		/** Convert icu::UnicodeString -> FString */
		void ConvertString(const icu::UnicodeString& Source, FString& Destination);
		FString ConvertString(const icu::UnicodeString& Source);

	private:
		/** Non-copyable */
		FStringConverter(const FStringConverter&);
		FStringConverter& operator=(const FStringConverter&);

		UConverter* ICUConverter;
	};

	/** Convert FString -> icu::UnicodeString */
	void ConvertString(const FString& Source, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
	void ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
	icu::UnicodeString ConvertString(const FString& Source, const bool ShouldNullTerminate = true);
	icu::UnicodeString ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, const bool ShouldNullTerminate = true);

	/** Convert icu::UnicodeString -> FString */
	void ConvertString(const icu::UnicodeString& Source, FString& Destination);
	FString ConvertString(const icu::UnicodeString& Source);
}
#endif
