// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "BreakIterator.h"

#if UE_ENABLE_ICU
#include "ICUBreakIterator.h"

class FICULineBreakIterator : public FICUBreakIterator
{
public:
	FICULineBreakIterator();
};

FICULineBreakIterator::FICULineBreakIterator()
	: FICUBreakIterator(FICUBreakIteratorManager::Get().CreateLineBreakIterator())
{
}

TSharedRef<IBreakIterator> FBreakIterator::CreateLineBreakIterator()
{
	return MakeShareable(new FICULineBreakIterator());
}

#endif
