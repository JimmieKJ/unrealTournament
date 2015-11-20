// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencer.h"
#include "ISequencerSection.h"
#include "IKeyArea.h"
#include "MovieSceneSection.h"

/**
 * A key area for float keys
 */
class MOVIESCENETOOLS_API FFloatCurveKeyArea : public IKeyArea
{
public:
	FFloatCurveKeyArea( FRichCurve* InCurve, UMovieSceneSection* InOwningSection )
		: Curve( InCurve )
		, OwningSection( InOwningSection )
	{}

	/** IKeyArea interface */
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override
	{
		TArray<FKeyHandle> OutKeyHandles;
		for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
		{
			OutKeyHandles.Add(It.Key());
		}
		return OutKeyHandles;
	}

	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) const override
	{
		Curve->SetKeyTime(KeyHandle, NewKeyTime);
	}

	virtual float GetKeyTime(FKeyHandle KeyHandle) const override
	{
		return Curve->GetKeyTime(KeyHandle);
	}

	virtual FKeyHandle MoveKey(FKeyHandle KeyHandle, float DeltaPosition) override
	{
		return Curve->SetKeyTime( KeyHandle, Curve->GetKeyTime( KeyHandle ) + DeltaPosition );
	}

	virtual void DeleteKey(FKeyHandle KeyHandle) override
	{
		if ( Curve->IsKeyHandleValid( KeyHandle ) )
		{
			Curve->DeleteKey( KeyHandle );
		}
	}

	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) override
	{
		if (Curve->IsKeyHandleValid(KeyHandle))
		{
			Curve->SetKeyInterpMode(KeyHandle, InterpMode);
		}
	}

	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle KeyHandle) const override
	{
		if (Curve->IsKeyHandleValid(KeyHandle))
		{
			return Curve->GetKeyInterpMode(KeyHandle);
		}
		return RCIM_None;
	}

	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) override
	{
		if (Curve->IsKeyHandleValid(KeyHandle))
		{
			Curve->SetKeyTangentMode(KeyHandle, TangentMode);
		}
	}

	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const override
	{
		if (Curve->IsKeyHandleValid(KeyHandle))
		{
			return Curve->GetKeyTangentMode(KeyHandle);
		}
		return RCTM_None;
	}

	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) override
	{
		if (bPreInfinity)
		{
			Curve->PreInfinityExtrap = ExtrapMode;
		}
		else
		{
			Curve->PostInfinityExtrap = ExtrapMode;
		}
	}

	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const override
	{
		if (bPreInfinity)
		{
			return Curve->PreInfinityExtrap;
		}
		else
		{
			return Curve->PostInfinityExtrap;
		}
		return RCCE_None;
	}

	virtual TArray<FKeyHandle> AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom = FLT_MAX) override;

	virtual FRichCurve* GetRichCurve() override { return Curve; }

	virtual UMovieSceneSection* GetOwningSection() override { return OwningSection; }

	virtual bool CanCreateKeyEditor() override
	{
		return true;
	}

	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;

	void SetIntermediateValue( float InIntermediateValue )
	{
		IntermediateValue = InIntermediateValue;
	}

	void ClearIntermediateValue()
	{
		IntermediateValue.Reset();
	}

private:
	/** The curve which provides the keys for this key area. */
	FRichCurve* Curve;
	/** The section that owns this key area. */
	UMovieSceneSection* OwningSection;

	TOptional<float> IntermediateValue;
};


/** A key area for integral keys */
template<typename IntegralType>
class MOVIESCENETOOLS_API FIntegralKeyArea : public IKeyArea
{
public:
	FIntegralKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: Curve(InCurve)
		, OwningSection(InOwningSection)
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

	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) const override
	{
		Curve.SetKeyTime(KeyHandle, NewKeyTime);
	}

	virtual float GetKeyTime(FKeyHandle KeyHandle) const override
	{
		return Curve.GetKeyTime(KeyHandle);
	}

	virtual FKeyHandle MoveKey(FKeyHandle KeyHandle, float DeltaPosition) override
	{
		return Curve.SetKeyTime( KeyHandle, Curve.GetKeyTime( KeyHandle ) + DeltaPosition );
	}

	virtual void DeleteKey(FKeyHandle KeyHandle) override
	{
		Curve.DeleteKey(KeyHandle);
	}

	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) override
	{
	}

	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle KeyHandle) const override
	{
		return RCIM_None;
	}

	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) override
	{
	}

	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const override
	{
		return RCTM_None;
	}

	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) override
	{
	}

	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const override
	{
		return RCCE_None;
	}

	virtual TArray<FKeyHandle> AddKeyUnique( float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom = FLT_MAX ) override
	{
		TArray<FKeyHandle> AddedKeyHandles;

		FKeyHandle CurrentKey = Curve.FindKey( Time );
		if ( Curve.IsKeyHandleValid( CurrentKey ) == false )
		{
			if ( OwningSection->GetStartTime() > Time )
			{
				OwningSection->SetStartTime( Time );
			}
			if ( OwningSection->GetEndTime() < Time )
			{
				OwningSection->SetEndTime( Time );
			}

			int32 Value = Curve.Evaluate( Time );
			if ( TimeToCopyFrom != FLT_MAX )
			{
				Value = Curve.Evaluate( TimeToCopyFrom );
			}
			else if ( IntermediateValue.IsSet() )
			{
				Value = IntermediateValue.GetValue();
			}

			Curve.AddKey( Time, Value, CurrentKey );
			AddedKeyHandles.Add( CurrentKey );
		}

		return AddedKeyHandles;
	}

	virtual FRichCurve* GetRichCurve() override { return nullptr; };

	virtual UMovieSceneSection* GetOwningSection() override { return OwningSection; }

	void SetIntermediateValue( IntegralType InIntermediateValue )
	{
		IntermediateValue = InIntermediateValue;
	}

	void ClearIntermediateValue()
	{
		IntermediateValue.Reset();
	}

