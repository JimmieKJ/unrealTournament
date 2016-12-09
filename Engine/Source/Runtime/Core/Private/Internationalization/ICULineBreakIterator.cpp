// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/IBreakIterator.h"
#include "Internationalization/BreakIterator.h"

#if UE_ENABLE_ICU
#include "Internationalization/ICUBreakIterator.h"
#include "Internationalization/ICUTextCharacterIterator.h"

class FICULineBreakIterator : public IBreakIterator
{
public:
	FICULineBreakIterator();
	virtual ~FICULineBreakIterator();

	virtual void SetString(const FText& InText) override;
	virtual void SetString(const FString& InString) override;
	virtual void SetString(const TCHAR* const InString, const int32 InStringLength) override;
	virtual void ClearString() override;

	virtual int32 GetCurrentPosition() const override;

	virtual int32 ResetToBeginning() override;
	virtual int32 ResetToEnd() override;

	virtual int32 MoveToPrevious() override;
	virtual int32 MoveToNext() override;
	virtual int32 MoveToCandidateBefore(const int32 InIndex) override;
	virtual int32 MoveToCandidateAfter(const int32 InIndex) override;

protected:
	int32 MoveToPreviousImpl();
	int32 MoveToNextImpl();

	TSharedRef<icu::BreakIterator> GetInternalLineBreakIterator() const;
	TSharedRef<icu::BreakIterator> GetInternalWordBreakIterator() const;

private:
	/**
	 * We use both a line-break and word-break iterator here, as CJK languages use dictionary-based word-breaking which isn't supported correctly by the line-break iterator.
	 * To compensate for that, we use both iterators in tandem and treat the word-break result as the minimum breaking point (to avoid breaking words in CJK).
	 * For example, in Korean the line-break iterator tries to break per-glyph, whereas the word-break iterator knows where the word boundaries are; however in English, the line-break
	 * iterator works correctly, but the word-break iterator may fall short as it doesn't handle symbols as would be expected.
	 */
	TWeakPtr<icu::BreakIterator> ICULineBreakIteratorHandle;
	TWeakPtr<icu::BreakIterator> ICUWordBreakIteratorHandle;
	int32 CurrentPosition;
};

FICULineBreakIterator::FICULineBreakIterator()
	: ICULineBreakIteratorHandle(FICUBreakIteratorManager::Get().CreateLineBreakIterator())
	, ICUWordBreakIteratorHandle(FICUBreakIteratorManager::Get().CreateWordBreakIterator())
	, CurrentPosition(0)
{
}

FICULineBreakIterator::~FICULineBreakIterator()
{
	// This assumes that FICULineBreakIterator owns the iterators, and that nothing ever copies an FICULineBreakIterator instance
	FICUBreakIteratorManager::Get().DestroyIterator(ICULineBreakIteratorHandle);
	FICUBreakIteratorManager::Get().DestroyIterator(ICUWordBreakIteratorHandle);
}

void FICULineBreakIterator::SetString(const FText& InText)
{
	GetInternalLineBreakIterator()->adoptText(new FICUTextCharacterIterator(InText)); // ICUBreakIterator takes ownership of this instance
	GetInternalWordBreakIterator()->adoptText(new FICUTextCharacterIterator(InText)); // ICUBreakIterator takes ownership of this instance
	ResetToBeginning();
}

void FICULineBreakIterator::SetString(const FString& InString)
{
	GetInternalLineBreakIterator()->adoptText(new FICUTextCharacterIterator(InString)); // ICUBreakIterator takes ownership of this instance
	GetInternalWordBreakIterator()->adoptText(new FICUTextCharacterIterator(InString)); // ICUBreakIterator takes ownership of this instance
	ResetToBeginning();
}

void FICULineBreakIterator::SetString(const TCHAR* const InString, const int32 InStringLength)
{
	GetInternalLineBreakIterator()->adoptText(new FICUTextCharacterIterator(InString, InStringLength)); // ICUBreakIterator takes ownership of this instance
	GetInternalWordBreakIterator()->adoptText(new FICUTextCharacterIterator(InString, InStringLength)); // ICUBreakIterator takes ownership of this instance
	ResetToBeginning();
}

