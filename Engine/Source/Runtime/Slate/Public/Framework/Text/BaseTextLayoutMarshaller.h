// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_FANCY_TEXT

#include "ITextLayoutMarshaller.h"

/**
 * Base class implementing some common functionality for all text marshallers
 */
class SLATE_API FBaseTextLayoutMarshaller : public ITextLayoutMarshaller
{
public:

	// BEGIN ITextLayoutMarshaller interface
	virtual bool RequiresLiveUpdate() const override
	{
		return false;
	}

	virtual void MakeDirty() override
	{
		bIsDirty = true;
	}

	virtual void ClearDirty() override
	{
		bIsDirty = false;
	}

	virtual bool IsDirty() const override
	{
		return bIsDirty;
	}
	// END ITextLayoutMarshaller interface

protected:

	FBaseTextLayoutMarshaller()
		: bIsDirty(false)
	{
	}

	virtual ~FBaseTextLayoutMarshaller()
	{
	}

private:

	bool bIsDirty;

};

#endif //WITH_FANCY_TEXT
