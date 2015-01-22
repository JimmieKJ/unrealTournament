/************************************************************************************

Filename    :   FolderBrowser.h
Content     :   A menu for browsing a hierarchy of folders with items represented by thumbnails.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright, Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "FolderBrowser.h"

#include <stdio.h>
#include "../App.h"
#include "VRMenuComponent.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "DefaultComponent.h"
#include "../VrCommon.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_String_Utils.h"
#include "3rdParty/stb/stb_image.h"
#include "3rdParty/stb/stb_image_write.h"
#include "ActionComponents.h"
#include "PackageFiles.h"
#include "OVR_JSON.h"
#include "AnimComponents.h"

namespace OVR {

VRMenuId_t OvrFolderBrowser::ID_CENTER_ROOT( 1000 );
char const * OvrFolderBrowser::MENU_NAME = "FolderBrowser";

static const Vector3f FWD( 0.0f, 0.0f, 1.0f );
static const Vector3f RIGHT( 1.0f, 0.0f, 0.0f );
static const Vector3f UP( 0.0f, 1.0f, 0.0f );
static const Vector3f DOWN( 0.0f, -1.0f, 0.0f );
static const float SCROLL_REPEAT_TIME = 0.5f;
static const float EYE_HEIGHT_OFFSET = 0.0f;
static const float MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ = 1800.0f;
//static const int MAXIMUM_TITLE_SIZE = 14;

//==============================
// OvrFolderBrowserRootComponent
// This component is attached to the root parent of the folder browsers and gets to consume input first 
class OvrFolderBrowserRootComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 57514;

	OvrFolderBrowserRootComponent( OvrFolderBrowser & folderBrowser )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_TOUCH_RELATIVE | VRMENU_EVENT_TOUCH_DOWN |
			VRMENU_EVENT_TOUCH_UP | VRMENU_EVENT_OPENING )
		, FolderBrowser( folderBrowser )
		, TouchDownTime( -1.0 )
		, TimeSinceScroll ( -1.0f )
		, PositionFader( 0.0f )
		, PositionOffset( 0.0f )
		, StartHeight( 0.0f )
		, TotalTouchDistance( 0.0f )
		, ActiveDepthOffset( 3.4f )
		, WaitForTouchTime( 0.0f )
		, SwipeHintDismissed( false )
	{
	}

	virtual int		GetTypeId() const
	{ 
		return TYPE_ID;
	}

	bool IsCurrentlyScrolling() const { return PositionFader.GetFadeState() != Fader::FADE_NONE; }

	bool Scroll( VRMenuObject * self,  OvrFolderBrowser::RootDirection direction )
	{
		// Are we already scrolling?
		if ( PositionOffset != 0.0f )
		{
			return false;
		}

		// Is the scroll possible?
		const int currentIndex = GetCurrentIndex( self );	
		const int numFolders = FolderBrowser.GetNumFolders();
		switch ( direction )
		{
		case OvrFolderBrowser::MOVE_ROOT_UP:
			if ( ( currentIndex + 1 ) < numFolders )
			{
				break;
			}
			return false;
		case OvrFolderBrowser::MOVE_ROOT_DOWN:
			const OvrFolderBrowser::Folder & favorites = FolderBrowser.GetFolder( 0 );
			const int topEdge = favorites.Panels.GetSizeI() == 0 ? 0 : -1;
			if ( ( currentIndex - 1 ) > topEdge )
			{
				break;
			}
			return false;
		}

		LOG( "Scroll: %d currentIndex %d", direction, currentIndex );

		// Do the scroll 
		switch ( direction )
		{
		case OvrFolderBrowser::MOVE_ROOT_UP:			
			PositionOffset = FolderBrowser.GetPanelHeight();
			LOG("OvrFolderBrowserRootComponent MOVE_ROOT_UP startHeight: %f, posOffSet %f", StartHeight, PositionOffset );
			break;
		case OvrFolderBrowser::MOVE_ROOT_DOWN:
			PositionOffset = -FolderBrowser.GetPanelHeight();
			LOG( "OvrFolderBrowserRootComponent MOVE_ROOT_DOWN startHeight: %f, posOffSet %f", StartHeight, PositionOffset );
			break;
		}

		if ( self )
		{
			StartHeight = self->GetLocalPosition().y;
		}

		PositionFader.Reset();
		PositionFader.StartFadeIn();

		return true;
	}

	int GetCurrentIndex( VRMenuObject * self ) const
	{
		const int numFolders = FolderBrowser.GetNumFolders();
		const float deltaHeight = FolderBrowser.GetPanelHeight();
		const float maxHeight = deltaHeight * numFolders;
		const float positionRatio = self->GetLocalPosition().y / maxHeight;
		return nearbyintf( positionRatio * numFolders );
	}

private:
	virtual eMsgStatus      OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
										VRMenuObject * self, VRMenuEvent const & event )
	{
		switch( event.EventType )
        {
            case VRMENU_EVENT_FRAME_UPDATE:
                return Frame( app, vrFrame, menuMgr, self, event );
			case VRMENU_EVENT_TOUCH_DOWN:
				return TouchDown( app, vrFrame, menuMgr, self, event);
			case VRMENU_EVENT_TOUCH_UP:
				return TouchUp( app, vrFrame, menuMgr, self, event );
            case VRMENU_EVENT_TOUCH_RELATIVE:
                return TouchRelative( app, vrFrame, menuMgr, self, event );
			case VRMENU_EVENT_OPENING:
				return OnOpening( app, vrFrame, menuMgr, self, event );
            default:
                OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
                return MSG_STATUS_ALIVE;
        }
	}

	eMsgStatus Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		// Opening?
		if ( WaitForTouchTime > 0.0f )
		{
			WaitForTouchTime -= vrFrame.DeltaSeconds;
			if ( WaitForTouchTime <= 0.0f )
				WaitForTouchTime = 0.0f;
		}

		// Ready to scroll again?
		if ( TimeSinceScroll != -1.0f )
		{
			TimeSinceScroll += vrFrame.DeltaSeconds;
			if ( TimeSinceScroll >= SCROLL_REPEAT_TIME )
			{
				TimeSinceScroll = -1.0f;
			}
		}
		
		PositionFader.Update( 3.0f, vrFrame.DeltaSeconds );
		const float alpha = PositionFader.GetFinalAlpha();

    	 // update root position to move folders 
		OVR_ASSERT( self != NULL );
		const Vector3f & rootPosition = self->GetLocalPosition();
		OVR_ASSERT( alpha >= 0.0f && alpha <= 1.0f );
		DROID_ASSERT( alpha == alpha, "NanCheck" );

		if ( alpha > Mathf::SmallestNonDenormal )
		{
			const float targetPos = PositionOffset * alpha;
			self->SetLocalPosition( Vector3f( rootPosition.x, StartHeight + targetPos, rootPosition.z ) );
		}

		if ( alpha >= 1.0f )
		{
			PositionOffset = 0.0f;
			PositionFader.Reset();
		}

		const float alphaSpace = FolderBrowser.GetPanelHeight() * 2.0f;
		const float rootOffset = rootPosition.y - EYE_HEIGHT_OFFSET;
		
		// Fade + hide based on distance from origin
    	for ( int index = 0; index < FolderBrowser.GetNumFolders(); ++ index )
    	{
			//const OvrFolderBrowser::Folder & folder = FolderBrowser.GetFolder( index );
			VRMenuObject * child = menuMgr.ToObject( self->GetChildHandleForIndex( index ) );
			OVR_ASSERT( child != NULL );

			const Vector3f & position = child->GetLocalPosition();
			Vector4f color = child->GetColor();
			color.w = 0.0f;
			
			VRMenuObjectFlags_t flags = child->GetFlags();
			const float absolutePosition = fabs( rootOffset - fabs( position.y ) );
			if ( absolutePosition < alphaSpace )
			{
				// Fade in / out
				float ratio = absolutePosition / alphaSpace; 
				float finalAlpha = -( ratio * ratio ) + 1.0f; 
				color.w = Alg::Clamp( finalAlpha, 0.0f, 1.0f );
				flags &= ~(VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL);

				// Lerp the folder towards or away from viewer
				Vector3f activePosition = position;
				const float targetZ = ActiveDepthOffset * finalAlpha;
				activePosition.z = targetZ;
				child->SetLocalPosition( activePosition );
			}
			else
			{
				flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL;
			}
			child->SetColor( color );
			child->SetFlags( flags );
    	}

    	return MSG_STATUS_ALIVE;
    }

	eMsgStatus TouchDown( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}
		TouchDownTime = TimeInSeconds();
		TotalTouchDistance = 0.0f;
		StartTouchPosition.x = event.FloatValue.x;
		StartTouchPosition.y = event.FloatValue.y;
		return MSG_STATUS_ALIVE;	// don't consume -- we're just listening
	}

	eMsgStatus TouchUp( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}
		TouchDownTime = -1.0;
		bool allowPanelTouchUp = false;
		if ( TotalTouchDistance < MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ )
		{
			allowPanelTouchUp = true;
		}

		if ( WaitForTouchTime != 0.0 )
		{
			allowPanelTouchUp = false;
		}
		FolderBrowser.SetAllowPanelTouchUp( allowPanelTouchUp );


		// If scrolling up or down, don't allow touch up to be passed to children
		if ( PositionFader.GetFadeState() != Fader::FADE_NONE )
		{
			return MSG_STATUS_CONSUMED;
		}
		
		return MSG_STATUS_ALIVE; 
	}

	eMsgStatus TouchRelative( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}

		// we should not get a touch relative unless touch is down, but if they happen on the same frame, touch relative may be called first
		if ( TouchDownTime < 0.0 )
		{
			return MSG_STATUS_CONSUMED;
		}

		Vector2f currentTouchPosition( event.FloatValue.x, event.FloatValue.y );
		TotalTouchDistance += ( currentTouchPosition - StartTouchPosition ).LengthSq();

		// allow scroll?
		bool allowScroll = false;
		if ( TimeSinceScroll == -1.0f )
		{
			TimeSinceScroll = 0.f;
			allowScroll = true;
		}

		//is this a swipe up/down ?
		float const timeTouchHasBeenDown = (float)( TimeInSeconds() - TouchDownTime );
		Vector3f const touchVelocity = event.FloatValue * ( 1.0f / timeTouchHasBeenDown );

		float upDownFacing = fabsf( touchVelocity.Dot( UP ) );
		float leftRightFacing = fabsf( touchVelocity.Dot( RIGHT ) );

		if (  upDownFacing > leftRightFacing )
		{
			if ( allowScroll && PositionFader.GetFadeState() == Fader::FADE_NONE )
			{
				if ( touchVelocity.y > 0.f  ) 
				{
					LOG( "TouchRelative MOVE_ROOT_UP" );
					FolderBrowser.ScrollRoot( OvrFolderBrowser::MOVE_ROOT_UP );
				}
				else
				{
					LOG( "TouchRelative MOVE_ROOT_DOWN" );
					FolderBrowser.ScrollRoot( OvrFolderBrowser::MOVE_ROOT_DOWN );
				}
			}

			return MSG_STATUS_CONSUMED;
		}

		return MSG_STATUS_ALIVE;
	}

	eMsgStatus OnOpening( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		WaitForTouchTime = 1.5f;
		return MSG_STATUS_ALIVE;
	}

	OvrFolderBrowser &	FolderBrowser;
	float				WaitForTouchTime;		// time to wait before allowing touch up again - prevents accidental use
	double				TouchDownTime;			// the time in second when a down even was received, < 0 if touch is not down
	float 				TimeSinceScroll;		// time before allowing scroll again
	float				PositionOffset;			// used for the root scrolling up/down
	float 				ActiveDepthOffset;		// used for bringing active row towards/away from user
	float				StartHeight;
	SineFader			PositionFader;
	Vector2f			StartTouchPosition;
	float				TotalTouchDistance;
	bool				SwipeHintDismissed;
};

//==============================================================
// OvrFolderBrowserFolderRootComponent
class OvrFolderSwipeComponent : public VRMenuComponent
{
public:
	OvrFolderSwipeComponent( OvrFolderBrowser & folderBrowser, int folderIndex )
		 : VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE )  )
		, FolderBrowser( folderBrowser )
		, FolderIndex( folderIndex )
	{
	}

private:
	virtual eMsgStatus      OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
											VRMenuObject * self, VRMenuEvent const & event )
		{
			switch( event.EventType )
	        {
	            case VRMENU_EVENT_FRAME_UPDATE:
	                return Frame( app, vrFrame, menuMgr, self, event );
	            default:
	                OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
	                return MSG_STATUS_ALIVE;
	        }
		}

    eMsgStatus Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
    {
		VRMenuObjectFlags_t flags = self->GetFlags();
		if ( FolderBrowser.GetActiveFolderIndex() != FolderIndex )
		{
			flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
		}
		else
		{
			flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
		}
		self->SetFlags( flags );

#if 0
    	// Have folder title anchored to gaze
    	const Matrix4f & view = app->GetLastViewMatrix();
    	Vector3f viewForwardFlat( view.M[2][0], 0.0f, view.M[2][2] );
    	viewForwardFlat.Normalize();

    	const OvrFolderBrowser::Folder & folder = FolderBrowser.GetFolder( FolderIndex );
    	VRMenuObject * titleRootObject = menuMgr.ToObject( folder.TitleRootHandle );
    	OVR_ASSERT( titleRootObject != NULL );

    	titleRootObject->SetLocalRotation( Quatf( Matrix4f::CreateFromBasisVectors( viewForwardFlat, UP ) ) );
#endif

    	return MSG_STATUS_ALIVE;
    }

	OvrFolderBrowser & FolderBrowser;
	int FolderIndex;
};

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

//==============================================================
// OvrFolderBrowserSwipeComponent
class OvrFolderBrowserSwipeComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 58524;
	static const int MAX_DELTAS = 5;

	struct delta_t
	{
		delta_t() : d( 0.0f ), t( 0.0f ) { }
		delta_t( float const d_, float const t_ ) : d( d_ ), t( t_ ) { }

		float	d;	// distance moved
		float	t;	// time stamp
	};

	OvrFolderBrowserSwipeComponent( OvrFolderBrowser & folderBrowser, int folderIndex )
       : VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_TOUCH_RELATIVE |
							VRMENU_EVENT_TOUCH_DOWN | VRMENU_EVENT_TOUCH_UP )
		, Position( 0.0f )
		, Velocity( 0.0f )
		, Accel( 0.0f )
		, TouchDownTime( 0.0f )
		, LastRelativeMove( 0.0f )
		, Deltas()
		, TouchIsDown( false )
		, FolderBrowser( folderBrowser )
		, FolderIndex( folderIndex )
		, Friction( 5.0f )
    {
		OVR_ASSERT( FolderBrowser.GetCircumferencePanelSlots() > 0 );
    }

	virtual int	GetTypeId() const
	{
		return TYPE_ID;
	}

	bool Rotate( const OvrFolderBrowser::CategoryDirection direction )
	{

		static float const MAX_SPEED = 5.5f;
		switch ( direction )
		{
		case OvrFolderBrowser::MOVE_PANELS_LEFT:
			Velocity = MAX_SPEED;
			return true;
		case OvrFolderBrowser::MOVE_PANELS_RIGHT:
			Velocity = -MAX_SPEED;
			return true;
		}
		return false;
	}

	void SetRotationByRatio( const float ratio )
	{
		OVR_ASSERT( ratio >= 0.0f && ratio <= 1.0f );
		Position = FolderBrowser.GetFolder( FolderIndex ).MaxRotation * ratio;
		Velocity = 0.0f;
	}

	void SetRotationByIndex( const int panelIndex )
	{
		const OvrFolderBrowser::Folder & folder = FolderBrowser.GetFolder( FolderIndex );
		OVR_ASSERT( panelIndex >= 0 && panelIndex < folder.Panels.GetSizeI() );
		Position = static_cast< float >( panelIndex );
	}

	void SetAccumulatedRotation( const float rot )	
	{ 
		Position = rot;
		Velocity = 0.0f;
	}

	const float GetAccumulatedRotation() const { return Position;  }

	int CurrentPanelIndex() const { return static_cast< int >( Position ); }

	OvrMetaDatum * NextPanelData( const int step ) 
	{
		OvrFolderBrowser::Folder & folder = FolderBrowser.GetFolder( FolderIndex );

		const int numPanels = folder.Panels.GetSizeI();

		if ( numPanels == 0 )
		{
			return NULL;
		}
		
		int nextPanelIndex = CurrentPanelIndex() + step;
		if ( nextPanelIndex >= numPanels )
		{
			nextPanelIndex = 0;
		}
		else if ( nextPanelIndex < 0 )
		{
			nextPanelIndex = numPanels - 1;
		}

		SetRotationByIndex( nextPanelIndex );
		OvrFolderBrowser::Panel& panel = folder.Panels.At( nextPanelIndex );
		return panel.Data;
	}

private:
    virtual eMsgStatus      OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                    VRMenuObject * self, VRMenuEvent const & event )
    {
        OVR_ASSERT( HandlesEvent( VRMenuEventFlags_t( event.EventType ) ) );
		
		switch( event.EventType )
        {
            case VRMENU_EVENT_FRAME_UPDATE:
                return Frame( app, vrFrame, menuMgr, self, event );
			case VRMENU_EVENT_TOUCH_DOWN:
				return TouchDown( app, vrFrame, menuMgr, self, event);
			case VRMENU_EVENT_TOUCH_UP:
				return TouchUp( app, vrFrame, menuMgr, self, event ); 
            case VRMENU_EVENT_TOUCH_RELATIVE:
                return TouchRelative( app, vrFrame, menuMgr, self, event );
            default:
                OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
                return MSG_STATUS_ALIVE;
        }
    }

    eMsgStatus Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
    {
		static const float EPSILON_VEL = 0.00001f;
		bool const isActiveFolder = FolderIndex == FolderBrowser.GetActiveFolderIndex();
		if ( !isActiveFolder )
		{
			TouchIsDown = false;
		}

		const OvrFolderBrowser::Folder & folder = FolderBrowser.GetFolder( FolderIndex );
		float distanceToNextPosition = 0.0f;
		
		if ( !TouchIsDown )	// only apply velocity when touch is up
		{
			float direction = -1.0f;
			distanceToNextPosition = Position - static_cast< int >( Position );
			if ( distanceToNextPosition > 0.5f )
			{
				distanceToNextPosition = -( 1.0f - distanceToNextPosition );
				direction = 1.0f;
			}

			const float absoluteVelocity = fabsf( Velocity );

			float modifiedFriction = Friction;

			// If slowing down, reduce friction so we smoothly get to next stop
			if ( absoluteVelocity < 2.0f )
			{
				modifiedFriction = 0.50f;
			}
			
			// If too slow, keep a constant velocity and perform stiction.
			if ( absoluteVelocity < 0.65f && fabsf( distanceToNextPosition ) > EPSILON_VEL )
			{
				modifiedFriction = 0.1f;
				Velocity = direction * 0.65f;
			}

			// if close enough and slow enough, snap in to closest point and stop
			if ( absoluteVelocity < 3.4f && fabsf( distanceToNextPosition ) <= 0.01f )
			{
				Velocity = 0.0f;
				Position -= distanceToNextPosition;
			}
			else
			{
				const float velocityAdjust = modifiedFriction * vrFrame.DeltaSeconds;
				Velocity -= Velocity * velocityAdjust;
				Position += Velocity * vrFrame.DeltaSeconds;
			}
		}

// 		if ( isActiveFolder )
// 		{
// 			app->ShowInfoText( 0.0f, "nextDist = %.4f, CurPos = %.4f\nCurVel = %.4f",
// 				distanceToNextPosition, Position, Velocity );
// 		}

		Position = Alg::Clamp( Position, 0.0f, folder.MaxRotation );
		if ( Position <= 0.0f || Position >= folder.MaxRotation )
		{
			Velocity = 0.0f;
		}

		float const curRot = Position * ( Mathf::TwoPi / FolderBrowser.GetCircumferencePanelSlots() );
		Quatf rot( UP, curRot );
		rot.Normalize();
		self->SetLocalRotation( rot );

		if ( FolderIndex == FolderBrowser.GetActiveFolderIndex() )
		{
//			app->ShowInfoText( 0.0f, "Accel = %.4f\nCurVel = %.4f", Accel, CurVel );
//			app->ShowInfoText( 0.0f, "td = %s, CurPos = %.4f\nCurVel = %.4f\ncurRot = %.4f\nmaxRot = %.1f", 
//				TouchIsDown ? "true" : "false", CurPos, CurVel, curRot, folder.MaxRotation );
/*
			float v;
			float d;
			float t;
			VelocityFromDeltas( v, d, t );
			app->ShowInfoText( 0.0f, "td = %.1f, CurPos = %.4f\nCurVel = %.4f\nv = %.2f\nd = %.2f\nt = %.2f", 
				TouchDownTime, CurPos, CurVel, v, d, t );
*/
		}

		// show or hide panels based on current position
		const int numPanels = folder.Panels.GetSizeI();
		// for rendering, we want to switch to occur between panels - hence nearbyint
		const int curPanelIndex = nearbyintf( Position );
		const int extraPanels = FolderBrowser.GetNumSwipePanels() / 2;	
		for ( int i = 0; i < numPanels; ++i )
	    {
			const OvrFolderBrowser::Panel & panel = folder.Panels.At( i );
			OVR_ASSERT( panel.Handle.IsValid() );
			VRMenuObject * panelObject = menuMgr.ToObject( panel.Handle );
			OVR_ASSERT( panelObject );

			VRMenuObjectFlags_t flags = panelObject->GetFlags();
			if ( i >= curPanelIndex - extraPanels && i <= curPanelIndex + extraPanels )
			{
				flags &= ~( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL );
				panelObject->SetFadeDirection( Vector3f( 0.0f ) );
				if ( i == curPanelIndex - extraPanels )
				{
					panelObject->SetFadeDirection( -RIGHT );
				}
				else if ( i == curPanelIndex + extraPanels )
				{
					panelObject->SetFadeDirection( RIGHT );
				}
			}
			else
			{
				flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL;
			}
			panelObject->SetFlags( flags );
		}
		return MSG_STATUS_ALIVE;
    }

	void VelocityFromDeltas( float & v, float & d, float & t )
	{
		if ( Deltas.GetNum() == 0 ) {
			d = 0.0f;
			v = 0.0f;
			t = 0.0f;
			return;
		}
		// accumulate total distance
		d = 0.0f;
		for ( int i = 0; i < Deltas.GetNum(); ++i )
		{
			d += Deltas[i].d;
		}
		int hi = Deltas.GetHeadIndex();
		int ti = Deltas.GetTailIndex();
		//LOG( "hi = %i ti = %i", hi, ti );
		//LOG( "th = %.4f tt = %.4f", Deltas.GetHead().t - Deltas.GetTail().t );
		delta_t const & head = Deltas.GetHead();
		delta_t const & tail = Deltas.GetTail();
		t = head.t - tail.t;
		v = t > Mathf::SmallestNonDenormal ? d / t : 0.0f;
	}

	eMsgStatus TouchDown( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		bool const wasMoving = fabs( Velocity ) > 0.001f;
		FolderBrowser.SetAllowPanelTouchUp( wasMoving );

		TouchIsDown = true;
		TouchDownTime = TimeInSeconds();
		TouchDownFrame = 0;
		Velocity = 0.0f;
		LastRelativeMove = 0.0f;
		Deltas.Clear();

		return MSG_STATUS_ALIVE;
	}

	eMsgStatus TouchUp( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		// start applying velocity once the touch comes up
		TouchIsDown = false;
		TouchDownTime = -1.0f;
		TouchDownFrame = 0;
		Deltas.Clear();
		return MSG_STATUS_ALIVE; // don't consume -- we are just listening
	}

    eMsgStatus TouchRelative( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
    {
		if ( !TouchIsDown )
		{
			return MSG_STATUS_ALIVE;
		}
		
		float const DISTANCE_RAMP = 150.0f;
		float const ramp = fabsf( event.FloatValue.x ) / DISTANCE_RAMP;
		float const DISTANCE_SCALE = 0.0025f;
		float const curMove = - ( event.FloatValue.x - LastRelativeMove );
		LastRelativeMove = event.FloatValue.x;

		if ( TouchDownFrame > 0 )
		{
			Deltas.Append( delta_t( curMove * DISTANCE_SCALE * ramp, TimeInSeconds() ) );

			float d;
			float t;
			VelocityFromDeltas( Velocity, d, t );
			d /= Deltas.GetNum();
			Position += d;
		}
		TouchDownFrame++;

		return MSG_STATUS_CONSUMED;
    }

private:
	CircularArrayT< delta_t, MAX_DELTAS >	Deltas;

	OvrFolderBrowser&	FolderBrowser;

	float				Position;
	float				Velocity;
	float				Accel;
	float				TouchDownTime;
	int					TouchDownFrame;		// used to ignore the relative move on the very first frame since it can be too large
	float				LastRelativeMove;
	float				Friction;
	bool				TouchIsDown;			// true if touch is being held
	int 				FolderIndex;			// Correlates the folder browser component to the folder it belongs to
};

