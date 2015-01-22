// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU

#include "ICUTextCharacterIterator.h"
#include "ICUUtilities.h"

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FICUTextCharacterIterator_NativeUTF16)

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const FText& InText)
	: InternalString(InText.ToString())
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const FString& InString)
	: InternalString(InString)
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const TCHAR* const InString, const int32 InStringLength)
	: InternalString(InString, InStringLength)
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(FString&& InString)
	: InternalString(InString)
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const FString* const InString)
	: StringPtr(InString)
{
	check(StringPtr);
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const FICUTextCharacterIterator_NativeUTF16& Other)
	: icu::UCharCharacterIterator(Other)
	, InternalString(*Other.StringPtr)
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::~FICUTextCharacterIterator_NativeUTF16()
{
}

FICUTextCharacterIterator_NativeUTF16& FICUTextCharacterIterator_NativeUTF16::operator=(const FICUTextCharacterIterator_NativeUTF16& Other)
{
	if(this != &Other)
	{
		icu::UCharCharacterIterator::operator=(Other);
		InternalString = *Other.StringPtr;
		StringPtr = &InternalString;
		setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
	}
	return *this;
}

icu::CharacterIterator* FICUTextCharacterIterator_NativeUTF16::clone() const
{
	return new FICUTextCharacterIterator_NativeUTF16(*this);
}


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FICUTextCharacterIterator_ConvertToUnicodeString)

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const FText& InText)
	: icu::StringCharacterIterator(ICUUtilities::ConvertString(InText.ToString()))
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const FString& InString)
	: icu::StringCharacterIterator(ICUUtilities::ConvertString(InString))
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const TCHAR* const InString, const int32 InStringLength)
	: icu::StringCharacterIterator(ICUUtilities::ConvertString(FString(InString, InStringLength)))
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(FString&& InString)
	: icu::StringCharacterIterator(ICUUtilities::ConvertString(InString))
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const FString* const InString)
	: icu::StringCharacterIterator(ICUUtilities::ConvertString(*InString))
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const FICUTextCharacterIterator_ConvertToUnicodeString& Other)
	: icu::StringCharacterIterator(Other)
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::~FICUTextCharacterIterator_ConvertToUnicodeString()
{
}

FICUTextCharacterIterator_ConvertToUnicodeString& FICUTextCharacterIterator_ConvertToUnicodeString::operator=(const FICUTextCharacterIterator_ConvertToUnicodeString& Other)
{
	if(this != &Other)
	{
		icu::StringCharacterIterator::operator=(Other);
	}
	return *this;
}

icu::CharacterIterator* FICUTextCharacterIterator_ConvertToUnicodeString::clone() const
{
	return new FICUTextCharacterIterator_ConvertToUnicodeString(*this);
}

#endif
