/************************************************************************************

Filename    :   ScrollManager.cpp
Content     :
Created     :   December 12, 2014
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "ScrollManager.h"
#include "Kernel/OVR_Alg.h"
#include "VrApi/Vsync.h"
#include "Input.h"
#include "VrCommon.h"

namespace OVR {

static const float 	EPSILON_VEL = 0.00001f;

static const int HALF_WRAP_JUMP = 4;
static const int HALF_WRAP_JUMP_ALPHA = 2;
static const float MINIMUM_FADE_VALUE = 0.2f;

OvrScrollManager::OvrScrollManager( eScrollType type )
		: CurrentScrollType( type )
		, MaxPosition( 0.0f )
		, TouchIsDown( false )
		, Deltas()
		, CurrentScrollState( SMOOTH_SCROLL )
		, Position( 0.0f )
		, Velocity( 0.0f )
		, AccumulatedDeltaTimeInSeconds( 0.0f )
		, WrapAroundEnabled( true )
		, IsWrapAroundTimeInitiated( false )
		, RemainingTimeForWrapAround( 0.0f )
		, AutoScrollInFwdDir( false )
		, AutoScrollBkwdDir( false )
		, PrevAutoScrollVelocity( 0.0f )
		, RestrictedScrolling( false )
		, TouchDirectionLocked( NO_LOCK )
		, ControllerDirectionLocked( NO_LOCK )
{
}

void OvrScrollManager::Frame( float deltaSeconds, unsigned int controllerState )
{
	// --
	// Controller Input Handling
	bool forwardScrolling = false;
	bool backwardScrolling = false;

	if( CurrentScrollType == HORIZONTAL_SCROLL )
	{
		forwardScrolling = controllerState & ( BUTTON_LSTICK_RIGHT | BUTTON_DPAD_RIGHT );
		backwardScrolling = controllerState & ( BUTTON_LSTICK_LEFT | BUTTON_DPAD_LEFT );
	}
	else if( CurrentScrollType == VERTICAL_SCROLL )
	{
		forwardScrolling = controllerState & ( BUTTON_LSTICK_DOWN | BUTTON_DPAD_DOWN );
		backwardScrolling = controllerState & ( BUTTON_LSTICK_UP | BUTTON_DPAD_UP );
	}

	if ( forwardScrolling || backwardScrolling )
	{
		CurrentScrollState = SMOOTH_SCROLL;
	}
	SetEnableAutoForwardScrolling( forwardScrolling );
	SetEnableAutoBackwardScrolling( backwardScrolling );

	// --
	// Logic update for the current scroll state
	switch( CurrentScrollState )
	{
		case SMOOTH_SCROLL:
		{
			// Auto scroll during wrap around initiation.
			if( ( !RestrictedScrolling ) || // Either not restricted scrolling
				( RestrictedScrolling && ( TouchDirectionLocked != NO_LOCK ) ) ) // or restricted scrolling with touch direction locked
			{
				if ( TouchIsDown )
				{
					if ( ( Position < 0.0f ) || ( Position > MaxPosition ) )
					{
						// When scrolled out of bounds and touch is still down,
						// auto-scroll to initiate wrap around easily.
						float totalDistance = 0.0f;
						for ( int i = 0; i < Deltas.GetNum(); ++i )
						{
							totalDistance += Deltas[i].d;
						}

						const float EPSILON_VEL = 0.00001f;
						if ( totalDistance < -EPSILON_VEL )
						{
							Velocity = -1.0f;
						}
						else if ( totalDistance > EPSILON_VEL )
						{
							Velocity = 1.0f;
						}
						else
						{
							Velocity = PrevAutoScrollVelocity;
						}
					}
					else
					{
						Velocity = 0.0f;
					}

					PrevAutoScrollVelocity = Velocity;
				}
			}

			if ( IsOutOfWrapBounds() && IsScrolling() )
			{
				if( IsWrapAroundTimeInitiated )
				{
					RemainingTimeForWrapAround -= deltaSeconds;
				}
				else
				{
					IsWrapAroundTimeInitiated = true;
					RemainingTimeForWrapAround = ScrollBehavior.WrapAroundHoldTime;
				}
			}
			else
			{
				IsWrapAroundTimeInitiated 	= false;
				RemainingTimeForWrapAround 	= 0.0f;
				if( IsOutOfBounds() )
				{
					if( !IsScrolling() )
					{
						CurrentScrollState = BOUNCE_SCROLL;
						const float bounceBackVelocity = 3.0f;
						if( Position < 0.0f )
						{
							Velocity = bounceBackVelocity;
						}
						else if( Position > MaxPosition )
						{
							Velocity = -bounceBackVelocity;
						}
						Velocity = GetModifiedVelocity( Velocity );
					}
				}
			}
		}
		break;

		case WRAP_AROUND_SCROLL:
		{
			if( ( Velocity > 0.0f && Position > MaxPosition ) ||
				( Velocity < 0.0f && Position < 0.0f ) ) // Wrapping around is over
			{
				Velocity = 0.0f;
				CurrentScrollState = SMOOTH_SCROLL;
				Position = Alg::Clamp( Position, 0.0f, MaxPosition );
			}

			else
			{
				if ( NeedsJumpDuringWrapAround() )
				{
					const int startCutOff 		= HALF_WRAP_JUMP - 1;
					const int endCutOff 		= MaxPosition - HALF_WRAP_JUMP;
					int direction = Velocity / fabsf( Velocity );

					if ( ( Position >= startCutOff ) &&  ( Position <= endCutOff ) )
					{
						if ( false ) // snap jump
						{
							if ( Velocity > 0.0f )
							{
								Position = endCutOff;
							}
							else if ( Velocity < 0.0f )
							{
								Position = startCutOff;
							}
						}
						else
						{
							float requiredJump = endCutOff - startCutOff;
							int SpeedUpRange = requiredJump / 2;
							SpeedUpRange = ( SpeedUpRange > 5 ) ? 5 : SpeedUpRange;
							float speedScale = requiredJump * 0.5f;
							speedScale = (speedScale > 4.0f) ? speedScale : 4.0f;
							if ( Position < startCutOff + SpeedUpRange )
							{
								speedScale = LinearRangeMapFloat( Position, startCutOff, startCutOff + SpeedUpRange, 1.0f, speedScale );
							}
							else if ( Position > endCutOff - SpeedUpRange )
							{
								speedScale = LinearRangeMapFloat( Position, endCutOff - SpeedUpRange, endCutOff, speedScale, 1.0f );
							}

							Velocity = ScrollBehavior.WrapAroundSpeed * direction * speedScale;
						}
					}
					else
					{

						Velocity = ScrollBehavior.WrapAroundSpeed * direction;
					}
				}
			}
		}
		break;

		case BOUNCE_SCROLL:
		{
			if( !IsOutOfBounds() )
			{
				CurrentScrollState = SMOOTH_SCROLL;
				if( Velocity > 0.0f ) // Pulled at the beginning
				{
					Position = 0.0f;
				}
				else if( Velocity < 0.0f ) // Pulled at the end
				{
					Position = MaxPosition;
				}
				Velocity = 0.0f;
			}
		}
		break;
	}

	// --
	// Scrolling physics update
	AccumulatedDeltaTimeInSeconds += deltaSeconds;
	OVR_ASSERT( AccumulatedDeltaTimeInSeconds <= 10.0f );
	while( true )
	{
		if( AccumulatedDeltaTimeInSeconds < 0.016f )
		{
			break;
		}

		AccumulatedDeltaTimeInSeconds -= 0.016f;
		if( CurrentScrollState == SMOOTH_SCROLL || CurrentScrollState == BOUNCE_SCROLL ) // While wrap around don't slow down scrolling
		{
			Velocity -= Velocity * ScrollBehavior.FrictionPerSec * 0.016f;
			if( fabsf(Velocity) < EPSILON_VEL )
			{
				Velocity = 0.0f;
			}
		}

		Position += Velocity * 0.016f;
		if( CurrentScrollState == BOUNCE_SCROLL )
		{
			if( Velocity > 0.0f )
			{
				Position = Alg::Clamp( Position, -ScrollBehavior.Padding, MaxPosition );
			}
			else
			{
				Position = Alg::Clamp( Position, 0.0f, MaxPosition + ScrollBehavior.Padding );
			}
		}
		else
		{
			Position = Alg::Clamp( Position, -ScrollBehavior.Padding, MaxPosition + ScrollBehavior.Padding );
		}
	}
}

void OvrScrollManager::TouchDown()
{
	TouchIsDown = true;
	CurrentScrollState = SMOOTH_SCROLL;
	LastTouchPosistion.Set( 0.0f, 0.0f, 0.0f );
	Velocity = 0.0f;
	Deltas.Clear();

}

void OvrScrollManager::TouchUp()
{
	TouchIsDown = false;

	if ( Deltas.GetNum() == 0 )
	{
		Velocity = 0.0f;
	}
	else
	{
		if ( WrapAroundEnabled && IsWrapAroundTimeInitiated )
		{
			PerformWrapAroundOnScrollFinish();
		}
		else
		{
			// accumulate total distance
			float totalDistance = 0.0f;
			for ( int i = 0; i < Deltas.GetNum(); ++i )
			{
				totalDistance += Deltas[i].d;
			}

			// calculating total time spent
			delta_t const & head = Deltas.GetHead();
			delta_t const & tail = Deltas.GetTail();
			float totalTime = head.t - tail.t;

			// calculating velocity based on touches
			float touchVelocity = totalTime > Mathf::SmallestNonDenormal ? totalDistance / totalTime : 0.0f;
			Velocity = GetModifiedVelocity( touchVelocity );
		}
	}

	Deltas.Clear();

	return;
}

void OvrScrollManager::TouchRelative( Vector3f touchPos )
{
	if ( !TouchIsDown )
	{
		return;
	}

	// Check if the touch is valid for this( horizontal / vertical ) scrolling type
	bool isValid = false;
	if( fabsf( LastTouchPosistion.x - touchPos.x ) > fabsf( LastTouchPosistion.y - touchPos.y ) )
	{
		if( CurrentScrollType == HORIZONTAL_SCROLL )
		{
			isValid = true;
		}
	}
	else
	{
		if( CurrentScrollType == VERTICAL_SCROLL )
		{
			isValid = true;
		}
	}

	if( isValid )
	{
		float touchVal;
		float lastTouchVal;
		float curMove;
		if( CurrentScrollType == HORIZONTAL_SCROLL )
		{
			touchVal = touchPos.x;
			lastTouchVal = LastTouchPosistion.x;
			curMove = lastTouchVal - touchVal;
		}
		else
		{
			touchVal = touchPos.y;
			lastTouchVal = LastTouchPosistion.y;
			curMove = touchVal - lastTouchVal;
		}

		float const DISTANCE_SCALE = 0.0025f;

		Position += curMove * DISTANCE_SCALE;

		float const DISTANCE_RAMP = 150.0f;
		float const ramp = fabsf( touchVal ) / DISTANCE_RAMP;
		Deltas.Append( delta_t( curMove * DISTANCE_SCALE * ramp, ovr_GetTimeInSeconds() ) );
	}

	LastTouchPosistion = touchPos;
}

bool OvrScrollManager::IsOutOfBounds() const
{
	return ( Position < 0.0f || Position > MaxPosition );
}

bool OvrScrollManager::IsOutOfWrapBounds() const
{
	return ( Position < -ScrollBehavior.WrapAroundScrollOffset || Position > MaxPosition + ScrollBehavior.WrapAroundScrollOffset );
}

void OvrScrollManager::SetEnableAutoForwardScrolling( bool value )
{
	if( value )
	{
		float newVelocity = ScrollBehavior.AutoVerticalSpeed;
		Velocity = Velocity + ( newVelocity - Velocity ) * 0.3f;
	}
	else
	{
		if( AutoScrollInFwdDir )
		{
			// Turning off auto forward scrolling, adjust velocity so it stops at proper position
			Velocity = GetModifiedVelocity( Velocity );

			if ( WrapAroundEnabled )
			{
				PerformWrapAroundOnScrollFinish();
			}
		}
	}

	AutoScrollInFwdDir = value;
}

void OvrScrollManager::SetEnableAutoBackwardScrolling( bool value )
{
	if( value )
	{
		float newVelocity = -ScrollBehavior.AutoVerticalSpeed;
		Velocity = Velocity + ( newVelocity - Velocity ) * 0.3f;
	}
	else
	{
		if( AutoScrollBkwdDir )
		{
			// Turning off auto forward scrolling, adjust velocity so it stops at proper position
			Velocity = GetModifiedVelocity( Velocity );

			if ( WrapAroundEnabled )
			{
				PerformWrapAroundOnScrollFinish();
			}
		}
	}

	AutoScrollBkwdDir = value;
}

float OvrScrollManager::GetModifiedVelocity( const float inVelocity )
{
	// calculating distance its going to travel with touch velocity.
	float distanceItsGonnaTravel = 0.0f;
	float begginingVelocity = inVelocity;
	while( true )
	{
		begginingVelocity -= begginingVelocity * ScrollBehavior.FrictionPerSec * 0.016f;
		distanceItsGonnaTravel += begginingVelocity * 0.016f;

		if( fabsf( begginingVelocity ) < EPSILON_VEL )
		{
			break;
		}
	}

	float currentTargetPosition = Position + distanceItsGonnaTravel;
	float desiredTargetPosition = nearbyint( currentTargetPosition );

	// Calculating velocity to compute desired target position
	float desiredVelocity = EPSILON_VEL;
	if( Position >= desiredTargetPosition )
	{
		desiredVelocity = -EPSILON_VEL;
	}
	float currentPosition = Position;
	float prevPosition;

	while( true )
	{
		prevPosition = currentPosition;
		currentPosition += desiredVelocity * 0.016f;
		desiredVelocity += desiredVelocity * ScrollBehavior.FrictionPerSec * 0.016f;

		if( ( prevPosition <= desiredTargetPosition && currentPosition >= desiredTargetPosition ) ||
			( currentPosition <= desiredTargetPosition && prevPosition >= desiredTargetPosition ) )
		{
			// Found the spot where it should end up.
			// Adjust the position so it can end up at desiredPosition with desiredVelocity
			Position += ( desiredTargetPosition - prevPosition );
			break;
		}
	}

	return desiredVelocity;
}

void OvrScrollManager::PerformWrapAroundOnScrollFinish()
{
	if( IsWrapAroundTimeInitiated &&
		( MaxPosition > 0.0f ) ) // No wrap around for just 1 item/position
	{
		if( RemainingTimeForWrapAround < 0.0f )
		{
			int direction = 0;
			if( Position <= -ScrollBehavior.Padding * 0.5f )
			{
				direction = 1;

			}
			else if( Position >= MaxPosition + ScrollBehavior.Padding * 0.5f )
			{
				direction = -1;
			}

			if( direction != 0 )
			{
				// int itemCount = (int)( MaxPosition + 1.0f );
				Velocity = ScrollBehavior.WrapAroundSpeed * direction;
				CurrentScrollState = WRAP_AROUND_SCROLL;
				IsWrapAroundTimeInitiated = false;
				RemainingTimeForWrapAround = 0.0f;
			}
		}
		else
		{
			CurrentScrollState = BOUNCE_SCROLL;
			const float bounceBackVelocity = 3.0f;
			if( Position < 0.0f )
			{
				Velocity = bounceBackVelocity;
			}
			else if( Position > MaxPosition )
			{
				Velocity = -bounceBackVelocity;
			}
			Velocity = GetModifiedVelocity( Velocity );
		}
	}
}

void OvrScrollManager::SetRestrictedScrollingData( bool isRestricted, eScrollDirectionLockType touchLock, eScrollDirectionLockType controllerLock )
{
	RestrictedScrolling = isRestricted;
	TouchDirectionLocked = touchLock;
	ControllerDirectionLocked = controllerLock;
}

bool IsInRage( float val, float startRange, float endRange )
{
	return ( ( val >= startRange ) && ( val <= endRange ) );
}

float OvrScrollManager::GetWrapAroundAlphaChange()
{
	float finalAlpha = 1.0f;

	if ( ( CurrentScrollState == WRAP_AROUND_SCROLL ) &&
		 ( NeedsJumpDuringWrapAround() ) )
	{
		float fadeStart = HALF_WRAP_JUMP_ALPHA;
		float fadeEnd 	= MaxPosition - HALF_WRAP_JUMP_ALPHA;

		if ( IsInRage( Position, fadeStart, fadeEnd ) )
		{
			float fadeOutStart 		= fadeStart;
			float fadeOutEnd 		= HALF_WRAP_JUMP - 1.0f;
			float fadeBackInStart 	= MaxPosition - ( HALF_WRAP_JUMP - 1.0f );
			float fadeBackInEnd 	= fadeEnd;

			if ( IsInRage( Position, fadeOutStart, fadeOutEnd ) )
			{
				finalAlpha = LinearRangeMapFloat( Position, fadeOutStart, fadeOutEnd, 1.0f, MINIMUM_FADE_VALUE );
			}
			else if ( IsInRage( Position, fadeBackInStart, fadeBackInEnd ) )
			{
				finalAlpha = LinearRangeMapFloat( Position, fadeBackInStart, fadeBackInEnd, MINIMUM_FADE_VALUE, 1.0f );
			}
			else
			{
				finalAlpha = MINIMUM_FADE_VALUE;
			}
		}
	}

	return finalAlpha;
}

bool OvrScrollManager::NeedsJumpDuringWrapAround()
{
	return MaxPosition > ( 2 * HALF_WRAP_JUMP );
}

}