//==============================
// OvrMetaData

const char * OvrMetaData::CATEGORIES = "Categories";
const char * OvrMetaData::DATA = "Data";
const char * OvrMetaData::TITLE = "Title";
const char * OvrMetaData::URL = "Url";
const char * OvrMetaData::FAVORITES_TAG = "Favorites";
const char * OvrMetaData::NAME = "name";
const char * OvrMetaData::TAGS = "tags";
const char * OvrMetaData::CATEGORY = "category";
const char * OvrMetaData::URL_INNER = "url";
const char * OvrMetaData::TITLE_INNER = "title";
const char * OvrMetaData::AUTHOR_INNER = "author";

void OvrMetaData::InitFromDirectory( const char * relativePath, const SearchPaths & paths, const OvrMetaDataFileExtensions & fileExtensions )
{
	LOG( "OvrMetaData::InitFromDirectory( %s )", relativePath );

	// Find all the files - checks all search paths
	StringHash< String > uniqueFileList = RelativeDirectoryFileList( paths, relativePath );
	Array<String> fileList;
	for ( StringHash< String >::ConstIterator iter = uniqueFileList.Begin(); iter != uniqueFileList.End(); ++iter )
	{
		fileList.PushBack( iter->First );
	}
	SortStringArray( fileList );

	Category currentCategory;
	currentCategory.CategoryName = ExtractFileBase( relativePath );

	LOG( "OvrMetaData start category: %s", currentCategory.CategoryName.ToCStr() );

	Array< String > subDirs;
	
	// Grab the categories and loose files
	for ( int i = 0; i < fileList.GetSizeI(); i++ )
	{
		const String & s = fileList[ i ];
		const String fileBase = ExtractFileBase( s );

		// subdirectory - add category
		if ( MatchesExtension( s, "/" ) )
		{	
			subDirs.PushBack( s );
			continue;
		}

		// See if we want this loose-file
		if ( !ShouldAddFile( s.ToCStr(), fileExtensions ) )
		{
			continue;
		}

		// Add loose file
		const int dataIndex = MetaData.GetSizeI();
		OvrMetaDatum data( dataIndex );
		data.Title = fileBase;
		data.Author = "Unspecified Author";
		data.Tags.PushBack( currentCategory.CategoryName );
		if ( paths.GetFullPath( s, data.Url ) )
		{
			StringHash< int >::ConstIterator iter = UrlToIndex.Find( data.Url );
			if ( iter == UrlToIndex.End() )
			{
				UrlToIndex.Add( data.Url, dataIndex );
			}
			else
			{
				LOG( "OvrMetaData::InitFromDirectory found duplicate url %s", data.Url.ToCStr() );
			}

			MetaData.PushBack( data );

			LOG( "OvrMetaData adding datum %s with index %d to %s", data.Title.ToCStr(), dataIndex, currentCategory.CategoryName.ToCStr() );

			// Register with category
			currentCategory.DatumIndicies.PushBack( dataIndex );
		}
		else
		{
			LOG( "OvrMetaData::InitFromDirectory failed to find %s", s.ToCStr() );
		}
	}

	if ( !currentCategory.DatumIndicies.IsEmpty() )
	{
		Categories.PushBack( currentCategory );
	}

	// Recurse into subdirs
	for ( int i = 0; i < subDirs.GetSizeI(); ++i )
	{
		const String & subDir = subDirs.At( i );
		InitFromDirectory( subDir.ToCStr(), paths, fileExtensions );
	}
}