protected:
	/** Curve with keys in this area */
	FIntegralCurve& Curve;
	/** The section that owns this key area. */
	UMovieSceneSection* OwningSection;

	TOptional<IntegralType> IntermediateValue;
};

/** A key area for displaying and editing byte curves. */
class FByteKeyArea : public FIntegralKeyArea < uint8 >
{
public:
	FByteKeyArea( FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection )
		: FIntegralKeyArea( InCurve, InOwningSection )
	{}

	virtual bool CanCreateKeyEditor() override
	{
		return true;
	}

	virtual TSharedRef<SWidget> CreateKeyEditor( ISequencer* Sequencer ) override;
};

/** A key area for displaying and editing integral curves representing enums. */
class FEnumKeyArea : public FByteKeyArea
{
public:
	FEnumKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection, const UEnum* InEnum)
		: FByteKeyArea(InCurve, InOwningSection)
		, Enum(InEnum)
	{}

	virtual bool CanCreateKeyEditor() override 
	{
		return true;
	}

	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;

private:
	/** The enum which provides available integral values for this key area. */
	const UEnum* Enum;

};


/** A key area for displaying and editing integral curves representing bools. */
class FBoolKeyArea : public FIntegralKeyArea<bool>
{
public:
	FBoolKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: FIntegralKeyArea(InCurve, InOwningSection)
	{}

	virtual bool CanCreateKeyEditor() override 
	{
		return true;
	}

	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;
};


/**
 * A key area for FName curves.
 */
class MOVIESCENETOOLS_API FNameCurveKeyArea
	: public IKeyArea
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InCurve The curve to assign to this key area.
	 * @param InOwningSection The section that owns this key area.
	 */
	FNameCurveKeyArea(FNameCurve& InCurve, UMovieSceneSection* InOwningSection)
		: Curve(InCurve)
		, OwningSection(InOwningSection)
	{ }

public:

	// IKeyArea interface

	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override
	{
		TArray<FKeyHandle> OutKeyHandles;

		for (auto It(Curve.GetKeyHandleIterator()); It; ++It)
		{
			OutKeyHandles.Add(It.Key());
		}

		return OutKeyHandles;
	}

	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) const override
	{
		Curve.SetKeyTime(KeyHandle, NewKeyTime);
	}

	virtual float GetKeyTime(FKeyHandle KeyHandle) const override
	{
		return Curve.GetKeyTime(KeyHandle);
	}

	virtual FKeyHandle MoveKey( FKeyHandle KeyHandle, float DeltaPosition ) override
	{
		return Curve.SetKeyTime(KeyHandle, Curve.GetKeyTime(KeyHandle) + DeltaPosition);
	}
	
	virtual void DeleteKey(FKeyHandle KeyHandle) override
	{
		Curve.DeleteKey(KeyHandle);
	}

	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) override
	{
		// do nothing
	}

	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle Keyhandle) const override
	{
		return RCIM_None;
	}

	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) override
	{
		// do nothing
	}

	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const override
	{
		return RCTM_None;
	}

	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) override
	{
		// do nothing
	}

	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const override
	{
		return RCCE_None;
	}

	virtual TArray<FKeyHandle> AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom = FLT_MAX) override;

	virtual FRichCurve* GetRichCurve() override
	{
		return nullptr;
	}

	virtual UMovieSceneSection* GetOwningSection() override
	{
		return OwningSection.Get();
	}

	virtual bool CanCreateKeyEditor() override
	{
		return false;
	}

	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override
	{
		return SNullWidget::NullWidget;
	}

protected:

	/** The curve managed by this area. */
	FNameCurve& Curve;

	/** The section that owns this area. */
	TWeakObjectPtr<UMovieSceneSection> OwningSection;
};


class MOVIESCENETOOLS_API MovieSceneToolHelpers
{
public:
	/**
	* Trim section at the given time
	*
	* @param Sections The sections to trim
	* @param Time	The time at which to trim
	* @param bTrimLeft Trim left or trim right
	*/
	static void TrimSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time, bool bTrimLeft);

	/**
	* Splits sections at the given time
	*
	* @param Sections The sections to split
	* @param Time	The time at which to split
	*/
	static void SplitSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time);
};
