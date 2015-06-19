/************************************************************************************

Filename    :   Fader.cpp
Content     :   Utility classes for animation based on alpha values
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "Fader.h"

#include "Kernel/OVR_Math.h"
#include "Android/LogUtils.h"

namespace OVR {

//======================================================================================
// Fader
//======================================================================================

//==============================
// Fader::Fader
Fader::Fader( float const startAlpha ) : 
    FadeState( FADE_NONE ),
    PrePauseState( FADE_NONE ),
	StartAlpha( startAlpha ),
	FadeAlpha( startAlpha )
{
}

//==============================
// Fader::Update
void Fader::Update( float const fadeRate, double const deltaSeconds )
{
    if ( FadeState > FADE_PAUSED )
    {
        float const fadeDelta = ( fadeRate * deltaSeconds ) * ( FadeState == FADE_IN ? 1.0f : -1.0f );
        FadeAlpha += fadeDelta;
		OVR_ASSERT( fabs( fadeDelta ) > Mathf::SmallestNonDenormal );
		if ( fabs( fadeDelta ) < Mathf::SmallestNonDenormal )
		{
			LOG( "Fader::Update fabs( fadeDelta ) < Mathf::SmallestNonDenormal !!!!" );
		}
        if ( FadeAlpha < Mathf::SmallestNonDenormal )
        {
            FadeAlpha = 0.0f;
            FadeState = FADE_NONE;
            //LOG( "FadeState = FADE_NONE" );
        }
        else if ( FadeAlpha >= 1.0f - Mathf::SmallestNonDenormal )
        {
            FadeAlpha = 1.0f;
            FadeState = FADE_NONE;
            //LOG( "FadeState = FADE_NONE" );
        }
        //LOG( "fadeState = %s, fadeDelta = %.4f, fadeAlpha = %.4f", GetFadeStateName( FadeState ), fadeDelta, FadeAlpha );
    }        
}

//==============================
// Fader::StartFadeIn
void Fader::StartFadeIn()
{
    //LOG( "StartFadeIn" );
    FadeState = FADE_IN;
}

//==============================
// Fader::StartFadeOut
void Fader::StartFadeOut()
{
    //LOG( "StartFadeOut" );
    FadeState = FADE_OUT;
}

//==============================
// Fader::PauseFade
void Fader::PauseFade()
{
    //LOG( "PauseFade" );
    PrePauseState = FadeState;
    FadeState = FADE_PAUSED;
}

//==============================
// Fader::UnPause
void Fader::UnPause()
{
    FadeState = PrePauseState;
}

//==============================
// Fader::GetFadeStateName
char const * Fader::GetFadeStateName( eFadeState const state ) const 
{
    char const * fadeStateNames[FADE_MAX] = { "FADE_NONE", "FADE_PAUSED", "FADE_IN", "FADE_OUT" };
    return fadeStateNames[state];
}

//==============================
// Fader::Reset
void Fader::Reset()
{
	FadeAlpha = StartAlpha;
}

//==============================
// Fader::Reset
void Fader::ForceFinish()
{
	FadeState = FADE_NONE;
	FadeAlpha = FadeState == FADE_IN ? 1.0f : 0.0f;
}

//======================================================================================
// SineFader
//======================================================================================

//==============================
// SineFader::SineFader
SineFader::SineFader( float const startAlpha ) :
    Fader( startAlpha )
{
}

//==============================
// SineFader::GetFinalAlpha
float SineFader::GetFinalAlpha() const
{
    // NOTE: pausing will still re-calculate the 
    if ( GetFadeState() == FADE_NONE )
    {
        return GetFadeAlpha();   // already clamped        
    }
    // map to sine wave
    float radians = ( 1.0f - GetFadeAlpha() ) * Mathf::Pi;  // range 0 to pi
    return ( cos( radians ) + 1.0f ) * 0.5f; // range 0 to 1
}

} // namespace OVR