void OvrMetaData::RenameCategory( const char * currentName, const char * newName )
{
	for ( int i = 0; i < Categories.GetSizeI(); ++i )
	{
		Category & cat = Categories.At( i );
		if ( cat.CategoryName == currentName )
		{
			cat.CategoryName = newName;
			break;
		}
	}
}

void OvrMetaData::WriteMetaFile( const char * metaFile ) const
{
	LOG( "Writing metafile from apk" );

	if ( FILE * newMetaFile = fopen( FilePath.ToCStr(), "w" ) )
	{
		int bufferLength = 0;
		void * 	buffer = NULL;
		String assetsMetaFile = "assets/";
		assetsMetaFile += metaFile;
		ovr_ReadFileFromApplicationPackage( assetsMetaFile.ToCStr(), bufferLength, buffer );
		if ( !buffer )
		{
			LOG( "OvrMetaData failed to read %s", assetsMetaFile.ToCStr() );
		}
		else
		{
			fwrite( buffer, 1, bufferLength, newMetaFile );
			free( buffer );
		}
		fclose( newMetaFile );
	}
	else
	{
		FAIL( "OvrMetaData failed to create %s - check app permissions", FilePath.ToCStr() );
	}
}

void OvrMetaData::InitFromDirectoryMergeMeta( const char * relativePath, const SearchPaths & paths,
	const OvrMetaDataFileExtensions & fileExtensions, const char * metaFile, const char * packageName )
{
	LOG( "OvrMetaData::InitFromDirectoryWithMeta" );

	FilePath = "/data/data/";
	FilePath += packageName;
	FilePath += "/cache/";
	FilePath += metaFile;

	JSON * DataFile = JSON::Load( FilePath );
	if ( DataFile == NULL )
	{
		// If this is the first run, or we had an error loading the file, we copy the meta file from assets to app's cache
		WriteMetaFile( metaFile );

		// try loading it again
		DataFile = JSON::Load( FilePath );
		if ( DataFile == NULL )
		{
			LOG( "OvrMetaData failed to load JSON meta file: %s", metaFile );
			// Create a Favorites folder since there isn't meta data
			Category favorites;
			favorites.CategoryName = FAVORITES_TAG;
			Categories.PushBack( favorites );
		}
	}

	// Init from whatever is in the directories - adding new categories if found
	InitFromDirectory( relativePath, paths, fileExtensions );
	
	if ( DataFile != NULL )
	{
		// Read in the stored categories
		Array< String > storedCategories;
		const JsonReader categories( DataFile->GetItemByName( CATEGORIES ) );

		if ( categories.IsArray() )
		{
			while ( const JSON * nextElement = categories.GetNextArrayElement() )
			{
				const JsonReader category( nextElement );
				if ( category.IsObject() )
				{
					const String categoryName = category.GetChildStringByName( NAME );
					LOG( "storedCategories adding category: %s", categoryName.ToCStr() );
					storedCategories.PushBack( categoryName );
				}
			}
		}

		// Read in the stored data
		Array< OvrMetaDatum > storedMetaData;
		ExtractMetaData( DataFile, paths, storedMetaData );

		// Reconcile the stored data vs the data read in
		ReconcileData( storedCategories, storedMetaData );

		// Recreate indices which may have changed after reconciliation
		RegenerateCategoryIndices();
		
		// Delete any newly empty categories except Favorites 
		for ( int catIndex = 1; catIndex < Categories.GetSizeI(); ++catIndex )
		{
			if ( Categories.At( catIndex ).DatumIndicies.IsEmpty() )
			{
				Categories.RemoveAt( catIndex );
			}
		}
		DataFile->Release();
	}
		
	// Rewrite new data
	DataFile = MetaDataToJson();
	if ( DataFile == NULL )
	{
		FAIL( "OvrMetaData::InitFromDirectoryCheckMeta failed to generate JSON meta file: %s", DataFile );
	}

	DataFile->Save( FilePath );

	LOG( "OvrMetaData::InitFromDirectoryCheckMeta created %s", FilePath.ToCStr() );
	DataFile->Release();
}

