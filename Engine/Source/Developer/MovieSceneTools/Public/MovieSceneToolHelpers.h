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
	FFloatCurveKeyArea( FRichCurve* InCurve, UMovieSceneSection* InOwningSection )
		: CurveInfo( InCurve, InOwningSection )
	{}

	/** IKeyArea interface */
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override
	{
		TArray<FKeyHandle> OutKeyHandles;
		for (auto It(CurveInfo.Curve->GetKeyHandleIterator()); It; ++It)
		{
			OutKeyHandles.Add(It.Key());
		}
		return OutKeyHandles;
	}

	virtual float GetKeyTime( FKeyHandle KeyHandle ) const override
	{
		return CurveInfo.Curve->GetKeyTime( KeyHandle );
	}

	virtual FKeyHandle MoveKey( FKeyHandle KeyHandle, float DeltaPosition ) override
	{
		return CurveInfo.Curve->SetKeyTime( KeyHandle, CurveInfo.Curve->GetKeyTime( KeyHandle ) + DeltaPosition );
	}

	virtual void DeleteKey(FKeyHandle KeyHandle) override
	{
		if ( CurveInfo.Curve->IsKeyHandleValid( KeyHandle ) )
		{
			CurveInfo.Curve->DeleteKey( KeyHandle );
		}
	}

	virtual FCurveInfo* GetCurveInfo() override { return &CurveInfo; }
private:
	IKeyArea::FCurveInfo CurveInfo;
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

	virtual IKeyArea::FCurveInfo* GetCurveInfo() override { return nullptr; };
private:
	/** Curve with keys in this area */
	FIntegralCurve& Curve;
};


