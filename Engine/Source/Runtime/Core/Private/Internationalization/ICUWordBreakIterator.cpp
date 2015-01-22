// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "BreakIterator.h"

#if UE_ENABLE_ICU
#include "ICUBreakIterator.h"

class FICUWordBreakIterator : public FICUBreakIterator
{
public:
	FICUWordBreakIterator();
};

FICUWordBreakIterator::FICUWordBreakIterator()
	: FICUBreakIterator(FICUBreakIteratorManager::Get().CreateWordBreakIterator())
{
}

TSharedRef<IBreakIterator> FBreakIterator::CreateWordBreakIterator()
{
	return MakeShareable(new FICUWordBreakIterator());
}

#endif
