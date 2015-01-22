// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A sequence of curves that can be used to drive animations.
 * Each curve within the sequence has a time offset and a duration.
 * This makes FCurveSequence convenient for crating staggered animations.
 * e.g.
 *   // We want to zoom in a widget, and then fade in its contents.
 *   FCurveHandle ZoomCurve = Sequence.AddCurve( 0, 0.15f );
 *   FCurveHandle FadeCurve = Sequence.AddCurve( 0.15f, 0.1f );
 *	 Sequence.Play();
 */
struct SLATECORE_API FCurveSequence
{
public:

	/** A curve has a time offset and duration.*/
	struct FSlateCurve
	{
		/** Constructor */
		FSlateCurve( float InStartTime, float InDurationSeconds, const ECurveEaseFunction::Type InEaseFunction )
			: DurationSeconds(InDurationSeconds)
			, StartTime(InStartTime)
			, EaseFunction(InEaseFunction)
		{
		}

		/** Length of this animation in seconds */
		float DurationSeconds;

		/** Start time for this animation */
		float StartTime;

		/**
		 * Type of easing function to use for this curve.
		 * Could be passed it at call site.
		 */
		ECurveEaseFunction::Type EaseFunction;
	};

	/** Default constructor */
	FCurveSequence( );

	/**
	 * Construct by adding a single animation curve to this sequence.  Does not provide access to the curve though.
	 *
	 * @param InStartTimeSeconds   When to start this curve.
	 * @param InDurationSeconds    How long this curve lasts.
	 * @param InEaseFunction       Easing function to use for this curve.  Defaults to Linear.  Use this to smooth out your animation transitions.
	 * @return A FCurveHandle that can be used to get the value of this curve after the animation starts playing.
	 */
	FCurveSequence( const float InStartTimeSeconds, const float InDurationSeconds, const ECurveEaseFunction::Type InEaseFunction = ECurveEaseFunction::Linear  );

	/**
	 * Add a new curve at a given time and offset.
	 *
	 * @param InStartTimeSeconds   When to start this curve.
	 * @param InDurationSeconds    How long this curve lasts.
	 * @param InEaseFunction       Easing function to use for this curve.  Defaults to Linear.  Use this to smooth out your animation transitions.
	 * @return A FCurveHandle that can be used to get the value of this curve after the animation starts playing.
	 */
	FCurveHandle AddCurve( const float InStartTimeSeconds, const float InDurationSeconds, const ECurveEaseFunction::Type InEaseFunction = ECurveEaseFunction::Linear );

	/**
	 * Add a new curve relative to the current end of the sequence. Makes stacking easier.
	 * e.g. doing 
	 *     AddCurveRelative(0,5);
	 *     AddCurveRelative(0,3);
	 * Is equivalent to
	 *     AddCurve(0,5);
	 *     AddCurve(5,3)
	 *
	 * @param InOffset             Offset from the last curve in the sequence.
	 * @param InDurationSecond     How long this curve lasts.
	 * @param InEaseFunction       Easing function to use for this curve.  Defaults to Linear.  Use this to smooth out your animation transitions.
	 */
	FCurveHandle AddCurveRelative( const float InOffset, const float InDurationSecond, const ECurveEaseFunction::Type InEaseFunction = ECurveEaseFunction::Linear );

	/**
	 * Start playing this curve sequence
	 *
	 * @param	StartAtTime		Specifies a time offset relative to the animation to start at.  Defaults to zero (the actual start of the sequence.)
	 */
	void Play( const float StartAtTime = 0.0f );

	/** Reverse the direction of an in-progress animation */
	void Reverse( );

	/**
	 * Start playing this curve sequence in reverse
	 *
	 * @param	StartAtTime		Specifies a time offset relative to the animation to start at.  Defaults to zero (the actual start of the sequence.)
	 */
	void PlayReverse( const float StartAtTime = 0.0f );

	/**
	 * Checks whether the sequence is currently playing.
	 *
	 * @return true if playing, false otherwise.
	 */
	bool IsPlaying( ) const;

	/** @return the current time relative to the beginning of the sequence. */
	float GetSequenceTime( ) const;

	/** @return the current time relative to the beginning of the sequence as if the animation were a looping one. */
	float GetSequenceTimeLooping( ) const;

	/** @return true if the animation is in reverse */
	bool IsInReverse( ) const;

	/** @return true if the animation is in forward gear */
	bool IsForward( ) const;

	/** Jumps immediately to the beginning of the animation sequence */
	void JumpToStart( );

	/** Jumps immediately to the end of the animation sequence */
	void JumpToEnd( );

	/** Is the sequence at the start? */
	bool IsAtStart( ) const;

	/** Is the sequence at the end? */
	bool IsAtEnd( ) const;

	/**
	 * For single-curve animations, returns the interpolation alpha for the animation.  If you call this function
	 * on a sequence with multiple curves, an assertion will trigger.
	 *
	 * @return A linearly interpolated value between 0 and 1 for this curve.
	 */
	float GetLerp( ) const;

	/**
	 * For single-curve animations, returns the looping interpolation alpha for the animation.  If you call this
	 * function on a sequence with multiple curves, an assertion will trigger.
	 *
	 * @return A linearly interpolated value between 0 and 1 for this curve.
	 */
	float GetLerpLooping( ) const;

	/**
	 * @param CurveIndex  Index of a curve in the curves array.
	 *
	 * @return A curve given the index into the curves array
	 */
	const FCurveSequence::FSlateCurve& GetCurve( int32 CurveIndex ) const;

protected:

	/** @param InStartTime  when this curve sequence started playing */
	void SetStartTime( double InStartTime );

private:

	/** All the curves in this sequence. */
	TArray<FSlateCurve> Curves;

	/** When the curve started playing. */
	double StartTime;

	/** How long the entire sequence lasts. */
	float TotalDuration;

	/** Are we playing the animation in reverse */
	bool bInReverse;
};
