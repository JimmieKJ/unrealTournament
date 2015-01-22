// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


/* FCurveSequence structors
 *****************************************************************************/

FCurveSequence::FCurveSequence( )
	: StartTime(0)
	, TotalDuration(0)
	, bInReverse(true)
{ }


/* FCurveSequence interface
 *****************************************************************************/

FCurveSequence::FCurveSequence( const float InStartTimeSeconds, const float InDurationSeconds, const ECurveEaseFunction::Type InEaseFunction )
	: StartTime(0)
	, TotalDuration(0)
	, bInReverse(true)
{
	const FCurveHandle IgnoredCurveHandle = AddCurve( InStartTimeSeconds, InDurationSeconds, InEaseFunction );
}


FCurveHandle FCurveSequence::AddCurve( const float InStartTimeSeconds, const float InDurationSeconds, const ECurveEaseFunction::Type InEaseFunction )
{
	// Keep track of how long this sequence is
	TotalDuration = FMath::Max(TotalDuration, InStartTimeSeconds + InDurationSeconds);
	// The initial state is to be at the end of the animation.
	StartTime = TotalDuration;

	// Actually make this curve and return a handle to it.
	Curves.Add( FSlateCurve(InStartTimeSeconds, InDurationSeconds, InEaseFunction) );
	return FCurveHandle(this, Curves.Num() - 1);
}


FCurveHandle FCurveSequence::AddCurveRelative( const float InOffset, const float InDurationSecond, const ECurveEaseFunction::Type InEaseFunction )
{
	const float CurveStartTime = TotalDuration + InOffset;
	return AddCurve(CurveStartTime, InDurationSecond, InEaseFunction);
}


void FCurveSequence::Play( const float StartAtTime )
{
	// Playing forward
	bInReverse = false;

	// We start playing NOW.
	SetStartTime(FSlateApplicationBase::Get().GetCurrentTime() - StartAtTime);
}


void FCurveSequence::Reverse( )
{
	// We're going the other way now.
	bInReverse = !bInReverse;	

	// CurTime is now; we cannot change that, so everything happens relative to CurTime.
	const double CurTime = FSlateApplicationBase::Get().GetCurrentTime();
	
	// We've played this far into the animation.
	const float FractionCompleted = FMath::Clamp((CurTime - StartTime) / TotalDuration, 0.0, 1.0);
	
	// Assume CurTime is constant (now).
	// Figure out when the animation would need to have started in order to keep
	// its place if playing in reverse.
	const double NewStartTime = CurTime - TotalDuration * (1 - FractionCompleted);
	SetStartTime(NewStartTime);
}


void FCurveSequence::PlayReverse( const float StartAtTime )
{
	bInReverse = true;

	// We start reversing NOW.
	SetStartTime(FSlateApplicationBase::Get().GetCurrentTime() - StartAtTime);
}


bool FCurveSequence::IsPlaying( ) const
{
	return (FSlateApplicationBase::Get().GetCurrentTime() - StartTime) <= TotalDuration;
}


void FCurveSequence::SetStartTime( double InStartTime )
{
	StartTime = InStartTime;
}


float FCurveSequence::GetSequenceTime( ) const
{
	const double CurrentTime = FSlateApplicationBase::Get().GetCurrentTime();

	return IsInReverse()
		? TotalDuration - (CurrentTime - StartTime)
		: CurrentTime - StartTime;
}


float FCurveSequence::GetSequenceTimeLooping( ) const
{
	return FMath::Fmod(GetSequenceTime(), TotalDuration);
}


bool FCurveSequence::IsInReverse( ) const
{
	return bInReverse;
}


bool FCurveSequence::IsForward( ) const
{
	return !bInReverse;
}


void FCurveSequence::JumpToStart( )
{
	bInReverse = true;
	SetStartTime(FSlateApplicationBase::Get().GetCurrentTime() - TotalDuration);
}


void FCurveSequence::JumpToEnd( )
{
	bInReverse = false;
	SetStartTime(FSlateApplicationBase::Get().GetCurrentTime() - TotalDuration);
}


bool FCurveSequence::IsAtStart( ) const
{
	return (IsInReverse() == true && IsPlaying() == false);
}


bool FCurveSequence::IsAtEnd( ) const
{
	return (IsForward() == true && IsPlaying() == false);
}


float FCurveSequence::GetLerp( ) const
{
	// Only supported for sequences with a single curve.  If you have multiple curves, use your FCurveHandle to compute
	// interpolation alpha values.
	checkSlow(Curves.Num() == 1);

	return FCurveHandle(this, 0).GetLerp();
}


float FCurveSequence::GetLerpLooping( ) const
{
	// Only supported for sequences with a single curve.  If you have multiple curves, use your FCurveHandle to compute
	// interpolation alpha values.
	checkSlow(Curves.Num() == 1);

	return FCurveHandle(this, 0).GetLerpLooping();
}


const FCurveSequence::FSlateCurve& FCurveSequence::GetCurve( int32 CurveIndex ) const
{
	return Curves[CurveIndex];
}