void OvrMetaData::ReconcileData( Array< String > & storedCategories, Array< OvrMetaDatum > & storedMetaData )
{
	// Fix the read in meta data using the stored
	for ( int i = 0; i < storedMetaData.GetSizeI(); ++i )
	{
		OvrMetaDatum & storedMetaDatum = storedMetaData.At( i );

		StringHash< int >::ConstIterator iter = UrlToIndex.Find( storedMetaDatum.Url );
		if ( iter != UrlToIndex.End() )
		{
			const int metaDatumIndex = iter->Second;
			OVR_ASSERT( metaDatumIndex >= 0 && metaDatumIndex < MetaData.GetSizeI() );

			OvrMetaDatum & dirDatum = MetaData.At( metaDatumIndex );
			//LOG( "Found stored metadata for %s", storedMetaDatum.Url.ToCStr() );
			Alg::Swap( dirDatum.Tags, storedMetaDatum.Tags );
// 			for ( int t = 0; t < dirDatum.Tags.GetSizeI(); ++t )
// 			{
// 				LOG( "tag: %s", dirDatum.Tags.At( t ).ToCStr() );
// 			}
			Alg::Swap( dirDatum.Title, storedMetaDatum.Title );
			Alg::Swap( dirDatum.Author, storedMetaDatum.Author );

		}
	}

	// Reconcile categories
	// We want Favorites always at the top
	// Followed by user created categories
	// Finally we want to maintain the order of the retail categories (defined in assets/meta.json)
	Array< Category > finalCategories;

	Category favorites;
	favorites.CategoryName = storedCategories.At( 0 );
	OVR_ASSERT( favorites.CategoryName == FAVORITES_TAG );
	finalCategories.PushBack( favorites );

	StringHash< bool > StoredCategoryMap; // using as set
	for ( int i = 0; i < storedCategories.GetSizeI(); ++i )
	{
		StoredCategoryMap.Add( storedCategories.At( i ), true );
	}
	
	// Now add the read in categories if they differ
	for ( int i = 0; i < Categories.GetSizeI(); ++i )
	{
		const Category & readInCategory = Categories.At( i );
		StringHash< bool >::ConstIterator iter = StoredCategoryMap.Find( readInCategory.CategoryName );

		if ( iter == StoredCategoryMap.End() )
		{
			finalCategories.PushBack( readInCategory );
		}
	}

	// Finally fill in the stored in categories after user made ones
	for ( int i = 1; i < storedCategories.GetSizeI(); ++i )
	{
		Category cat;
		cat.CategoryName = storedCategories.At( i );
		finalCategories.PushBack( cat );
	}

	// Now replace Categories
	Alg::Swap( Categories, finalCategories );
}

void OvrMetaData::ExtractMetaData( JSON * dataFile, const SearchPaths & paths, Array< OvrMetaDatum > & outMetaData ) const
{
	const JsonReader data( dataFile->GetItemByName( DATA ) );
	if ( data.IsArray() )
	{
		int jsonIndex = 0;
		int datumIndex = 0;
		while ( const JSON * nextElement = data.GetNextArrayElement() )
		{
			const JsonReader datum( nextElement );
			if ( datum.IsObject() )
			{
				OvrMetaDatum data( jsonIndex++ );

				const JsonReader tags( datum.GetChildByName( TAGS ) );
				if ( tags.IsArray() )
				{
					while ( const JSON * tagElement = tags.GetNextArrayElement() )
					{
						const JsonReader tag( tagElement );
						if ( tag.IsObject() )
						{
							data.Tags.PushBack( tag.GetChildStringByName( CATEGORY ) );
						}
					}
				}
				
				const String relativeUrl( datum.GetChildStringByName( URL_INNER ) );

				if ( !paths.GetFullPath( relativeUrl, data.Url ) )
				{
					LOG( "OvrMetaData::ExtractMetaData failed to find %s", relativeUrl.ToCStr() );
				}

				// failed to find data - don't add it
				if ( !data.Url.IsEmpty() )
				{
					data.Title = datum.GetChildStringByName( TITLE_INNER );
					data.Author = datum.GetChildStringByName( AUTHOR_INNER );
					//LOG( "OvrMetaData::ExtractMetaData adding datum %s with title %s", data.Url.ToCStr(), data.Title.ToCStr() );
					outMetaData.PushBack( data );
				}
			}
		}
	}
}

void OvrMetaData::RegenerateCategoryIndices()
{
	for ( int catIndex = 0; catIndex < Categories.GetSizeI(); ++catIndex )
	{
		Category & cat = Categories.At( catIndex );
		cat.DatumIndicies.Clear();
	}
	
	for ( int metaDataIndex = 0; metaDataIndex < MetaData.GetSizeI(); ++metaDataIndex )
	{
		OvrMetaDatum & datum = MetaData.At( metaDataIndex );
		Array< String > & tags = datum.Tags;

		if ( tags.At( 0 ) == FAVORITES_TAG && tags.GetSizeI() > 1 )
		{
			Alg::Swap( tags.At( 0 ), tags.At( 1 ) );
		}

		for ( int tagIndex = 0; tagIndex < tags.GetSizeI(); ++tagIndex )
		{
			const String & tag = tags[ tagIndex ];
			if ( !tag.IsEmpty() )
			{
				if ( Category * category = GetCategory( tag ) )
				{
					//LOG( "OvrMetaData adding datum %s with index %d to %s", data.Title.ToCStr(), t, category->CategoryName.ToCStr() );
					category->DatumIndicies.PushBack( metaDataIndex );
				}
			}
		}
	}
}

JSON * OvrMetaData::MetaDataToJson()
{
	JSON * DataFile = JSON::CreateObject();

	// Add categories
	JSON * newCategoriesObject = JSON::CreateArray();

	for ( int c = 0; c < Categories.GetSizeI(); ++c )
	{
		if ( JSON * catObject = JSON::CreateObject() )
		{
			catObject->AddStringItem( NAME, Categories.At( c ).CategoryName.ToCStr() );
			newCategoriesObject->AddArrayElement( catObject );
		}
	}
	DataFile->AddItem( CATEGORIES, newCategoriesObject );

	// Add meta data
	JSON * newDataObject = JSON::CreateArray();

	for ( int i = 0; i < MetaData.GetSizeI(); ++i )
	{
		const OvrMetaDatum & metaDatum = MetaData.At( i );

		if ( JSON * datumObject = JSON::CreateObject() )
		{
			datumObject->AddStringItem( TITLE_INNER, metaDatum.Title.ToCStr() );
			datumObject->AddStringItem( AUTHOR_INNER, metaDatum.Author.ToCStr() );
			datumObject->AddStringItem( URL_INNER, metaDatum.Url.ToCStr() );

			if ( JSON * newTagsObject = JSON::CreateArray() )
			{
				for ( int t = 0; t < metaDatum.Tags.GetSizeI(); ++t )
				{
					if ( JSON * tagObject = JSON::CreateObject() )
					{
						tagObject->AddStringItem( CATEGORY, metaDatum.Tags.At( t ).ToCStr() );
						newTagsObject->AddArrayElement( tagObject );
					}
				}

				datumObject->AddItem( TAGS, newTagsObject );
			}
			newDataObject->AddArrayElement( datumObject );
		}
	}
	DataFile->AddItem( DATA, newDataObject );

	return DataFile; 
}

TagAction OvrMetaData::ToggleTag( OvrMetaDatum * metaDatum, const String & newTag )
{
	JSON * DataFile = JSON::Load( FilePath );
	if ( DataFile == NULL )
	{
		FAIL( "OvrMetaData failed to load JSON meta file: %s", FilePath.ToCStr() );
	}

	OVR_ASSERT( DataFile );
	OVR_ASSERT( metaDatum );

	// First update the local data
	TagAction action = TAG_ERROR;
	for ( int t = 0; t < metaDatum->Tags.GetSizeI(); ++t )
	{
		if ( metaDatum->Tags.At( t ) == newTag )
		{
			action = TAG_REMOVED;
			metaDatum->Tags.RemoveAt( t );
			break;
		}
	}

	if ( action == TAG_ERROR )
	{
		metaDatum->Tags.PushBack( newTag );
		action = TAG_ADDED;
	}

	// Then serialize 
	JSON * newTagsObject = JSON::CreateArray();
	OVR_ASSERT( newTagsObject );

	newTagsObject->Name = TAGS;

	for ( int t = 0; t < metaDatum->Tags.GetSizeI(); ++t )
	{
		if ( JSON * tagObject = JSON::CreateObject() )
		{
			tagObject->AddStringItem( CATEGORY, metaDatum->Tags.At( t ).ToCStr() );
			newTagsObject->AddArrayElement( tagObject );
		}
	}

	if (JSON * data = DataFile->GetItemByName( DATA ) )
	{
		if ( JSON * datum = data->GetItemByIndex( metaDatum->JsonIndex ) )
		{
			if ( JSON * tags = datum->GetItemByName( TAGS ) )
			{
				tags->ReplaceNodeWith( newTagsObject );
				tags->Release();
				DataFile->Save( FilePath );
			}
		}
	}
	DataFile->Release();
	return action;
}

void OvrMetaData::AddCategory( const String & name )
{
	Category cat;
	cat.CategoryName = name;
	Categories.PushBack( cat );
}

OvrMetaData::Category * OvrMetaData::GetCategory(const String & categoryName )
{
	const int numCategories = Categories.GetSizeI();
	for ( int i = 0; i < numCategories; ++i )
	{
		Category & category = Categories.At( i );
		if ( category.CategoryName == categoryName )
		{
			return &category;
		}
	}
	return NULL;
}

const OvrMetaDatum &	OvrMetaData::GetMetaDatum( const int index ) const
{
	OVR_ASSERT( index >= 0 && index < MetaData.GetSizeI() );
	return MetaData.At( index );
}


bool OvrMetaData::GetMetaData( const Category & category, Array< const OvrMetaDatum * > & outMetaData ) const
{
	const int numPanos = category.DatumIndicies.GetSizeI();
	for ( int i = 0; i < numPanos; ++i )
	{
		const int metaDataIndex = category.DatumIndicies.At( i );
		OVR_ASSERT( metaDataIndex >= 0 && metaDataIndex < MetaData.GetSizeI() );
		const OvrMetaDatum * panoData = &MetaData.At( metaDataIndex );
		//LOG( "Getting MetaData %d title %s from category %s", metaDataIndex, panoData->Title.ToCStr(), category.CategoryName.ToCStr() );
		outMetaData.PushBack( &MetaData.At( metaDataIndex ) );
	}
	return true;
}

