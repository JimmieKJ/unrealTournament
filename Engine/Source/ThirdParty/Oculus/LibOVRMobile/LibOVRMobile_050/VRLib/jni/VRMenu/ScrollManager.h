/************************************************************************************

Filename    :   ScrollManager.h
Content     :
Created     :   December 12, 2014
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_ScrollManager_h )
#define OVR_ScrollManager_h

#include "Kernel/OVR_Math.h"

namespace OVR {

template< typename _type_, int _size_ >
class CircularArrayT
{
public:
	static const int MAX = _size_;

	CircularArrayT() :
		HeadIndex( 0 ),
		Num( 0 )
	{
	}

	int				GetIndex( const int offset ) const
	{
		int idx = HeadIndex + offset;
		if ( idx >= _size_ )
		{
			idx -= _size_;
		}
		else if ( idx < 0 )
		{
			idx += _size_;
		}
		return idx;
	}

	_type_ &		operator[] ( int const idx )
	{
		return Buffer[GetIndex( -idx )];
	}
	_type_ const &	operator[] ( int const idx ) const
	{
		return Buffer[GetIndex( -idx )];
	}

	bool			IsEmpty() const { return Num == 0; }
	int				GetHeadIndex() const { return GetIndex( 0 ); }
	int				GetTailIndex() const { return IsEmpty() ? GetIndex( 0 ) : GetIndex( - ( Num - 1 ) ); }

	_type_ const &	GetHead() const { return operator[]( 0 ); }
	_type_ const &	GetTail() const { return operator[]( Num - 1 ); }

	int				GetNum() const { return Num; }

	void			Clear()
	{
		HeadIndex = 0;
		Num = 0;
	}

	void			Append( _type_ const & obj )
	{
		if ( Num > 0 )
		{
			HeadIndex = GetIndex( 1 );
		}
		Buffer[HeadIndex] = obj;
		if ( Num < _size_ )
		{
			Num++;
		}
	}

private:
	_type_	Buffer[_size_];
	int		HeadIndex;
	int		Num;
};

enum eScrollType
{
	HORIZONTAL_SCROLL,
	VERTICAL_SCROLL
};

enum eScrollDirectionLockType
{
	NO_LOCK = 0,
	HORIZONTAL_LOCK,
	VERTICAL_LOCK
};

enum eScrollState
{
	SMOOTH_SCROLL,			// Regular smooth scroll
	WRAP_AROUND_SCROLL,		// Wrap around scroll : begin to end or end to begin
	BOUNCE_SCROLL			// Bounce back scroll, when user pulls
};

struct OvrScrollBehavior
{
	float 	FrictionPerSec;
	float 	Padding; 				// Padding at the beginning and end of the scroll bounds
	float 	WrapAroundScrollOffset; // Tells how scrolling should be pulled beyond its bounds to consider for wrap around
	float 	WrapAroundHoldTime; 	// Tells how long you need to hold at WrapAroundScrollOffset, in order to consider as wrap around event
	float 	WrapAroundSpeed;
	float 	AutoVerticalSpeed;

	OvrScrollBehavior()
	: FrictionPerSec( 5.0f )
	, Padding( 1.0f )
	, WrapAroundHoldTime( 0.4f )
	, WrapAroundSpeed( 7.0f )
	, AutoVerticalSpeed( 5.5f )
	{
		WrapAroundScrollOffset = Padding * 0.75f; // 75% of Padding
	}

	void SetPadding( float value )
	{
		Padding = value;
		WrapAroundScrollOffset = Padding * 0.75f;
	}
};

class OvrScrollManager
{
public:
			OvrScrollManager( eScrollType type );

	void 	Frame( float deltaSeconds, unsigned int controllerState );

	void 	TouchDown();
	void 	TouchUp();
	void 	TouchRelative( Vector3f touchPos );

	bool 	IsOutOfBounds() 		const;
	bool 	IsOutOfWrapBounds()		const;
	bool 	IsScrolling() 			const 			{ return TouchIsDown || AutoScrollInFwdDir || AutoScrollBkwdDir; }
	bool	IsTouchIsDown()			const			{ return TouchIsDown; }
	bool	IsWrapAroundEnabled() 	const			{ return WrapAroundEnabled; }

	void 	SetMaxPosition( float position ) 		{ MaxPosition = position; }
	void 	SetPosition( float position ) 			{ Position = position; }
	void 	SetVelocity( float velocity ) 			{ Velocity = velocity; }
	void	SetTouchIsDown( bool value )			{ TouchIsDown = value; }
	void	SetScrollPadding( float value )			{ ScrollBehavior.SetPadding( value ); }
	void	SetWrapAroundEnable( bool value )		{ WrapAroundEnabled = value; }
	void	SetRestrictedScrollingData( bool isRestricted, eScrollDirectionLockType touchLock, eScrollDirectionLockType controllerLock );

	float 	GetPosition() 					const 	{ return Position; }
	float 	GetMaxPosition()				const 	{ return MaxPosition; }
	float	GetVelocity()					const	{ return Velocity; }
	bool 	GetIsWrapAroundTimeInitiated() 	const 	{ return IsWrapAroundTimeInitiated; }
	float 	GetRemainingTimeForWrapAround() const 	{ return RemainingTimeForWrapAround; }
	float 	GetWrapAroundScrollOffset()		const	{ return ScrollBehavior.WrapAroundScrollOffset; }
	float 	GetWrapAroundHoldTime()			const	{ return ScrollBehavior.WrapAroundHoldTime; }
	float  	GetWrapAroundAlphaChange();

	void 	SetEnableAutoForwardScrolling( bool value );
	void 	SetEnableAutoBackwardScrolling( bool value );

private:
	struct delta_t
	{
		delta_t() : d( 0.0f ), t( 0.0f ) { }
		delta_t( float const d_, float const t_ ) : d( d_ ), t( t_ ) { }

		float	d;	// distance moved
		float	t;	// time stamp
	};

	eScrollType 							CurrentScrollType; // Is it horizontal scrolling or vertical scrolling
	OvrScrollBehavior						ScrollBehavior;
	float 									MaxPosition;

	bool 									TouchIsDown;
	Vector3f 								LastTouchPosistion;
	// Keeps track of last few(=5) touch movement information
	CircularArrayT< delta_t, 5 > 			Deltas;
	// Current State - smooth scrolling, wrap around and bounce back
	eScrollState 							CurrentScrollState;

	float 									Position;
	float 									Velocity;
	// Delta time is accumulated and scrolling physics will be done in iterations of 0.016f seconds, to stop scrolling at right spot
	float 									AccumulatedDeltaTimeInSeconds;

	bool									WrapAroundEnabled;
	bool 									IsWrapAroundTimeInitiated;
	float 									RemainingTimeForWrapAround;

	// Controller Input
	bool									AutoScrollInFwdDir;
	bool									AutoScrollBkwdDir;
	bool									IsHorizontalScrolling;

	float 									PrevAutoScrollVelocity;

	bool									RestrictedScrolling;
	eScrollDirectionLockType				TouchDirectionLocked;
	eScrollDirectionLockType				ControllerDirectionLocked;

	// Returns the velocity that stops scrolling in proper place, over an item rather than in between items.
	float 									GetModifiedVelocity( const float inVelocity );
	void									PerformWrapAroundOnScrollFinish();
	bool									NeedsJumpDuringWrapAround();
};

} // namespace OVR

#endif // OVR_ScrollManager_h
