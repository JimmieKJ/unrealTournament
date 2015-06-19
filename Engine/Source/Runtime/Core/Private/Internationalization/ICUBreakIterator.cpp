// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "ICUBreakIterator.h"

#if UE_ENABLE_ICU
#include "ICUTextCharacterIterator.h"
#include "ICUCulture.h"

FICUBreakIteratorManager* FICUBreakIteratorManager::Singleton = nullptr;

void FICUBreakIteratorManager::Create()
{
	check(!Singleton);
	Singleton = new FICUBreakIteratorManager();
}

void FICUBreakIteratorManager::Destroy()
{
	check(Singleton);
	delete Singleton;
	Singleton = nullptr;
}

FICUBreakIteratorManager& FICUBreakIteratorManager::Get()
{
	check(Singleton);
	return *Singleton;
}

TWeakPtr<icu::BreakIterator> FICUBreakIteratorManager::CreateCharacterBoundaryIterator()
{
	TSharedRef<icu::BreakIterator> Iterator = MakeShareable( FInternationalization::Get().GetDefaultCulture()->Implementation->GetBreakIterator(EBreakIteratorType::Grapheme)->clone() );
	{
		FScopeLock ScopeLock(&AllocatedIteratorsCS);
		AllocatedIterators.Add(Iterator);
	}
	return Iterator;
}

TWeakPtr<icu::BreakIterator> FICUBreakIteratorManager::CreateWordBreakIterator()
{
	TSharedRef<icu::BreakIterator> Iterator = MakeShareable( FInternationalization::Get().GetDefaultCulture()->Implementation->GetBreakIterator(EBreakIteratorType::Word)->clone() );
	{
		FScopeLock ScopeLock(&AllocatedIteratorsCS);
		AllocatedIterators.Add(Iterator);
	}
	return Iterator;
}

TWeakPtr<icu::BreakIterator> FICUBreakIteratorManager::CreateLineBreakIterator()
{
	TSharedRef<icu::BreakIterator> Iterator = MakeShareable( FInternationalization::Get().GetDefaultCulture()->Implementation->GetBreakIterator(EBreakIteratorType::Line)->clone() );
	{
		FScopeLock ScopeLock(&AllocatedIteratorsCS);
		AllocatedIterators.Add(Iterator);
	}
	return Iterator;
}

void FICUBreakIteratorManager::DestroyIterator(TWeakPtr<icu::BreakIterator>& InIterator)
{
	TSharedPtr<icu::BreakIterator> Iterator = InIterator.Pin();
	if(Iterator.IsValid())
	{
		FScopeLock ScopeLock(&AllocatedIteratorsCS);
		AllocatedIterators.Remove(Iterator);
	}
	InIterator.Reset();
}

FICUBreakIterator::FICUBreakIterator(TWeakPtr<icu::BreakIterator>&& InICUBreakIteratorHandle)
	: ICUBreakIteratorHandle(InICUBreakIteratorHandle)
{
}

FICUBreakIterator::~FICUBreakIterator()
{
	// This assumes that FICUBreakIterator owns the iterator, and that nothing ever copies an FICUBreakIterator instance
	FICUBreakIteratorManager::Get().DestroyIterator(ICUBreakIteratorHandle);
}

void FICUBreakIterator::SetString(const FText& InText)
{
	GetInternalBreakIterator()->adoptText(new FICUTextCharacterIterator(InText)); // ICUBreakIterator takes ownership of this instance
	ResetToBeginning();
}

void FICUBreakIterator::SetString(const FString& InString)
{
	GetInternalBreakIterator()->adoptText(new FICUTextCharacterIterator(InString)); // ICUBreakIterator takes ownership of this instance
	ResetToBeginning();
}

void FICUBreakIterator::SetString(const TCHAR* const InString, const int32 InStringLength) 
{
	GetInternalBreakIterator()->adoptText(new FICUTextCharacterIterator(InString, InStringLength)); // ICUBreakIterator takes ownership of this instance
	ResetToBeginning();
}

void FICUBreakIterator::ClearString()
{
	GetInternalBreakIterator()->setText(icu::UnicodeString());
	ResetToBeginning();
}

int32 FICUBreakIterator::GetCurrentPosition() const
{
	return GetInternalBreakIterator()->current();
}

int32 FICUBreakIterator::ResetToBeginning()
{
	return GetInternalBreakIterator()->first();
}

int32 FICUBreakIterator::ResetToEnd()
{
	return GetInternalBreakIterator()->last();
}

int32 FICUBreakIterator::MoveToPrevious()
{
	return GetInternalBreakIterator()->previous();
}

int32 FICUBreakIterator::MoveToNext()
{
	return GetInternalBreakIterator()->next();
}

int32 FICUBreakIterator::MoveToCandidateBefore(const int32 InIndex)
{
	return GetInternalBreakIterator()->preceding(InIndex);
}

int32 FICUBreakIterator::MoveToCandidateAfter(const int32 InIndex)
{
	return GetInternalBreakIterator()->following(InIndex);
}

TSharedRef<icu::BreakIterator> FICUBreakIterator::GetInternalBreakIterator() const
{
	return ICUBreakIteratorHandle.Pin().ToSharedRef();
}

#endif