bool OvrMetaData::ShouldAddFile( const char * filename, const OvrMetaDataFileExtensions & fileExtensions )
{
	for ( int index = 0; index < fileExtensions.BadExtensions.GetSizeI(); ++index )
	{
		const String & ext = fileExtensions.BadExtensions.At( index );
		if ( strstr( filename, ext.ToCStr() ) )
		{
			return false;
		}
	}

	for ( int index = 0; index < fileExtensions.GoodExtensions.GetSizeI(); ++index )
	{
		const String & ext = fileExtensions.GoodExtensions.At( index );
		if ( strstr( filename, ext.ToCStr() ) )
		{
			return true;
		}
	}
	
	return false;
}

//==============================
// OvrFolderBrowser
unsigned char * OvrFolderBrowser::ThumbPanelBG = NULL;

OvrFolderBrowser::OvrFolderBrowser(
		App * app,
		OvrVRMenuMgr & menuMgr,
		BitmapFont const & font,
		SearchPaths & paths,
		OvrMetaData & metaData,
		unsigned	defaultFileTexture_,
		unsigned	defaultDirectoryTexture_,
		float panelWidth /*= 512.0f*/,
		float panelHeight /*= 512.0f*/,
		float radius_ /*= 3.0f*/,
		unsigned 	numSwipePanels /*= 5*/, 
		unsigned thumbWidth /*= 256*/,
		unsigned thumbHeight /*= 160 */ )
	: VRMenu( MENU_NAME )
	, AppPtr( app )
	, MenuMgr( menuMgr )
	, Font( font )
	, Paths( paths )
	, DefaultFileTexture( defaultFileTexture_)
	, DefaultDirectoryTexture( defaultDirectoryTexture_ )
	, NumSwipePanels( numSwipePanels )
	, CurrentPanelData( NULL )
	, TextureCommands( 10000 )
	, BackgroundCommands( 10000 )
	, MetaData( metaData )
	, ThumbWidth( thumbWidth )
	, ThumbHeight( thumbHeight )
	, PanelTextSpacingScale( 0.5f )
	, FolderTitleSpacingScale( 0.5f )
	, NoMedia( false )
	, OnEnterMenuRootAdjust( MOVE_ROOT_NONE )
	, SwipeHeldDown( false )
	, DebounceTime( 0.0f )
	, ScrollHintShown( false )
	, AllowPanelTouchUp( false )
{
	//  Load up thumbnail alpha from panel.tga
	if ( ThumbPanelBG == NULL )
	{
		void * 	buffer;
		int		bufferLength;

		const char * panel = NULL;

		if ( ThumbWidth == ThumbHeight )
		{
			panel = "res/raw/panel_square.tga";
		}
		else
		{
			panel = "res/raw/panel.tga";
		}

		ovr_ReadFileFromApplicationPackage( panel, bufferLength, buffer );

		int panelW = 0;
		int panelH = 0;
		ThumbPanelBG = stbi_load_from_memory( ( stbi_uc const * )buffer, bufferLength, &panelW, &panelH, NULL, 4 );

		OVR_ASSERT( ThumbPanelBG && panelW == ThumbWidth && panelH == ThumbHeight );
	}

	// spawn the thumbnail loading thread with the command list
	pthread_t loadingThread;
	const int createErr = pthread_create( &loadingThread, NULL /* default attributes */,
		&ThumbnailThread, this );
	if ( createErr != 0 )
	{
		LOG( "pthread_create returned %i", createErr );
	}

	PanelWidth = panelWidth * VRMenuObject::DEFAULT_TEXEL_SCALE;
	PanelHeight = panelHeight * VRMenuObject::DEFAULT_TEXEL_SCALE;
	Radius = radius_;
	const float circumference = Mathf::TwoPi * Radius;

	CircumferencePanelSlots = ( int )( floor( circumference / PanelWidth ) );
	VisiblePanelsArcAngle = (( float )( NumSwipePanels + 1 ) / CircumferencePanelSlots ) * Mathf::TwoPi;

	Array< VRMenuComponent* > comps;
	comps.PushBack( new OvrFolderBrowserRootComponent( *this ) );
	Init( MenuMgr, Font, 0.0f, VRMenuFlags_t(), comps );
}

OvrFolderBrowser::~OvrFolderBrowser()
{
	if ( ThumbPanelBG != NULL )
	{
		free( ThumbPanelBG );
		ThumbPanelBG = NULL;
	}
}

void OvrFolderBrowser::Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
	BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId )
{
	// Gamepad input
	static const float DEBOUNCE_TIME = 0.25f;
	if ( SwipeHeldDown )
	{	
		DebounceTime -= vrFrame.DeltaSeconds;
	}
	else
	{
		DebounceTime = 0.0f;
	}

	if ( vrFrame.Input.buttonState & ( BUTTON_LSTICK_UP | BUTTON_DPAD_UP ) )
	{
		LOG( "Frame_Impl BUTTON_LSTICK_UP MOVE_ROOT_DOWN" );
		ScrollRoot( MOVE_ROOT_DOWN );
	}
	else if ( vrFrame.Input.buttonState & ( BUTTON_LSTICK_DOWN | BUTTON_DPAD_DOWN ) )
	{
		LOG( "Frame_Impl BUTTON_LSTICK_UP MOVE_ROOT_UP" );
		ScrollRoot( MOVE_ROOT_UP );
	}
	else if ( DebounceTime <= 0.0f && vrFrame.Input.buttonState & ( BUTTON_LSTICK_LEFT | BUTTON_DPAD_LEFT ) )
	{
		if ( !SwipeHeldDown )
		{
			DebounceTime = DEBOUNCE_TIME;
		}
		SwipeHeldDown = true;
		RotateCategory( MOVE_PANELS_RIGHT );
	}
	else if ( DebounceTime <= 0.0f && vrFrame.Input.buttonState & ( BUTTON_LSTICK_RIGHT | BUTTON_DPAD_RIGHT ) )
	{
		if ( !SwipeHeldDown )
		{
			DebounceTime = DEBOUNCE_TIME;
		}
		SwipeHeldDown = true;
		RotateCategory( MOVE_PANELS_LEFT );
	}
	// Automate scroll up/down for testing
// 	else
// 	{
// 		RootDirection randDir = static_cast< RootDirection >( (rand() % 2) + 1 );
// 		LOG( "Frame_Impl randDir %d", randDir );
// 		ScrollRoot( randDir );
// 	}

	if ( vrFrame.Input.buttonReleased & ( BUTTON_LSTICK_LEFT | BUTTON_DPAD_LEFT ) || 
		vrFrame.Input.buttonReleased & ( BUTTON_LSTICK_RIGHT | BUTTON_DPAD_RIGHT ) )
	{
		SwipeHeldDown = false;
	}
	
	// Check for thumbnail loads
	while ( 1 )
	{
		const char * cmd = TextureCommands.GetNextMessage();
		if ( !cmd )
		{
			break;
		}

		//LOG( "TextureCommands: %s", cmd );
		LoadThumbnailToTexture( cmd );
		free( ( void * )cmd );
	}
}

void OvrFolderBrowser::Open_Impl( App * app, OvrGazeCursor & gazeCursor )
{
	if ( NoMedia )
	{
		return;
	}

	// Rebuild favorites if not empty 
	OnBrowserOpen();
	
	LOG( "Open_Impl ScrollRoot %d", OnEnterMenuRootAdjust );
	ScrollRoot( OnEnterMenuRootAdjust );
	OnEnterMenuRootAdjust = MOVE_ROOT_NONE;
}