void FICULineBreakIterator::ClearString()
{
	GetInternalLineBreakIterator()->adoptText(new FICUTextCharacterIterator(FString())); // ICUBreakIterator takes ownership of this instance
	GetInternalWordBreakIterator()->adoptText(new FICUTextCharacterIterator(FString())); // ICUBreakIterator takes ownership of this instance
	ResetToBeginning();
}

int32 FICULineBreakIterator::GetCurrentPosition() const
{
	return CurrentPosition;
}

int32 FICULineBreakIterator::ResetToBeginning()
{
	TSharedRef<icu::BreakIterator> LineBrkIt = GetInternalLineBreakIterator();
	CurrentPosition = LineBrkIt->first();
	GetInternalWordBreakIterator()->first();
	CurrentPosition = static_cast<FICUTextCharacterIterator&>(LineBrkIt->getText()).InternalIndexToSourceIndex(CurrentPosition);
	return CurrentPosition;
}

int32 FICULineBreakIterator::ResetToEnd()
{
	TSharedRef<icu::BreakIterator> LineBrkIt = GetInternalLineBreakIterator();
	CurrentPosition = LineBrkIt->last();
	GetInternalWordBreakIterator()->last();
	CurrentPosition = static_cast<FICUTextCharacterIterator&>(LineBrkIt->getText()).InternalIndexToSourceIndex(CurrentPosition);
	return CurrentPosition;
}

int32 FICULineBreakIterator::MoveToPrevious()
{
	return MoveToPreviousImpl();
}

int32 FICULineBreakIterator::MoveToNext()
{
	return MoveToNextImpl();
}

int32 FICULineBreakIterator::MoveToCandidateBefore(const int32 InIndex)
{
	CurrentPosition = InIndex;
	return MoveToPreviousImpl();
}

int32 FICULineBreakIterator::MoveToCandidateAfter(const int32 InIndex)
{
	CurrentPosition = InIndex;
	return MoveToNextImpl();
}

int32 FICULineBreakIterator::MoveToPreviousImpl()
{
	TSharedRef<icu::BreakIterator> LineBrkIt = GetInternalLineBreakIterator();
	CurrentPosition = static_cast<FICUTextCharacterIterator&>(LineBrkIt->getText()).SourceIndexToInternalIndex(CurrentPosition);

	const int32 NewLinePosition = LineBrkIt->preceding(CurrentPosition);
	const int32 NewWordPosition = GetInternalWordBreakIterator()->preceding(CurrentPosition);

	CurrentPosition = FMath::Min(NewLinePosition, NewWordPosition);
	CurrentPosition = static_cast<FICUTextCharacterIterator&>(LineBrkIt->getText()).InternalIndexToSourceIndex(CurrentPosition);

	return CurrentPosition;
}

int32 FICULineBreakIterator::MoveToNextImpl()
{
	TSharedRef<icu::BreakIterator> LineBrkIt = GetInternalLineBreakIterator();
	CurrentPosition = static_cast<FICUTextCharacterIterator&>(LineBrkIt->getText()).SourceIndexToInternalIndex(CurrentPosition);

	const int32 NewLinePosition = LineBrkIt->following(CurrentPosition);
	const int32 NewWordPosition = GetInternalWordBreakIterator()->following(CurrentPosition);

	CurrentPosition = FMath::Max(NewLinePosition, NewWordPosition);
	CurrentPosition = static_cast<FICUTextCharacterIterator&>(LineBrkIt->getText()).InternalIndexToSourceIndex(CurrentPosition);

	return CurrentPosition;
}

TSharedRef<icu::BreakIterator> FICULineBreakIterator::GetInternalLineBreakIterator() const
{
	return ICULineBreakIteratorHandle.Pin().ToSharedRef();
}

TSharedRef<icu::BreakIterator> FICULineBreakIterator::GetInternalWordBreakIterator() const
{
	return ICUWordBreakIteratorHandle.Pin().ToSharedRef();
}

TSharedRef<IBreakIterator> FBreakIterator::CreateLineBreakIterator()
{
	return MakeShareable(new FICULineBreakIterator());
}

#endif
