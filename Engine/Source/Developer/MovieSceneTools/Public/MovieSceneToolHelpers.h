// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"
#include "IKeyArea.h"

/**
 * A key area for float keys
 */
class FFloatCurveKeyArea : public IKeyArea
{
public:
	FFloatCurveKeyArea( FRichCurve& InCurve )
		: Curve( InCurve )
	{}

	/** IKeyArea interface */
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override
	{
		TArray<FKeyHandle> OutKeyHandles;
		for (auto It(Curve.GetKeyHandleIterator()); It; ++It)
		{
			OutKeyHandles.Add(It.Key());
		}
		return OutKeyHandles;
	}

	virtual float GetKeyTime( FKeyHandle KeyHandle ) const override
	{
		return Curve.GetKeyTime( KeyHandle );
	}

	virtual FKeyHandle MoveKey( FKeyHandle KeyHandle, float DeltaPosition ) override
	{
		return Curve.SetKeyTime( KeyHandle, Curve.GetKeyTime( KeyHandle ) + DeltaPosition );
	}

	virtual void DeleteKey(FKeyHandle KeyHandle) override
	{
		if( Curve.IsKeyHandleValid(KeyHandle) )
		{
			Curve.DeleteKey(KeyHandle);
		}
	}
private:
	/** Curve with keys in this area */
	FRichCurve& Curve;
};



/** A key area for integral keys */
class FIntegralKeyArea : public IKeyArea
{
public:
	FIntegralKeyArea( FIntegralCurve& InCurve )
		: Curve(InCurve)
	{}

	/** IKeyArea interface */
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override
	{
		TArray<FKeyHandle> OutKeyHandles;
		for (auto It(Curve.GetKeyHandleIterator()); It; ++It)
		{
			OutKeyHandles.Add(It.Key());
		}
		return OutKeyHandles;
	}

	virtual float GetKeyTime( FKeyHandle KeyHandle ) const override
	{
		return Curve.GetKeyTime( KeyHandle );
	}

	virtual FKeyHandle MoveKey( FKeyHandle KeyHandle, float DeltaPosition ) override
	{
		return Curve.SetKeyTime( KeyHandle, Curve.GetKeyTime( KeyHandle ) + DeltaPosition );
	}

	virtual void DeleteKey(FKeyHandle KeyHandle) override
	{
		Curve.DeleteKey(KeyHandle);
	}
private:
	/** Curve with keys in this area */
	FIntegralCurve& Curve;
};