void OvrFolderBrowser::BuildMenu()
{
	// move the root up to eye height
	VRMenuObject * root = MenuMgr.ToObject( GetRootHandle() );
	if ( root != NULL )
	{
		Vector3f pos = root->GetLocalPosition();
		pos.y += EYE_HEIGHT_OFFSET;
		root->SetLocalPosition( pos );
	}

	Array< OvrMetaData::Category > categories = MetaData.GetCategories();

	// load folders and position
	const int numFolders = categories.GetSizeI();
	int mediaCount = 0;
	for ( int folderIndex = 0; folderIndex < numFolders; ++folderIndex )
	{
		LOG( "Loading folder %i named %s", folderIndex, categories.At( folderIndex ).CategoryName.ToCStr() );
		LoadCategory( folderIndex );

		// Set up initial positions - 0 in center, the rest ascending in order below it
		const Folder & folder = GetFolder( folderIndex );
		mediaCount += folder.Panels.GetSizeI();
		VRMenuObject * folderObject = MenuMgr.ToObject( folder.Handle );
		OVR_ASSERT( folderObject != NULL );
		folderObject->SetLocalPosition( ( DOWN * PanelHeight * folderIndex ) + folderObject->GetLocalPosition() );
	}

	if ( mediaCount == 0 )
	{
		String title;
		String imageFile;
		String message;
		OnMediaNotFound( title, imageFile, message );

		// Create a folder if we didn't create at least one to display no media info
		if ( Folders.GetSizeI() < 1 )
		{
			MetaData.AddCategory( "No Media" );
			LoadCategory( 0 );
		}

		// Set title 
		const Folder & folder = Folders.At( 0 );
		VRMenuObject * folderTitleObject = MenuMgr.ToObject( folder.TitleHandle );
		OVR_ASSERT( folderTitleObject != NULL );
		folderTitleObject->SetText( title );
		VRMenuObjectFlags_t flags = folderTitleObject->GetFlags();
		flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
		folderTitleObject->SetFlags( flags );

		// Add no media panel
		Array< VRMenuObjectParms const * > parms;

		const Vector3f dir( -FWD );
		const Posef panelPose( Quatf(), dir * Radius );
		const Vector3f panelScale( 1.0f );
		const Posef textPose( Quatf(), Vector3f( 0.0f, -PanelHeight * PanelTextSpacingScale, 0.0f ) );

		VRMenuSurfaceParms panelSurfParms( "panelSurface",
			imageFile, SURFACE_TEXTURE_DIFFUSE,
			NULL, SURFACE_TEXTURE_MAX,
			NULL, SURFACE_TEXTURE_MAX );

		const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 0.5f );
		VRMenuObjectParms * p = new VRMenuObjectParms( VRMENU_STATIC, Array< VRMenuComponent* >(),
			panelSurfParms, message, panelPose, panelScale, textPose, Vector3f( 1.3f ), fontParms, VRMenuId_t(),
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

		parms.PushBack( p );
		AddItems( MenuMgr, Font, parms, folder.SwipeHandle, true ); // PARENT: folder.TitleRootHandle
		parms.Clear();

		NoMedia = true;
		return;
	}

	if ( Folders.GetSizeI() < 2 )
	{
		// Don't bother showing scroll hint
		ScrollHintShown = true;
	}
	else // build scroll hint
	{
		// Set up "swipe to browse" hint
		// We want the hint to take up the top "empty" folder space at launch and go away once user scrolls down
		// If the top folder is empty, use its position, otherwise attach one above
		const int positionIndex = Folders.At( 0 ).Panels.IsEmpty() ? 0 : 1;
		const int numFrames = 6;
		const int numLeads = 1;
		const int numTrails = 2;
		const int numIndicatorChildren = 3;
		const float swipeFPS = 4.0f;
		const float factor = 1.0f / 8.0f;

		VRMenuId_t swipeDownId( ID_CENTER_ROOT.Get() + 407 );

		Array< VRMenuObjectParms const * > parms;
		const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 1.0f );
		Array< VRMenuComponent* > comps;
		comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.25f, Vector4f( 0.f ) ) );
		comps.PushBack( new OvrTrailsAnimComponent( swipeFPS, true, numFrames, numLeads, numTrails ) );
		const Posef hintPose( Quatf(), -FWD * ( 0.52f * Radius) + UP * PanelHeight * 0.2f );
		VRMenuObjectParms swipeHintRootParms(
			VRMENU_CONTAINER,
			comps,
			VRMenuSurfaceParms(),
			"hint root",
			hintPose,
			Vector3f( 1.0f ),
			fontParms,
			swipeDownId,
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ),
			VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
		parms.PushBack( &swipeHintRootParms );
		AddItems( MenuMgr, Font, parms, GetRootHandle(), false );
		parms.Clear();
		comps.Clear();

		ScrollHintHandle = HandleForId( MenuMgr, swipeDownId );
		VRMenuObject * swipeHintObject = MenuMgr.ToObject( ScrollHintHandle );
		OVR_ASSERT( swipeHintObject != NULL );

		swipeHintObject->SetLocalPosition( ( UP * PanelHeight * positionIndex ) + swipeHintObject->GetLocalPosition() );
		swipeHintObject->SetColor( Vector4f( 1.0f, 1.0f, 1.0f, 0.5f ) );
		// Hint text
		Posef textPose( Quatf(), DOWN * 0.2f );
		VRMenuObjectParms titleParms(
			VRMENU_STATIC,
			Array< VRMenuComponent* >(),
			VRMenuSurfaceParms(),
			"Swipe to Browse",
			textPose,
			Vector3f( 0.7f ),
			fontParms,
			VRMenuId_t(),
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
			VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
		parms.PushBack( &titleParms );
		AddItems( MenuMgr, Font, parms, ScrollHintHandle, true ); // PARENT: SwipeHintHandle
		parms.Clear();

		const char * swipeDownIcon = "res/raw/nav_arrow_down.png";
		VRMenuSurfaceParms downIndicatorSurfaceParms( "swipeDownSurface",
			swipeDownIcon, SURFACE_TEXTURE_DIFFUSE,
			NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		const float iconSpacing = 25 * VRMenuObject::DEFAULT_TEXEL_SCALE;

		for ( int i = 0; i < numIndicatorChildren; ++i )
		{
			const Vector3f pos = ( DOWN * iconSpacing * i ) + ( FWD * i * 0.01f );
			VRMenuObjectParms swipeDownFrame( VRMENU_STATIC, Array< VRMenuComponent* >(), downIndicatorSurfaceParms, "",
				Posef( Quatf(), pos ), Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), fontParms, VRMenuId_t(),
				VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			parms.PushBack( &swipeDownFrame );
			AddItems( MenuMgr, Font, parms, ScrollHintHandle, false );
			parms.Clear();
		}

		if ( OvrTrailsAnimComponent* animComp = swipeHintObject->GetComponentByName< OvrTrailsAnimComponent >() )
		{
			animComp->Play();
		}
	}

	// Hide "Favorites" Category if empty
	if ( Folders.GetSizeI() > 1 && Folders.At( 0 ).Panels.IsEmpty() )
	{
		LOG( "Hide Favorites MOVE_ROOT_UP" );
		ScrollRoot( MOVE_ROOT_UP );
		// We still want to show the scroll hint 
		ScrollHintShown = false;
		SetScrollHintVisible( true );
	}
}

void OvrFolderBrowser::LoadCategory( int folderIndex )
{
	// Load a category to build swipe folder
	//
	const OvrMetaData::Category & category = MetaData.GetCategory( folderIndex );

	Array< VRMenuObjectParms const * > parms;
	const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 1.0f );

	// Create internal folder struct
	Folder folder( category.CategoryName );
	const int numPanels = category.DatumIndicies.GetSizeI();
	LOG( "Building Folder %s with %d panels", category.CategoryName.ToCStr(), numPanels );
	
	// Create OvrFolderBrowserFolderRootComponent root for folder
	VRMenuId_t folderId( ID_CENTER_ROOT.Get() + folderIndex + 1 );
	Array< VRMenuComponent* > comps;
	comps.PushBack( new OvrFolderSwipeComponent( *this, folderIndex ) );
	VRMenuObjectParms folderParms(
		VRMENU_CONTAINER,
		comps,
		VRMenuSurfaceParms(),
		( folder.Name + " root" ).ToCStr(),
		Posef(),
		Vector3f( 1.0f ),
		fontParms,
		folderId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION )
		);
	parms.PushBack( &folderParms );
	AddItems( MenuMgr, Font, parms, GetRootHandle(), false ); // PARENT: Root
	parms.Clear();

	// grab the folder handle and make sure it was created
	folder.Handle = HandleForId( MenuMgr, folderId );
	VRMenuObject * folderObject = MenuMgr.ToObject( folder.Handle );
	OVR_UNUSED( folderObject );

	// Add new OvrFolderBrowserSwipeComponent to folder
	VRMenuId_t swipeFolderId( ID_CENTER_ROOT.Get() + folderIndex + 100000 );
	Array< VRMenuComponent* > swipeComps;
	swipeComps.PushBack( new OvrFolderBrowserSwipeComponent( *this, folderIndex ) );
	VRMenuObjectParms swipeParms(
		VRMENU_CONTAINER, 
		swipeComps,
		VRMenuSurfaceParms(), 
		( folder.Name + " swipe" ).ToCStr(),
		Posef(), 
		Vector3f( 1.0f ), 
		fontParms,
		swipeFolderId,
		VRMenuObjectFlags_t( VRMENUOBJECT_NO_GAZE_HILIGHT ), 
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) 
		);
	parms.PushBack( &swipeParms );
	AddItems( MenuMgr, Font, parms, folder.Handle, false ); // PARENT: folder.Handle
	parms.Clear();

	// grab the SwipeHandle handle and make sure it was created
	folder.SwipeHandle = folderObject->ChildHandleForId( MenuMgr, swipeFolderId );
	VRMenuObject * swipeObject = MenuMgr.ToObject( folder.SwipeHandle );
	OVR_ASSERT( swipeObject != NULL );
	
	// build a collision primitive that encompasses all of the panels for a raw (including the empty space between them)
	// so that we can always send our swipe messages to the correct row based on gaze.
	Array< Vector3f > vertices( CircumferencePanelSlots * 2 );
	Array< TriangleIndex > indices( CircumferencePanelSlots * 6 );
	int curIndex = 0;
	int curVertex = 0;
	for ( int i = 0; i < CircumferencePanelSlots; ++i )
	{
		float theta = ( i * Mathf::TwoPi ) / CircumferencePanelSlots;
		float x = cos( theta ) * Radius * 1.05f;
		float z = sin( theta ) * Radius * 1.05f;
		Vector3f topVert( x, PanelHeight * 0.5f, z );
		Vector3f bottomVert( x, PanelHeight * -0.5f, z );

		vertices[curVertex + 0] = topVert;
		vertices[curVertex + 1] = bottomVert;
		if ( i > 0 )	// only set up indices if we already have one column's vertices
		{
			// first tri
			indices[curIndex + 0] = curVertex + 1;
			indices[curIndex + 1] = curVertex + 0;
			indices[curIndex + 2] = curVertex - 1;
			// second tri
			indices[curIndex + 3] = curVertex + 0;
			indices[curIndex + 4] = curVertex - 2;
			indices[curIndex + 5] = curVertex - 1;
			curIndex += 6;
		}

		curVertex += 2;
	}
	// connect the last vertices to the first vertices for the last sector
	indices[curIndex + 0] = 1;
	indices[curIndex + 1] = 0;
	indices[curIndex + 2] = curVertex - 1;
	indices[curIndex + 3] = 0;
	indices[curIndex + 4] = curVertex - 2;
	indices[curIndex + 5] = curVertex - 1;

	// create and set the collision primitive on the swipe object
	OvrTriCollisionPrimitive * cp = new OvrTriCollisionPrimitive( vertices, indices, ContentFlags_t( CONTENT_SOLID ) );
	swipeObject->SetCollisionPrimitive( cp );

	if ( !category.DatumIndicies.IsEmpty() )
	{
		// Grab panel parms 
		LoadFolderPanels( category, folderIndex, folder, parms );
		
		// Add panels to swipehandle
		AddItems( MenuMgr, Font, parms, folder.SwipeHandle, false );
		DeletePointerArray( parms );
		parms.Clear();

		// Assign handles to panels
		for ( int i = 0; i < folder.Panels.GetSizeI(); ++i )
		{
			Panel& panel = folder.Panels.At( i );
			panel.Handle = swipeObject->GetChildHandleForIndex( i );
		}
	}

	// Folder title
	VRMenuId_t folderTitleRootId( ID_CENTER_ROOT.Get() + folderIndex + 1000000 );
	VRMenuObjectParms titleRootParms(
		VRMENU_CONTAINER,
		Array< VRMenuComponent* >(),
		VRMenuSurfaceParms(),
		( folder.Name + " title root" ).ToCStr(),
		Posef(),
		Vector3f( 1.0f ),
		fontParms,
		folderTitleRootId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION )
		);
	parms.PushBack( &titleRootParms );
	AddItems( MenuMgr, Font, parms, folder.Handle, false ); // PARENT: folder.Handle
	parms.Clear();

	// grab the title root handle and make sure it was created
	folder.TitleRootHandle = folderObject->ChildHandleForId( MenuMgr, folderTitleRootId );
	VRMenuObject * folderTitleRootObject = MenuMgr.ToObject( folder.TitleRootHandle );
	OVR_UNUSED( folderTitleRootObject );
	OVR_ASSERT( folderTitleRootObject != NULL );

	VRMenuId_t folderTitleId( ID_CENTER_ROOT.Get() + folderIndex + 1100000 );
	Posef titlePose( Quatf(), -FWD * Radius + UP * PanelHeight * FolderTitleSpacingScale );
	VRMenuObjectParms titleParms( 
		VRMENU_STATIC, 
		Array< VRMenuComponent* >(),
		VRMenuSurfaceParms(),
		"no title",
		titlePose, 
		Vector3f( 1.25f ), 
		fontParms,
		folderTitleId,
		VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ), 
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &titleParms );
	AddItems( MenuMgr, Font, parms, folder.TitleRootHandle, true ); // PARENT: folder.TitleRootHandle
	parms.Clear();

	// grab folder title handle and make sure it was created
	folder.TitleHandle = folderTitleRootObject->ChildHandleForId( MenuMgr, folderTitleId );
	VRMenuObject * folderTitleObject = MenuMgr.ToObject( folder.TitleHandle );
	OVR_UNUSED( folderTitleObject );
	OVR_ASSERT( folderTitleObject != NULL );
	
	// Add folder to local array
	Folders.PushBack( folder );

	UpdateFolderTitle( folderIndex );
	Folders.Back().MaxRotation = CalcFolderMaxRotation( folderIndex );
}

void OvrFolderBrowser::RebuildFolder( const int folderIndex, const Array< OvrMetaDatum * > & data )
{
	if ( folderIndex >= 0 && Folders.GetSizeI() > folderIndex )
	{		
		Folder & folder = Folders.At( folderIndex );
		VRMenuObject * swipeObject = MenuMgr.ToObject( folder.SwipeHandle );
		OVR_ASSERT( swipeObject );

		swipeObject->FreeChildren( MenuMgr );
		folder.Panels.Clear();

		Array< VRMenuObjectParms const * > outParms;
		for ( int panelIndex = 0; panelIndex < data.GetSizeI(); ++panelIndex )
		{
			OvrMetaDatum * panelData = data.At( panelIndex );
			if ( panelData )
			{
				AddPanelToFolder( data.At( panelIndex ), folderIndex, folder, outParms );
				AddItems( MenuMgr, Font, outParms, folder.SwipeHandle, false );
				DeletePointerArray( outParms );
				outParms.Clear();

				// Assign handle to panel
				Panel& panel = folder.Panels.At( panelIndex );
				panel.Handle = swipeObject->GetChildHandleForIndex( panelIndex );
			}
		}
		OvrFolderBrowserSwipeComponent * swipeComp = swipeObject->GetComponentById< OvrFolderBrowserSwipeComponent >();
		OVR_ASSERT( swipeComp );		
		UpdateFolderTitle( folderIndex );

		// Recalculate accumulated rotation in the swipe component based on ratio of where user left off before adding/removing favorites
		const float currentMaxRotation = folder.MaxRotation > 0.0f ? folder.MaxRotation : 1.0f;
		const float positionRatio = Alg::Clamp( swipeComp->GetAccumulatedRotation() / currentMaxRotation, 0.0f, 1.0f );
		folder.MaxRotation = CalcFolderMaxRotation( folderIndex );
		swipeComp->SetAccumulatedRotation( folder.MaxRotation * positionRatio );
	}
}

void OvrFolderBrowser::UpdateFolderTitle( const int folderIndex )
{
	const Folder & folder = Folders.At( folderIndex );
	const int numPanels = folder.Panels.GetSizeI();
	
	String folderTitle = folder.Name;
	
	char buf[ 6 ];
	OVR_itoa( numPanels, buf, 6, 10 );

	folderTitle += " (";
	folderTitle += buf;
	folderTitle += ")";

	VRMenuObject * folderTitleObject = MenuMgr.ToObject( folder.TitleHandle );

	OVR_ASSERT( folderTitleObject != NULL );

	folderTitleObject->SetText( folderTitle );

	VRMenuObjectFlags_t flags = folderTitleObject->GetFlags();
	if ( numPanels > 0 )
	{
		flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
		flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
	}
	else
	{
		flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
		flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
	}

	folderTitleObject->SetFlags( flags );
}

void * OvrFolderBrowser::ThumbnailThread( void * v )
{
	int result = pthread_setname_np( pthread_self(), "FolderBrowser" );
	if ( result != 0 )
	{
		LOG( "FolderBrowser: pthread_setname_np failed %s", strerror( result ) );
	}

	OvrFolderBrowser * folderBrowser = ( OvrFolderBrowser * )v;

	const double start = ovr_GetTimeInSeconds();

	for ( ;; )
	{
		folderBrowser->BackgroundCommands.SleepUntilMessage();
		const char * msg = folderBrowser->BackgroundCommands.GetNextMessage();
		//LOG( "BackgroundCommands: %s", msg );
		if ( MessageQueue::MatchesHead( "load ", msg ) )
		{
			Array<Folder> * folders;
			int folderId;
			int panelId;

			sscanf( msg, "load %p %i %i", &folders, &folderId, &panelId );
			const char * fileName = strstr( msg, ":" ) + 1;

			const String fullPath( fileName );

			int		width;
			int		height;
			unsigned char * data = folderBrowser->LoadThumbAndApplyAA( fullPath, width, height );
			if ( data != NULL )
			{
				folderBrowser->TextureCommands.PostPrintf( "thumb %p %i %i %p %i %i",
					&( *folders ), folderId, panelId, data, width, height );
			}

			free( ( void * )msg );
		}
		else if ( MessageQueue::MatchesHead( "create ", msg ) )
		{
			const String fullPath( msg + strlen( "create " ) );

			int pathLen = fullPath.GetLengthI();
			if ( pathLen > 2 && OVR_stricmp( fullPath.ToCStr() + pathLen - 2, ".x" ) == 0 )
			{
				LOG( "Thumbnails cannot be generated from encrypted images." );
			}
			else
			{
				// First check if we can write to destination
				const String thumbNailDestination = folderBrowser->ThumbName( fullPath );
				FILE * f = fopen( thumbNailDestination.ToCStr(), "wb" );
				if ( f != NULL )
				{
					fclose( f );
					int	width = 0;
					int height = 0;
					unsigned char * data = folderBrowser->CreateThumbnail( fullPath.ToCStr(), width, height );

					// Should we write out a trivial thumbnail if the create failed?
					if ( data != NULL )
					{
						// write it out
						WriteJpeg( thumbNailDestination.ToCStr(), data, width, height );
						free( data );
					}
				}
			}
		}
		else
		{
			LOG( "OvrFolderBrowser::ThumbnailThread received unhandled message: %s", msg );
			OVR_ASSERT( false );
			free( ( void * )msg );
		}
	}

	return NULL;
}

void OvrFolderBrowser::LoadThumbnailToTexture( const char * thumbnailCommand )
{
	Array<Folder> * folders = NULL;
	
	int folderId;
	int panelId;
	unsigned char * data;
	int width;
	int height;

	sscanf( thumbnailCommand, "thumb %p %i %i %p %i %i", &folders, &folderId, &panelId, &data, &width, &height );
	OVR_ASSERT( folders );
	
	Array<Panel> * panels = &folders->At( folderId ).Panels;

	Panel * panel = NULL;

	// find panel using panelId
	const int numPanels = panels->GetSizeI();
	for ( int index = 0; index < numPanels; ++index )
	{
		Panel& currentPanel = panels->At( index );
		if ( currentPanel.Id == panelId )
		{
			panel = &currentPanel;
			break;
		}
	}

	if ( panel == NULL ) // Panel not found as it was moved. Delete data and bail
	{
		free( data );
		return;
	}

	const int max = Alg::Max( width, height );

	// Grab the Panel from VRMenu
	VRMenuObject * panelObject = NULL;
	panelObject = MenuMgr.ToObject( panel->Handle );
	OVR_ASSERT( panelObject );

	panel->Size[ 0 ] *= ( float )width / max;
	panel->Size[ 1 ] *= ( float )height / max;

	GLuint texId = LoadRGBATextureFromMemory(
		data, width, height, true /* srgb */ ).texture;

	OVR_ASSERT( texId );

	panelObject->SetSurfaceTextureTakeOwnership( 0, 0, SURFACE_TEXTURE_DIFFUSE,
		texId, panel->Size[ 0 ], panel->Size[ 1 ] );

	BuildTextureMipmaps( texId );
	MakeTextureTrilinear( texId );
	MakeTextureClamped( texId );

	free( data );
}

void OvrFolderBrowser::LoadFolderPanels( const OvrMetaData::Category & category, const int folderIndex, Folder & folder,
		Array< VRMenuObjectParms const * >& outParms )
{
	// Build panels 
	Array< const OvrMetaDatum * > categoryPanos;
	MetaData.GetMetaData( category, categoryPanos );
	const int numPanos = categoryPanos.GetSizeI();
	LOG( "Building %d panels for %s", numPanos, category.CategoryName.ToCStr() );
	int panelIndex = 0;
	for ( int panoIndex = 0; panoIndex < numPanos; panoIndex++ )
	{
		AddPanelToFolder( const_cast< OvrMetaDatum * const >( categoryPanos.At( panoIndex ) ), folderIndex, folder, outParms );
	}
}

void OvrFolderBrowser::AddPanelToFolder( OvrMetaDatum * const panoData, const int folderIndex, Folder & folder, Array< VRMenuObjectParms const * >& outParms )
{
	// Need data in panel to be non const as it will be modified externally
	OVR_ASSERT( panoData );

	if ( !FileExists( panoData->Url ) )
	{
		return;
	}

	Panel panel;
	panel.Data = panoData;
	const int panelIndex = folder.Panels.GetSizeI();
	panel.Id = panelIndex;
	panel.Size.x = PanelWidth;
	panel.Size.y = PanelHeight;

	String panelTitle = panoData->Title;

#if 0
	const int numChars = panelTitle.GetSize();
	if ( numChars > MAXIMUM_TITLE_SIZE )
	{
		for ( int c = MAXIMUM_TITLE_SIZE - 1; c < numChars; ++c )
		{
			if ( panelTitle[ c ] == ' ' )
			{
				panelTitle.Insert( "\n", c );
				break;
			}
		}
	}
#endif

	// Load a panel
	Array< VRMenuComponent* > panelComps;
	VRMenuId_t id( ID_CENTER_ROOT.Get() + panelIndex + 10000000 );

	int  folderIndexShifted = folderIndex << 24;
	VRMenuId_t buttonId( folderIndexShifted | panel.Id );

	panelComps.PushBack( new OvrButton_OnUp( this, buttonId ) );
	panelComps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.25f, Vector4f( 0.f ) ) );

	static const char * panelSrc = NULL;
	static const char * panelHiSrc = NULL;

	if ( ThumbWidth == ThumbHeight )
	{
		panelSrc = "res/raw/panel_square.tga";
		panelHiSrc = "res/raw/panel_hi_square.tga";
	}
	else
	{
		panelSrc = "res/raw/panel.tga";
		panelHiSrc = "res/raw/panel_hi.tga";
	}

#if 1	// single-pass multitexture
	VRMenuSurfaceParms panelSurfParms( "panelSurface",
		panelSrc, SURFACE_TEXTURE_DIFFUSE,
		panelHiSrc, SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX );
#else	// two-pass
	VRMenuSurfaceParms diffuseParms( "diffuse",
		"res/raw/panel.tga", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
	VRMenuSurfaceParms additiveParms( "additive",
		"res/raw/panel_hi.tga", SURFACE_TEXTURE_ADDITIVE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
	Array< VRMenuSurfaceParms > panelSurfParms;
	panelSurfParms.PushBack( diffuseParms );
	panelSurfParms.PushBack( additiveParms );
#endif

	float const factor = ( float )panelIndex / ( float )CircumferencePanelSlots;

	Quatf rot( DOWN, ( Mathf::TwoPi * factor ) );

	Vector3f dir( -FWD * rot );
	Posef panelPose( rot, dir * Radius );
	Vector3f panelScale( 1.0f );

	const Posef textPose( Quatf(), Vector3f( 0.0f, -PanelHeight * PanelTextSpacingScale, 0.0f ) );

	const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 0.5f );
	VRMenuObjectParms * p = new VRMenuObjectParms( VRMENU_BUTTON, panelComps,
		panelSurfParms, panelTitle, panelPose, panelScale, textPose, Vector3f( 1.0f ), fontParms, id,
		VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	outParms.PushBack( p );

	folder.Panels.PushBack( panel );

	// Create or load thumbnail
	const String	thumbName = ThumbName( panel.Data->Url );
	
#if 0
	// delete all thumbs
	LOG( "Removing thumbnail '%s'", thumbName.ToCStr() );
	if ( remove( thumbName ) != 0 )
	{
		LOG( "Failed to remove thumbnail '%s'", thumbName.ToCStr() );
	}
#elif 0 // enable to re-create all thumbnails
	String	createCmd( "create " );
	createCmd += panelFile;
	BackgroundCommands.PostString( createCmd );

	char	cmd[ 1024 ];
	sprintf( cmd, "load %p %i %i:%s", &Folders, folderIndex, panel.Id, thumbName.ToCStr() );
	BackgroundCommands.PostString( String( cmd ) );
#else

	// For reasons - thumbs might be in assets - check there first
	// If so, load them right away as PackageFile is not thread safe
	String finalThumb;
	
	String assetsThumb = "assets/";
	const String fileBase = ExtractFileBase( thumbName );
	assetsThumb += AlternateThumbName( fileBase );
	finalThumb = assetsThumb;
	if ( ovr_PackageFileExists( finalThumb ) )
	{
		LOG( "Start loading assets thumb '%s'", finalThumb.ToCStr() );
		int		width;
		int		height;
		unsigned char * data = LoadThumbAndApplyAA( finalThumb, width, height );

		if ( data != NULL )
		{
			// The data will be consumed in the next Frame where we have finished building panel vrmenu objects
			// So yes - we are dogfooding ;)
			TextureCommands.PostPrintf( "thumb %p %i %i %p %i %i",
				&Folders, folderIndex, panel.Id, data, width, height );
		}
		return;
	}

	finalThumb = thumbName;
	if ( !FileExists( thumbName ) )
	{
		const String altThumbName = AlternateThumbName( panel.Data->Url );
		if ( !FileExists( altThumbName ) )
		{
			String	cmd( "create " );
			cmd += panel.Data->Url;
			LOG( "Start creating thumb '%s'", panel.Data->Url.ToCStr() );
			BackgroundCommands.PostString( cmd );
		}
		else
		{
			finalThumb = altThumbName;
		}
	}
	LOG( "Start loading thumb '%s'", thumbName.ToCStr() );
	char	cmd[ 1024 ];
	sprintf( cmd, "load %p %i %i:%s", &Folders, folderIndex, panel.Id, finalThumb.ToCStr() );
	//LOG( "Thumb cmd: %s", cmd );
	BackgroundCommands.PostString( String( cmd ) );
#endif
}

unsigned char * OvrFolderBrowser::LoadThumbAndApplyAA( const String & fileName, int & width, int & height )
{
	unsigned char * data = NULL;	
	data = LoadThumbnail( fileName, width, height );
	if ( data != NULL )
	{
		if ( ThumbPanelBG != NULL )
		{
			const unsigned ThumbWidth = GetThumbWidth();
			const unsigned ThumbHeight = GetThumbHeight();

			const int numBytes = width * height * 4;
			const int thumbPanelBytes = ThumbWidth * ThumbHeight * 4;
			if ( numBytes != thumbPanelBytes )
			{
				LOG( "Thumbnail image '%s' is the wrong size! Regenerate thumbnails!", fileName.ToCStr() );
				free( data );
				data = NULL;
			}
			else
			{
				// Apply alpha from vrlib/res/raw to alpha channel for anti-aliasing
				for ( int i = 3; i < thumbPanelBytes; i += 4 )
				{
					data[ i ] = ThumbPanelBG[ i ];
				}
				return data;
			}
		}
	}

	return NULL;
}

const OvrFolderBrowser::Folder& OvrFolderBrowser::GetFolder( int index ) const
{
	return Folders.At( index );
}

OvrFolderBrowser::Folder& OvrFolderBrowser::GetFolder( int index )
{
	return Folders.At( index );
}

bool OvrFolderBrowser::ScrollRoot( const RootDirection direction )
{
	// Are we already scrolling?
	VRMenuObject * root = MenuMgr.ToObject( GetRootHandle() );
	OVR_ASSERT( root );
	OvrFolderBrowserRootComponent * rootComp = root->GetComponentById< OvrFolderBrowserRootComponent >();
	OVR_ASSERT( rootComp );

	// Hide the scroll hint if shown
	if ( !ScrollHintShown )
	{
		SetScrollHintVisible( false );
		ScrollHintShown = true;
	}

	return rootComp->Scroll( root, direction );
}

bool OvrFolderBrowser::RotateCategory( const CategoryDirection direction )
{
	const Folder& folder = GetFolder( GetActiveFolderIndex() );
	VRMenuObject * swipe = MenuMgr.ToObject( folder.SwipeHandle );
	OVR_ASSERT( swipe );

	OvrFolderBrowserSwipeComponent * swipeComp = swipe->GetComponentById< OvrFolderBrowserSwipeComponent >();
	OVR_ASSERT( swipeComp );
	
	return swipeComp->Rotate( direction );
}

void OvrFolderBrowser::SetCategoryRotation( const int folderIndex, const int panelIndex )
{
	OVR_ASSERT( folderIndex >= 0 && folderIndex < Folders.GetSizeI() );
	const Folder& folder = GetFolder( folderIndex );
	VRMenuObject * swipe = MenuMgr.ToObject( folder.SwipeHandle );
	OVR_ASSERT( swipe );

	OvrFolderBrowserSwipeComponent * swipeComp = swipe->GetComponentById< OvrFolderBrowserSwipeComponent >();
	OVR_ASSERT( swipeComp );

	swipeComp->SetRotationByIndex( panelIndex );
}

void OvrFolderBrowser::OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event )
{
	if ( AllowPanelTouchUp )
	{
		const int id = itemId.Get();
		const int folderIndex = ( id >> 24 ) & 0x0000007F;
		const int panelIndex = id & 0x00FFFFFF;

		OVR_ASSERT( folderIndex >= 0 && folderIndex < Folders.GetSizeI() );
		const Folder& folder = GetFolder( folderIndex );
		OVR_ASSERT( panelIndex >= 0 && panelIndex < folder.Panels.GetSizeI() );
		const Panel& panel = folder.Panels.At( panelIndex );

		CurrentPanelData = panel.Data;

		// Hide scroll hint
		if ( !ScrollHintShown )
		{
			SetScrollHintVisible( false );
			ScrollHintShown = true;
		}
	}
}

OvrMetaDatum	* OvrFolderBrowser::QueryActivePanel()
{
	OvrMetaDatum * action = CurrentPanelData;
	CurrentPanelData = NULL;
	return action;
}

OvrMetaDatum * OvrFolderBrowser::NextFileInDirectory( const int step )
{
	const Folder & folder = Folders.At( GetActiveFolderIndex() );

	VRMenuObject * swipeObject = MenuMgr.ToObject( folder.SwipeHandle );
	OVR_ASSERT( swipeObject );

	OvrFolderBrowserSwipeComponent * swipeComp = swipeObject->GetComponentById<OvrFolderBrowserSwipeComponent>();
	OVR_ASSERT( swipeComp );

	return swipeComp->NextPanelData( step );
}

float OvrFolderBrowser::CalcFolderMaxRotation( const int folderIndex ) const
{
	const Folder & folder = Folders.At( folderIndex );
	int numPanels = Alg::Clamp( folder.Panels.GetSizeI() - 1, 0, INT_MAX );
	return static_cast< float >( numPanels );
}

void OvrFolderBrowser::SetScrollHintVisible( const bool visible )
{
	if ( VRMenuObject * swipeHintObject = MenuMgr.ToObject( ScrollHintHandle ) )
	{
		VRMenuObjectFlags_t flags = swipeHintObject->GetFlags();
		if ( visible )
		{
			flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
		}
		else
		{
			flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
		}
		swipeHintObject->SetFlags( flags );
	}
}

bool OvrFolderBrowser::GazingAtMenu() const
{
	if ( GetFocusedHandle().IsValid() )
	{
		const Matrix4f & view = AppPtr->GetLastViewMatrix();
		Vector3f viewForwardFlat( view.M[ 2 ][ 0 ], 0.0f, view.M[ 2 ][ 2 ] );
		viewForwardFlat.Normalize();

		static const float cosHalf = cos( VisiblePanelsArcAngle );
		const float dot = viewForwardFlat.Dot( FWD * GetMenuPose().Orientation );
		
		if ( dot >= cosHalf )
		{
			return true;
		}
	}
	return false;
}

void OvrFolderBrowser::GetFolderData( const int folderIndex, Array< OvrMetaDatum * >& data )
{
	if ( Folders.GetSizeI() > 0 && folderIndex >= 0 && folderIndex < Folders.GetSizeI() )
	{
		const Folder & folder = Folders.At( folderIndex );
		const Array<Panel> & panels = folder.Panels;
		for ( int panelIndex = 0; panelIndex < panels.GetSizeI(); ++panelIndex )
		{
			OvrMetaDatum * panelData = panels.At( panelIndex ).Data;
			OVR_ASSERT( panelData );
			data.PushBack( panelData );
		}
	}
}

int OvrFolderBrowser::GetActiveFolderIndex() const
{
	VRMenuObject * rootObject = MenuMgr.ToObject( GetRootHandle() );
	OVR_ASSERT( rootObject );

	OvrFolderBrowserRootComponent * rootComp = rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
	OVR_ASSERT( rootComp );

	return rootComp->GetCurrentIndex( rootObject );
}

} // namespace OVR
