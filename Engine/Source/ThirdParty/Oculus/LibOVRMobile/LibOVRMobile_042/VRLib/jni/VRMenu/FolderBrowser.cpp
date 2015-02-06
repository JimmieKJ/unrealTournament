/************************************************************************************

Filename    :   FolderBrowser.cpp
Content     :   A menu for browsing a hierarchy of folders with items represented by thumbnails.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "FolderBrowser.h"

#include "../LibOVR/Src/Kernel/OVR_Threads.h"
#include <stdio.h>
#include <unistd.h>		
#include "../App.h"
#include "VRMenuComponent.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "DefaultComponent.h"
#include "../VrCommon.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_String_Utils.h"
#include "3rdParty/stb/stb_image.h"
#include "PackageFiles.h"
#include "OVR_JSON.h"
#include "AnimComponents.h"
#include "linux/stat.h"
#include "VrCommon.h"
#include "VrApi/VrLocale.h"
#include "ScrollManager.h"
#include "VRMenuObject.h"

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

// helps avoiding copy pasting code for updating wrap around indicator effect
void UpdateWrapAroundIndicator(const OvrScrollManager & ScrollMgr, const OvrVRMenuMgr & menuMgr, VRMenuObject * wrapIndicatorObject, const Vector3f initialPosition, const float initialAngle)
{
	static const float WRAP_INDICATOR_INITIAL_SCALE = 0.8f;

	float fadeValue;
	float rotation = 0.0f;
	float rotationDirection;
	Vector3f position = initialPosition;
	if( ScrollMgr.GetPosition() < 0.0f )
	{
		fadeValue = -ScrollMgr.GetPosition();
		rotationDirection = -1.0f;
	}
	else
	{
		fadeValue = ScrollMgr.GetPosition() - ScrollMgr.GetMaxPosition();
		rotationDirection = 1.0f;
	}

	fadeValue = Alg::Clamp( fadeValue, 0.0f, ScrollMgr.GetWrapAroundScrollOffset() ) / ScrollMgr.GetWrapAroundScrollOffset();
	rotation += fadeValue * rotationDirection * Mathf::Pi;
	float scaleValue = 0.0f;
	if( ScrollMgr.GetIsWrapAroundTimeInitiated() )
	{
		if( ScrollMgr.GetRemainingTimeForWrapAround() >= 0.0f )
		{
			float timeSpentPercent = LinearRangeMapFloat( ScrollMgr.GetRemainingTimeForWrapAround(), 0.0f, ScrollMgr.GetWrapAroundHoldTime(), 100, 0.0f );
			// Time Spent % :  0% ---  70% --- 90% --- 100%
			//      Scale % :  0% --- 300% --- 80% --- 100%  <= Gives popping effect
			static const float POP_EFFECT[4][2] =
			{
					{ 0		, 0.0f },
					{ 70	, 3.0f },
					{ 80	, 0.8f },
					{ 100	, 1.0f },
			};

			if( timeSpentPercent <= 70 )
			{
				scaleValue = LinearRangeMapFloat( timeSpentPercent, POP_EFFECT[0][0], POP_EFFECT[1][0], POP_EFFECT[0][1], POP_EFFECT[1][1] );
			}
			else if( timeSpentPercent <= 90 )
			{
				scaleValue = LinearRangeMapFloat( timeSpentPercent, POP_EFFECT[1][0], POP_EFFECT[2][0], POP_EFFECT[1][1], POP_EFFECT[2][1] );
			}
			else
			{
				scaleValue = LinearRangeMapFloat( timeSpentPercent, POP_EFFECT[2][0], POP_EFFECT[3][0], POP_EFFECT[2][1], POP_EFFECT[3][1] );
			}
		}
		else
		{
			scaleValue = 1.0f;
		}
	}

	float finalScale = WRAP_INDICATOR_INITIAL_SCALE + ( 1.0f - WRAP_INDICATOR_INITIAL_SCALE ) * scaleValue;
	rotation += initialAngle;
	Vector4f col = wrapIndicatorObject->GetTextColor();
	col.w = fadeValue;
	Quatf rot( FWD, rotation );
	rot.Normalize();

	wrapIndicatorObject->SetLocalRotation( rot );
	wrapIndicatorObject->SetLocalPosition( position );
	wrapIndicatorObject->SetColor( col );
	wrapIndicatorObject->SetLocalScale( Vector3f( finalScale, finalScale, finalScale ) );
}

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
		, ScrollMgr( VERTICAL_SCROLL )
		, TotalTouchDistance( 0.0f )
	{
	}

	virtual int		GetTypeId() const
	{ 
		return TYPE_ID;
	}

	int GetCurrentIndex( VRMenuObject * self ) const
	{
		// First calculating index of a folder with in valid folders(folder that has atleast one panel) array based on its position
		const int validNumFolders = GetFolderBrowserValidFolderCount();
		const float deltaHeight = FolderBrowser.GetPanelHeight();
		const float maxHeight = deltaHeight * validNumFolders;
		const float positionRatio = self->GetLocalPosition().y / maxHeight;
		int idx = nearbyintf( positionRatio * validNumFolders );
		idx = Alg::Clamp( idx, 0, FolderBrowser.GetNumFolders() - 1 );

		// Remapping index with in valid folders to index in entire Folder array
		const int numValidFolders = GetFolderBrowserValidFolderCount();
		int counter = 0;
		int remapedIdx = 0;
		for( ; remapedIdx < numValidFolders; ++remapedIdx )
		{
			if( !FolderBrowser.GetFolder( remapedIdx ).Panels.IsEmpty() )
			{
				if( counter == idx )
				{
					break;
				}
				++counter;
			}
		}

		return Alg::Clamp( remapedIdx, 0, FolderBrowser.GetNumFolders() - 1 );
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
		const int folderCount = FolderBrowser.GetNumFolders();
		int validFoldersCount = 0;
		// Hides invalid folders(folder that doesn't have atleast one panel), and rearranges valid folders positions to avoid gaps between folders
		for( int i = 0; i < folderCount; ++i )
		{
			const OvrFolderBrowser::Folder & currentFolder = FolderBrowser.GetFolder( i );
			VRMenuObject * folderObject = menuMgr.ToObject( currentFolder.Handle );
			OVR_ASSERT( folderObject != NULL );
			if ( currentFolder.Panels.GetSizeI() > 0 )
			{
				folderObject->SetVisible( true );
				folderObject->SetLocalPosition( ( DOWN * FolderBrowser.GetPanelHeight() * validFoldersCount ) );
				++validFoldersCount;
			}
			else
			{
				folderObject->SetVisible( false );
			}
		}

		ScrollMgr.SetMaxPosition( validFoldersCount - 1 );
		if( folderCount > 1 ) // Need at least one folder in order to enable vertical scrolling
		{
			bool forwardScrolling = vrFrame.Input.buttonState & ( BUTTON_LSTICK_DOWN | BUTTON_DPAD_DOWN );
			bool backwardScrolling = vrFrame.Input.buttonState & ( BUTTON_LSTICK_UP | BUTTON_DPAD_UP );
			if( forwardScrolling || backwardScrolling )
			{
				FolderBrowser.SetScrollHintVisible( false );
			}
			ScrollMgr.SetEnableAutoForwardScrolling( forwardScrolling );
			ScrollMgr.SetEnableAutoBackwardScrolling( backwardScrolling );
		}
		ScrollMgr.Frame( vrFrame.DeltaSeconds );

		const Vector3f & rootPosition = self->GetLocalPosition();
		self->SetLocalPosition( Vector3f( rootPosition.x, FolderBrowser.GetPanelHeight() * ScrollMgr.GetPosition(), rootPosition.z ) );

		const float alphaSpace = FolderBrowser.GetPanelHeight() * 2.0f;
		const float rootOffset = rootPosition.y - EYE_HEIGHT_OFFSET;

		// Fade + hide each category/folder based on distance from origin
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
				const float targetZ = ACTIVE_DEPTH_OFFSET * finalAlpha;
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

		// Updating folder wrap around indicator logic/effect
		VRMenuObject * wrapIndicatorObject = menuMgr.ToObject( FoldersWrapHandle );
		if (wrapIndicatorObject != NULL)
		{
			if (ScrollMgr.IsScrolling() && ScrollMgr.IsOutOfBounds())
			{
				wrapIndicatorObject->SetVisible(true);
				bool scrollingAtStart = (ScrollMgr.GetPosition() < 0.0f);
				if (scrollingAtStart)
				{
					UpdateWrapAroundIndicator(ScrollMgr, menuMgr, wrapIndicatorObject, FoldersWrapHandleTopPosition, -Mathf::PiOver2);
				}
				else
				{
					FoldersWrapHandleBottomPosition.y = -FolderBrowser.GetPanelHeight() * validFoldersCount;
					UpdateWrapAroundIndicator(ScrollMgr, menuMgr, wrapIndicatorObject, FoldersWrapHandleBottomPosition, Mathf::PiOver2);
				}

				Vector3f position = wrapIndicatorObject->GetLocalPosition();
				float ratio;
				if (scrollingAtStart)
				{
					ratio = LinearRangeMapFloat(ScrollMgr.GetPosition(), -1.0f, 0.0f, 0.0f, 1.0f);
				}
				else
				{
					ratio = LinearRangeMapFloat(ScrollMgr.GetPosition(), validFoldersCount - 1.0f, validFoldersCount, 1.0f, 0.0f);
				}
				float finalAlpha = -(ratio * ratio) + 1.0f;

				position.z += finalAlpha;
				wrapIndicatorObject->SetLocalPosition(position);
			}
			else
			{
				wrapIndicatorObject->SetVisible(false);
			}
		}

		return  MSG_STATUS_ALIVE;
    }

	eMsgStatus TouchDown( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}

		const int folderCount = FolderBrowser.GetNumFolders();
		if (folderCount > 1)
		{
			ScrollMgr.TouchDown();
		}

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

		const int folderCount = FolderBrowser.GetNumFolders();
		if (folderCount > 1)
		{
			ScrollMgr.TouchUp();
		}

		bool allowPanelTouchUp = false;
		if ( TotalTouchDistance < MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ )
		{
			allowPanelTouchUp = true;
		}

		FolderBrowser.SetAllowPanelTouchUp( allowPanelTouchUp );

		return MSG_STATUS_ALIVE;
	}

	eMsgStatus TouchRelative( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}


		Vector2f currentTouchPosition( event.FloatValue.x, event.FloatValue.y );
		TotalTouchDistance += ( currentTouchPosition - StartTouchPosition ).LengthSq();
		const int folderCount = FolderBrowser.GetNumFolders();
		if (folderCount > 1)
		{
			if (TotalTouchDistance < MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ)
			{
				FolderBrowser.SetScrollHintVisible(false);
			}
			ScrollMgr.TouchRelative(event.FloatValue);
		}

		return MSG_STATUS_ALIVE;
	}

	eMsgStatus OnOpening( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		return MSG_STATUS_ALIVE;
	}

	// Return valid folder count, folder that has atleast one panel is considered as valid folder.
	int GetFolderBrowserValidFolderCount() const
	{
		int validFoldersCount = 0;
		const int folderCount = FolderBrowser.GetNumFolders();
		for( int i = 0; i < folderCount; ++i )
		{
			const OvrFolderBrowser::Folder & currentFolder = FolderBrowser.GetFolder( i );
			if ( currentFolder.Panels.GetSizeI() > 0 )
			{
				++validFoldersCount;
			}
		}
		return validFoldersCount;
	}

	OvrFolderBrowser &	FolderBrowser;
	OvrScrollManager	ScrollMgr;
	Vector2f			StartTouchPosition;
	float				TotalTouchDistance;

	menuHandle_t		FoldersWrapHandle;
	Vector3f			FoldersWrapHandleTopPosition;
	Vector3f			FoldersWrapHandleBottomPosition;

	const static float ACTIVE_DEPTH_OFFSET = 3.4f;

public:
	void SetFoldersWrapHandle( menuHandle_t handle ) { FoldersWrapHandle = handle; }
	void SetFoldersWrapHandleTopPosition( Vector3f position ) { FoldersWrapHandleTopPosition = position; }
	void SetFoldersWrapHandleBottomPosition( Vector3f position ) { FoldersWrapHandleBottomPosition = position; }
};

//==============================================================
// OvrFolderBrowserFolderRootComponent
class OvrFolderSwipeComponent : public VRMenuComponent
{
public:
	OvrFolderSwipeComponent( OvrFolderBrowser & folderBrowser, OvrFolderBrowser::Folder * folder )
		 : VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE )  )
		, FolderBrowser( folderBrowser )
		, FolderPtr( folder )
	{
		OVR_ASSERT( FolderPtr );
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
		if ( &FolderBrowser.GetFolder( FolderBrowser.GetActiveFolderIndex() ) != FolderPtr )
		{
			flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
		}
		else
		{
			flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
		}
		self->SetFlags( flags );

    	return MSG_STATUS_ALIVE;
    }

	OvrFolderBrowser &	FolderBrowser;
	OvrFolderBrowser::Folder * FolderPtr;
};

//==============================================================
// OvrFolderBrowserSwipeComponent
class OvrFolderBrowserSwipeComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 58524;

	OvrFolderBrowserSwipeComponent( OvrFolderBrowser & folderBrowser, OvrFolderBrowser::Folder * folder )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_TOUCH_RELATIVE | VRMENU_EVENT_TOUCH_DOWN | VRMENU_EVENT_TOUCH_UP )
		, FolderBrowser( folderBrowser )
		, ScrollMgr( HORIZONTAL_SCROLL )
		, FolderPtr( folder )
    {
		OVR_ASSERT( FolderBrowser.GetCircumferencePanelSlots() > 0 );
		ScrollMgr.SetScrollPadding(0.5f);
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
			ScrollMgr.SetVelocity( MAX_SPEED );
			return true;
		case OvrFolderBrowser::MOVE_PANELS_RIGHT:
			ScrollMgr.SetVelocity( -MAX_SPEED );
			return true;
		default:
			return false;
		}
	}

	void SetRotationByRatio( const float ratio )
	{
		OVR_ASSERT( ratio >= 0.0f && ratio <= 1.0f );
		ScrollMgr.SetPosition( FolderPtr->MaxRotation * ratio );
		ScrollMgr.SetVelocity( 0.0f );
	}

	void SetRotationByIndex( const int panelIndex )
	{
		OVR_ASSERT( panelIndex >= 0 && panelIndex < ( *FolderPtr ).Panels.GetSizeI() );
		ScrollMgr.SetPosition( static_cast< float >( panelIndex ) );
	}

	void SetAccumulatedRotation( const float rot )	
	{ 
		ScrollMgr.SetPosition( rot );
		ScrollMgr.SetVelocity( 0.0f );
	}

	float GetAccumulatedRotation() const { return ScrollMgr.GetPosition();  }

	int CurrentPanelIndex() const { return static_cast< int >( ScrollMgr.GetPosition() ); }

private:
    virtual eMsgStatus Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
    {
    	bool const isActiveFolder = ( FolderPtr == &FolderBrowser.GetFolder( FolderBrowser.GetActiveFolderIndex() ) );
		OvrFolderBrowser::Folder & folder = *FolderPtr;
		ScrollMgr.SetMaxPosition( static_cast<float>( folder.Panels.GetSizeI() - 1 ) );
		if( isActiveFolder )
		{
			ScrollMgr.SetEnableAutoForwardScrolling( vrFrame.Input.buttonState & ( BUTTON_LSTICK_RIGHT | BUTTON_DPAD_RIGHT ) );
			ScrollMgr.SetEnableAutoBackwardScrolling( vrFrame.Input.buttonState & ( BUTTON_LSTICK_LEFT | BUTTON_DPAD_LEFT ) );
		}
		else
		{
			ScrollMgr.SetTouchIsDown( false );
			ScrollMgr.SetEnableAutoForwardScrolling( false );
			ScrollMgr.SetEnableAutoBackwardScrolling( false );
		}
		ScrollMgr.Frame( vrFrame.DeltaSeconds );

    	if( folder.Panels.GetSizeI() <= 0 )
    	{
    		return MSG_STATUS_ALIVE;
    	}

		if (folder.Panels.GetSizeI() > 1) // Make sure has atleast 1 panel to show wrap around indicators.
		{
			VRMenuObject * wrapIndicatorObject = menuMgr.ToObject(folder.WrapIndicatorHandle);
			if (ScrollMgr.IsScrolling() && ScrollMgr.IsOutOfBounds())
			{
				static const float WRAP_INDICATOR_X_OFFSET = 0.2f;
				wrapIndicatorObject->SetVisible(true);
				bool scrollingAtStart = (ScrollMgr.GetPosition() < 0.0f);
				Vector3f position = wrapIndicatorObject->GetLocalPosition();
				if (scrollingAtStart)
				{
					position.x = -WRAP_INDICATOR_X_OFFSET;
					UpdateWrapAroundIndicator(ScrollMgr, menuMgr, wrapIndicatorObject, position, 0.0f);
				}
				else
				{
					position.x = WRAP_INDICATOR_X_OFFSET;
					UpdateWrapAroundIndicator(ScrollMgr, menuMgr, wrapIndicatorObject, position, Mathf::Pi);
				}
			}
			else
			{
				wrapIndicatorObject->SetVisible(false);
			}
		}

    	float const curRot = ScrollMgr.GetPosition() * ( Mathf::TwoPi / FolderBrowser.GetCircumferencePanelSlots() );
		Quatf rot( UP, curRot );
		rot.Normalize();
		self->SetLocalRotation( rot );

		// show or hide panels based on current position
		const int numPanels = folder.Panels.GetSizeI();
		// for rendering, we want to switch to occur between panels - hence nearbyint
		const int curPanelIndex = nearbyintf( ScrollMgr.GetPosition() );
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

    eMsgStatus OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
    {
    	switch( event.EventType )
    	{
    		case VRMENU_EVENT_FRAME_UPDATE:
    			return Frame( app, vrFrame, menuMgr, self, event );
    		case VRMENU_EVENT_TOUCH_DOWN:
    			ScrollMgr.TouchDown();
    			break;
    		case VRMENU_EVENT_TOUCH_UP:
    			ScrollMgr.TouchUp();
    			break;
    		case VRMENU_EVENT_TOUCH_RELATIVE:
    			ScrollMgr.TouchRelative( event.FloatValue );
    			break;
    		default:
    			OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
    	}

    	return MSG_STATUS_ALIVE;
    }

private:
	OvrFolderBrowser&			FolderBrowser;
	OvrScrollManager			ScrollMgr;
	OvrFolderBrowser::Folder *	FolderPtr;			// Correlates the folder browser component to the folder it belongs to
};

//==============================
// OvrMetaData

const char * OvrMetaData::CATEGORIES = "Categories";
const char * OvrMetaData::DATA = "Data";
const char * OvrMetaData::TITLE = "Title";
const char * OvrMetaData::URL = "Url";
const char * OvrMetaData::FAVORITES_TAG = "Favorites";
const char * OvrMetaData::TAG = "tag";
const char * OvrMetaData::LABEL = "label";
const char * OvrMetaData::TAGS = "tags";
const char * OvrMetaData::CATEGORY = "category";
const char * OvrMetaData::URL_INNER = "url";
const char * OvrMetaData::TITLE_INNER = "title";
const char * OvrMetaData::AUTHOR_INNER = "author";

void OvrMetaData::InitFromDirectory( const char * relativePath, const Array< String > & searchPaths, const OvrMetaDataFileExtensions & fileExtensions )
{
	LOG( "OvrMetaData::InitFromDirectory( %s )", relativePath );

	// Find all the files - checks all search paths
	StringHash< String > uniqueFileList = RelativeDirectoryFileList( searchPaths, relativePath );
	Array<String> fileList;
	for ( StringHash< String >::ConstIterator iter = uniqueFileList.Begin(); iter != uniqueFileList.End(); ++iter )
	{
		fileList.PushBack( iter->First );
	}
	SortStringArray( fileList );

	Category currentCategory;
	currentCategory.CategoryTag = ExtractFileBase( relativePath );
	// The label is the same as the tag by default. 
	//Will be replaced if definition found in loaded metadata
	currentCategory.Label = currentCategory.CategoryTag;

	LOG( "OvrMetaData start category: %s", currentCategory.CategoryTag.ToCStr() );

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
		OvrMetaDatum data;
		data.Id = dataIndex;
		data.Title = fileBase;
		data.Author = "Unspecified Author";
		data.Tags.PushBack( currentCategory.CategoryTag );
		if ( GetFullPath( searchPaths, s, data.Url ) )
		{
			StringHash< int >::ConstIterator iter = UrlToIndex.FindCaseInsensitive( data.Url );
			if ( iter == UrlToIndex.End() )
			{
				UrlToIndex.Add( data.Url, dataIndex );
			}
			else
			{
				LOG( "OvrMetaData::InitFromDirectory found duplicate url %s", data.Url.ToCStr() );
			}

			MetaData.PushBack( data );

			//LOG( "OvrMetaData adding datum %s with index %d to %s", data.Title.ToCStr(), dataIndex, currentCategory.CategoryTag.ToCStr() );

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
		InitFromDirectory( subDir.ToCStr(), searchPaths, fileExtensions );
	}
}

void OvrMetaData::InitFromFileList( const Array< String > & fileList, const OvrMetaDataFileExtensions & fileExtensions )
{
	// Create unique categories
	StringHash< int > uniqueCategoryList;
	for ( int i = 0; i < fileList.GetSizeI(); ++i )
	{
		const String & filePath = fileList.At( i );
		const String categoryTag = ExtractDirectory( fileList.At( i ) );
		StringHash< int >::ConstIterator iter = uniqueCategoryList.Find( categoryTag );
		int catIndex = -1;
		if ( iter == uniqueCategoryList.End() )
		{
			LOG( " category: %s", categoryTag.ToCStr() );
			Category cat;
			cat.CategoryTag = categoryTag;
			// The label is the same as the tag by default. 
			//Will be replaced if definition found in loaded metadata
			cat.Label = cat.CategoryTag;
			catIndex = Categories.GetSizeI();
			Categories.PushBack( cat );
			uniqueCategoryList.Add( categoryTag, catIndex );
		}
		else
		{
			catIndex = iter->Second;
		}
		
		OVR_ASSERT( catIndex > -1 );
		Category & currentCategory = Categories.At( catIndex );

		// See if we want this loose-file
		if ( !ShouldAddFile( filePath.ToCStr(), fileExtensions ) )
		{
			continue;
		}

		// Add loose file
		const int dataIndex = MetaData.GetSizeI();
		OvrMetaDatum data;
		data.Id = dataIndex;
		data.Url = filePath;
		data.Title = ExtractFileBase( filePath );
		data.Author = "Unspecified Author";
		data.Tags.PushBack( currentCategory.CategoryTag );

		MetaData.PushBack( data );

		//LOG( "OvrMetaData::InitFromFileList adding datum %s with index %d to %s", data.Url.ToCStr(), dataIndex, currentCategory.CategoryTag.ToCStr() );

		// Register with category
		currentCategory.DatumIndicies.PushBack( dataIndex );
	}
}

void OvrMetaData::RenameCategory( const char * currentTag, const char * newName )
{
	for ( int i = 0; i < Categories.GetSizeI(); ++i )
	{
		Category & cat = Categories.At( i );
		if ( cat.CategoryTag == currentTag )
		{
			cat.Label = newName;
			break;
		}
	}
}

JSON * LoadPackageMetaFile( const char * metaFile ) 
{
	int bufferLength = 0;
	void * 	buffer = NULL;
	String assetsMetaFile = "assets/";
	assetsMetaFile += metaFile;
	ovr_ReadFileFromApplicationPackage( assetsMetaFile.ToCStr(), bufferLength, buffer );
	if ( !buffer )
	{
		LOG( "LoadPackageMetaFile failed to read %s", assetsMetaFile.ToCStr() );
	}
	return JSON::Parse( ( const char * )buffer );
}

JSON * OvrMetaData::CreateOrGetStoredMetaFile( const char * appFileStoragePath, const char * metaFile )
{
	FilePath = appFileStoragePath;
	FilePath += metaFile;

	LOG( "CreateOrGetStoredMetaFile FilePath: %s", FilePath.ToCStr() );

	JSON * dataFile = JSON::Load( FilePath );
	if ( dataFile == NULL )
	{
		// If this is the first run, or we had an error loading the file, we copy the meta file from assets to app's cache
		WriteMetaFile( metaFile );

		// try loading it again
		dataFile = JSON::Load( FilePath );
		if ( dataFile == NULL )
		{
			LOG( "OvrMetaData failed to load JSON meta file: %s", metaFile );
			// Create a Favorites folder since there isn't meta data
			Category favorites;
			favorites.CategoryTag = FAVORITES_TAG;
			Categories.PushBack( favorites );
		}
	}
	else
	{
		LOG( "OvrMetaData::CreateOrGetStoredMetaFile found %s", FilePath.ToCStr() );
	}
	return dataFile;
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
			int writtenCount = fwrite( buffer, 1, bufferLength, newMetaFile );
			if ( writtenCount != bufferLength )
			{
				FAIL( "OvrMetaData::WriteMetaFile failed to write %s", metaFile );
			}
			free( buffer );
		}
		fclose( newMetaFile );
	}
	else
	{
		FAIL( "OvrMetaData failed to create %s - check app permissions", FilePath.ToCStr() );
	}
}

void OvrMetaData::InitFromDirectoryMergeMeta( const char * relativePath, const Array< String > & searchPaths, 
	const OvrMetaDataFileExtensions & fileExtensions, const char * metaFile, const char * packageName )
{
	LOG( "OvrMetaData::InitFromDirectoryMergeMeta" );

	String appFileStoragePath = "/data/data/";
	appFileStoragePath += packageName;
	appFileStoragePath += "/files/";

	FilePath = appFileStoragePath + metaFile;

	OVR_ASSERT( HasPermission( FilePath, R_OK ) );

	JSON * dataFile = CreateOrGetStoredMetaFile( appFileStoragePath, metaFile );
	
	InitFromDirectory( relativePath, searchPaths, fileExtensions );
	ProcessMetaData( dataFile, searchPaths, metaFile );
}

void OvrMetaData::InitFromFileListMergeMeta( const Array< String > & fileList, const Array< String > & searchPaths,
	const OvrMetaDataFileExtensions & fileExtensions, const char * appFileStoragePath, const char * metaFile )
{
	LOG( "OvrMetaData::InitFromFileListMergeMeta" );

	JSON * dataFile = CreateOrGetStoredMetaFile( appFileStoragePath, metaFile );
	OVR_ASSERT( dataFile );
	
	InitFromFileList( fileList, fileExtensions );
	ProcessMetaData( dataFile, searchPaths, metaFile );
}

void OvrMetaData::ProcessMetaData( JSON * dataFile, const Array< String > & searchPaths, const char * metaFile )
{
	if ( dataFile != NULL )
	{
		Array< Category > storedCategories;
		StringHash< OvrMetaDatum > storedMetaData;
		ExtractCategories( dataFile, storedCategories );

		// Read in package data first
		JSON * packageMeta = LoadPackageMetaFile( metaFile );
		if ( packageMeta )
		{
			ExtractCategories( packageMeta, storedCategories );
			ExtractMetaData( packageMeta, searchPaths, storedMetaData );
			packageMeta->Release();
		}
		else
		{
			LOG( "ProcessMetaData LoadPackageMetaFile failed for %s", metaFile );
		}

		// Read in the stored data - overriding any found in the package
		ExtractMetaData( dataFile, searchPaths, storedMetaData );

		// Reconcile the stored data vs the data read in
		ReconcileCategories( storedCategories );
		ReconcileMetaData( storedMetaData );

		// Recreate indices which may have changed after reconciliation
		RegenerateCategoryIndices();

		// Delete any newly empty categories except Favorites 
		Array< Category > finalCategories;
		finalCategories.PushBack( Categories.At( 0 ) );
		for ( int catIndex = 1; catIndex < Categories.GetSizeI(); ++catIndex )
		{
			Category & cat = Categories.At( catIndex );
			if ( !cat.DatumIndicies.IsEmpty() )
			{
				finalCategories.PushBack( cat );
			}
		}
		Alg::Swap( finalCategories, Categories );
		dataFile->Release();
	}
	else
	{
		LOG( "OvrMetaData::ProcessMetaData NULL dataFile" );
	}

	// Rewrite new data
	dataFile = MetaDataToJson();
	if ( dataFile == NULL )
	{
		FAIL( "OvrMetaData::ProcessMetaData failed to generate JSON meta file" );
	}

	dataFile->Save( FilePath );

	LOG( "OvrMetaData::ProcessMetaData created %s", FilePath.ToCStr() );
	dataFile->Release();
}

void OvrMetaData::ReconcileMetaData( StringHash< OvrMetaDatum > & storedMetaData )
{
	// Fix the read in meta data using the stored
	for ( int i = 0; i < MetaData.GetSizeI(); ++i )
	{
		OvrMetaDatum & metaDatum = MetaData.At( i );

		StringHash< OvrMetaDatum >::Iterator iter = storedMetaData.FindCaseInsensitive( metaDatum.Url );

		if ( iter != storedMetaData.End() )
		{
			OvrMetaDatum & storedDatum = iter->Second;
			//LOG( "ReconcileMetaData metadata for %s", storedDatum.Url.ToCStr() );
			Alg::Swap( storedDatum.Tags, metaDatum.Tags );
			Alg::Swap( storedDatum.Title, metaDatum.Title );
			Alg::Swap( storedDatum.Author, metaDatum.Author );
		}
		else
		{
			LOG( "ReconcileMetaData NO metadata %s", metaDatum.Url.ToCStr() );
		}
	}	
}

void OvrMetaData::ReconcileCategories( Array< Category > & storedCategories )
{
	// Reconcile categories
	// We want Favorites always at the top
	// Followed by user created categories
	// Finally we want to maintain the order of the retail categories (defined in assets/meta.json)
	Array< Category > finalCategories;

	Category favorites = storedCategories.At( 0 );
	if ( favorites.CategoryTag != FAVORITES_TAG )
	{
		LOG( "OvrMetaData::ReconcileCategories failed to find expected category order -- missing assets/meta.json?" );
	}

	finalCategories.PushBack( favorites );

	StringHash< bool > StoredCategoryMap; // using as set
	for ( int i = 0; i < storedCategories.GetSizeI(); ++i )
	{
		const Category & storedCategory = storedCategories.At( i );
		StoredCategoryMap.Add( storedCategory.CategoryTag, true );
	}

	// Now add the read in categories if they differ
	for ( int i = 0; i < Categories.GetSizeI(); ++i )
	{
		const Category & readInCategory = Categories.At( i );
		StringHash< bool >::ConstIterator iter = StoredCategoryMap.Find( readInCategory.CategoryTag );

		if ( iter == StoredCategoryMap.End() )
		{
			finalCategories.PushBack( readInCategory );
		}
	}

	// Finally fill in the stored in categories after user made ones
	for ( int i = 1; i < storedCategories.GetSizeI(); ++i )
	{
		finalCategories.PushBack( storedCategories.At( i ) );
	}

	// Now replace Categories
	Alg::Swap( Categories, finalCategories );
}

void OvrMetaData::ExtractCategories( JSON * dataFile, Array< Category > & outCategories )
{
	if ( dataFile == NULL )
	{
		return;
	}
	
	const JsonReader categories( dataFile->GetItemByName( CATEGORIES ) );

	if ( categories.IsArray() )
	{
		while ( const JSON * nextElement = categories.GetNextArrayElement() )
		{
			const JsonReader category( nextElement );
			if ( category.IsObject() )
			{
				Category extractedCategory;
				extractedCategory.CategoryTag = category.GetChildStringByName( TAG );
				extractedCategory.Label	= category.GetChildStringByName( LABEL );
								
				// Check if we already have this category
				bool exists = false;
				for ( int i = 0; i < outCategories.GetSizeI(); ++i )
				{
					const Category & existingCat = outCategories.At( i );
					if ( extractedCategory.CategoryTag == existingCat.CategoryTag )
					{
						exists = true;
						break;
					}
				}

				if ( !exists )
				{
					LOG( "Extracting category: %s", extractedCategory.CategoryTag.ToCStr() );
					outCategories.PushBack( extractedCategory );
				}
			}
		}
	}
}

void OvrMetaData::ExtractMetaData( JSON * dataFile, const Array< String > & searchPaths, StringHash< OvrMetaDatum > & outMetaData ) const
{
	if ( dataFile == NULL )
	{
		return;
	}

	const JsonReader data( dataFile->GetItemByName( DATA ) );
	if ( data.IsArray() )
	{
		int jsonIndex = 0;
		while ( const JSON * nextElement = data.GetNextArrayElement() )
		{
			const JsonReader datum( nextElement );
			if ( datum.IsObject() )
			{
				OvrMetaDatum data;
				data.Id = jsonIndex++;
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

				bool foundPath = GetFullPath( searchPaths, relativeUrl, data.Url );
				if ( !foundPath )
				{
					foundPath = GetFullPath( searchPaths, relativeUrl + ".x", data.Url );
				}

				if ( foundPath )
				{
					data.Title = datum.GetChildStringByName( TITLE_INNER );
					data.Author = datum.GetChildStringByName( AUTHOR_INNER );
					//LOG( "OvrMetaData::ExtractMetaData adding datum %s with title %s", data.Url.ToCStr(), data.Title.ToCStr() );
					
					StringHash< OvrMetaDatum >::Iterator iter = outMetaData.FindCaseInsensitive( data.Url );
					if ( iter == outMetaData.End() )
					{
						outMetaData.Add( data.Url, data );
					}
					else
					{
						iter->Second = data;
					}
				}
				else
				{
					LOG( "OvrMetaData::ExtractMetaData failed to find %s", relativeUrl.ToCStr() );
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

	// Delete any data only tagged as "Favorite" - this is a fix for user created "Favorite" folder which is a special case
	// Not doing this will show photos already favorited that the user cannot unfavorite
	for ( int metaDataIndex = 0; metaDataIndex < MetaData.GetSizeI(); ++metaDataIndex )
	{
		OvrMetaDatum & metaDatum = MetaData.At( metaDataIndex );
		Array< String > & tags = metaDatum.Tags;

		OVR_ASSERT( metaDatum.Tags.GetSizeI() > 0 );
		if ( tags.GetSizeI() == 1 )
		{
			if ( tags.At( 0 ) == FAVORITES_TAG )
			{
				//LOG( "Removing %s", metaDatum.Url.ToCStr() );
				MetaData.RemoveAtUnordered( metaDataIndex );
			}
		}
	}

	// Fix the indices
	for ( int metaDataIndex = 0; metaDataIndex < MetaData.GetSizeI(); ++metaDataIndex )
	{
		OvrMetaDatum & datum = MetaData.At( metaDataIndex );
		Array< String > & tags = datum.Tags;

		OVR_ASSERT( tags.GetSizeI() > 0 );

		if ( tags.GetSizeI() == 1 ) OVR_ASSERT( tags.At( 0 ) != FAVORITES_TAG );

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
					//LOG( "OvrMetaData adding datum %s with index %d to %s", datum.Title.ToCStr(), metaDataIndex, category->CategoryTag.ToCStr() );
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
			const Category & cat = Categories.At( c );
			catObject->AddStringItem( TAG, cat.CategoryTag.ToCStr() );
			catObject->AddStringItem( LABEL, cat.Label.ToCStr() );
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
			// Handle case which leaves us with no tags - ie. broken state
			if ( metaDatum->Tags.GetSizeI() < 2 )
			{
				LOG( "ToggleTag attempt to remove only tag: %s on %s", newTag.ToCStr(), metaDatum->Url.ToCStr() );
				return TAG_ERROR;
			}

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
		if ( JSON * datum = data->GetItemByIndex( metaDatum->Id ) )
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
	cat.CategoryTag = name;
	Categories.PushBack( cat );
}

OvrMetaData::Category * OvrMetaData::GetCategory(const String & categoryName )
{
	const int numCategories = Categories.GetSizeI();
	for ( int i = 0; i < numCategories; ++i )
	{
		Category & category = Categories.At( i );
		if ( category.CategoryTag == categoryName )
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
		//const OvrMetaDatum * panoData = &MetaData.At( metaDataIndex );
		//LOG( "Getting MetaData %d title %s from category %s", metaDataIndex, panoData->Title.ToCStr(), category.CategoryName.ToCStr() );
		outMetaData.PushBack( &MetaData.At( metaDataIndex ) );
	}
	return true;
}

bool OvrMetaData::ShouldAddFile( const char * filename, const OvrMetaDataFileExtensions & fileExtensions )
{
	const int pathLen = OVR_strlen( filename );
	for ( int index = 0; index < fileExtensions.BadExtensions.GetSizeI(); ++index )
	{
		const String & ext = fileExtensions.BadExtensions.At( index );
		const int extLen = ext.GetLengthI();
		if ( pathLen > extLen && OVR_stricmp( filename + pathLen - extLen, ext.ToCStr() ) == 0 )
		{
			return false;
		}
	}

	for ( int index = 0; index < fileExtensions.GoodExtensions.GetSizeI(); ++index )
	{
		const String & ext = fileExtensions.GoodExtensions.At( index );
		const int extLen = ext.GetLengthI();
		if ( pathLen > extLen && OVR_stricmp( filename + pathLen - extLen, ext.ToCStr() ) == 0 )
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
		const Array< String > & searchPaths,
		OvrMetaData & metaData,
		float panelWidth,
		float panelHeight,
		float radius_,
		unsigned numSwipePanels, 
		unsigned thumbWidth,
		unsigned thumbHeight )
	: VRMenu( MENU_NAME )
	, AppPtr( app )
	, SearchPaths( searchPaths )
	, MetaData( metaData )
	, ThumbWidth( thumbWidth )
	, ThumbHeight( thumbHeight )
	, PanelTextSpacingScale( 0.5f )
	, FolderTitleSpacingScale( 0.5f )
	, NumSwipePanels( numSwipePanels )
	, NoMedia( false )
	, AllowPanelTouchUp( false )
	, ScrollHintShown( false )
	, TextureCommands( 10000 )
	, BackgroundCommands( 10000 )
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

		OVR_ASSERT( ThumbPanelBG != 0 && panelW == ThumbWidth && panelH == ThumbHeight );
	}

	// spawn the thumbnail loading thread with the command list
	pthread_attr_t loadingThreadAttr;
	pthread_attr_init( &loadingThreadAttr );
	sched_param sparam;
	sparam.sched_priority = Thread::GetOSPriority( Thread::BelowNormalPriority );
	pthread_attr_setschedparam( &loadingThreadAttr, &sparam );

	pthread_t loadingThread;
	const int createErr = pthread_create( &loadingThread, &loadingThreadAttr, &ThumbnailThread, this );
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
	Init( app->GetVRMenuMgr(), app->GetDefaultFont(), 0.0f, VRMenuFlags_t(), comps );
}

OvrFolderBrowser::~OvrFolderBrowser()
{
	if ( ThumbPanelBG != NULL )
	{
		free( ThumbPanelBG );
		ThumbPanelBG = NULL;
	}

	int numFolders = Folders.GetSizeI();
	for ( int i = 0; i < numFolders; ++i )
	{
		Folder * folder = Folders.At( i );
		if ( folder )
		{
			delete folder;
		}
	}
}

void OvrFolderBrowser::Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
	BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId )
{
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
}

void OvrFolderBrowser::BuildMenu()
{
	const OvrStoragePaths & storagePaths = AppPtr->GetStoragePaths();
	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_CACHE, "", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", ThumbSearchPaths );

	// move the root up to eye height
	OvrVRMenuMgr & menuManager = AppPtr->GetVRMenuMgr();
	BitmapFont & font = AppPtr->GetDefaultFont();
	VRMenuObject * root = menuManager.ToObject( GetRootHandle() );
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
		LOG( "Loading folder %i named %s", folderIndex, categories.At( folderIndex ).CategoryTag.ToCStr() );
		BuildFolder( folderIndex );

		// Set up initial positions - 0 in center, the rest ascending in order below it
		const Folder & folder = GetFolder( folderIndex );
		mediaCount += folder.Panels.GetSizeI();
		VRMenuObject * folderObject = menuManager.ToObject( folder.Handle );
		OVR_ASSERT( folderObject != NULL );
		folderObject->SetLocalPosition( ( DOWN * PanelHeight * folderIndex ) + folderObject->GetLocalPosition() );
	}

	// Process any thumb creation commands
	char cmd[ 1024 ];
	sprintf( cmd, "processCreates %p", &ThumbCreateAndLoadCommands );
	BackgroundCommands.PostString( cmd );

	// Show no media menu if no media found
	if ( mediaCount == 0 )
	{
		String title;
		String imageFile;
		String message;
		OnMediaNotFound( AppPtr, title, imageFile, message );

		// Create a folder if we didn't create at least one to display no media info
		if ( Folders.GetSizeI() < 1 )
		{
			MetaData.AddCategory( "No Media" );
			BuildFolder( 0 );
		}

		// Set title 
		const Folder & folder = GetFolder( 0 );
		VRMenuObject * folderTitleObject = menuManager.ToObject( folder.TitleHandle );
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
		AddItems( menuManager, font, parms, folder.SwipeHandle, true ); // PARENT: folder.TitleRootHandle
		parms.Clear();

		NoMedia = true;
		return;
	}

	// Show scroll hint if we have more than one folder and favorites not empty
	Array< VRMenuObjectParms const * > parms;
	const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 1.0f );
	const Folder & favFolder = GetFolder( 0 );
	if ( Folders.GetSizeI() < 2 || ( Folders.GetSizeI() == 2 && favFolder.Panels.IsEmpty() ) )
	{
		// Don't bother showing scroll hint
		ScrollHintShown = true;
	}
	else // build scroll hint
	{
		// Set up "swipe to browse" hint
		// We want the hint to take up the top "empty" folder space at launch and go away once user scrolls down
		// If the top folder is empty, use its position, otherwise attach one above
		const int numFrames = 6;
		const int numLeads = 1;
		const int numTrails = 2;
		const int numIndicatorChildren = 3;
		const float swipeFPS = 4.0f;

		VRMenuId_t swipeDownId( ID_CENTER_ROOT.Get() + 407 );

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
		AddItems( menuManager, font, parms, GetRootHandle(), false );
		parms.Clear();
		comps.Clear();

		ScrollHintHandle = HandleForId( menuManager, swipeDownId );
		VRMenuObject * swipeHintObject = menuManager.ToObject( ScrollHintHandle );
		OVR_ASSERT( swipeHintObject != NULL );

		swipeHintObject->SetLocalPosition( ( UP * PanelHeight ) + swipeHintObject->GetLocalPosition() );
		swipeHintObject->SetColor( Vector4f( 1.0f, 1.0f, 1.0f, 0.5f ) );
		String swipeToBrowse;
		VrLocale::GetString( AppPtr->GetVrJni(), AppPtr->GetJavaObject(), "@string/swipe_to_browse", "@string/swipe_to_browse", swipeToBrowse );
		// Hint text
		Posef textPose( Quatf(), DOWN * 0.2f );
		VRMenuObjectParms titleParms(
			VRMENU_STATIC,
			Array< VRMenuComponent* >(),
			VRMenuSurfaceParms(),
			swipeToBrowse,
			textPose,
			Vector3f( 0.7f ),
			fontParms,
			VRMenuId_t(),
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
			VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
		parms.PushBack( &titleParms );
		AddItems( menuManager, font, parms, ScrollHintHandle, true ); // PARENT: SwipeHintHandle
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
			AddItems( menuManager, font, parms, ScrollHintHandle, false );
			parms.Clear();
		}

		if ( OvrTrailsAnimComponent* animComp = swipeHintObject->GetComponentByName< OvrTrailsAnimComponent >() )
		{
			animComp->Play();
		}

		const char * indicatorLeftIcon = "res/raw/wrap_left.png";
		VRMenuSurfaceParms indicatorSurfaceParms( "leftSurface",
				indicatorLeftIcon, SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );

		// Wrap around indicator - used for indicating all folders/category wrap around.
		VRMenuId_t indicatorId( ID_CENTER_ROOT.Get() + 409 );
		Posef indicatorPos( Quatf(), -FWD * ( Radius + 0.1f ) + UP * PanelHeight * 0.0f );
		VRMenuObjectParms indicatorParms(
				VRMENU_STATIC,
				Array< VRMenuComponent* >(),
				indicatorSurfaceParms,
				"",
				indicatorPos,
				Vector3f( 3.0f ),
				fontParms,
				indicatorId,
				VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ),
				VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

		parms.PushBack( &indicatorParms );

		AddItems(menuManager, font, parms, GetRootHandle(), true);
		menuHandle_t foldersWrapHandle = root->ChildHandleForId(menuManager, indicatorId);
		VRMenuObject * wrapIndicatorObject = menuManager.ToObject( foldersWrapHandle );
		OVR_UNUSED( wrapIndicatorObject );
		OVR_ASSERT( wrapIndicatorObject != NULL );


		VRMenuObject * rootObject = AppPtr->GetVRMenuMgr().ToObject( GetRootHandle() );
		OVR_ASSERT( rootObject );

		OvrFolderBrowserRootComponent * rootComp = rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
		OVR_ASSERT( rootComp );

		rootComp->SetFoldersWrapHandle( foldersWrapHandle );
		rootComp->SetFoldersWrapHandleTopPosition( -FWD * ( 0.52f * Radius) + UP * PanelHeight * 1.0f );
		rootComp->SetFoldersWrapHandleBottomPosition( -FWD * ( 0.52f * Radius) + DOWN * PanelHeight * numFolders );

		wrapIndicatorObject->SetVisible( false );

		parms.Clear();
	}
}

void OvrFolderBrowser::BuildFolder( int folderIndex )
{
	OvrVRMenuMgr & menuManager = AppPtr->GetVRMenuMgr();
	BitmapFont & font = AppPtr->GetDefaultFont();

	// Load a category to build swipe folder
	//
	const OvrMetaData::Category & category = MetaData.GetCategory( folderIndex );

	Array< VRMenuObjectParms const * > parms;
	const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 1.0f );

	// Create internal folder struct
	String localizedCategoryName;
	VrLocale::GetString( AppPtr->GetOvrMobile(), VrLocale::MakeStringIdFromANSI( category.CategoryTag ), category.CategoryTag, localizedCategoryName );

	Folder * folder = new Folder( localizedCategoryName );
	const int numPanels = category.DatumIndicies.GetSizeI();
	LOG( "Building Folder %s with %d panels", category.CategoryTag.ToCStr(), numPanels );
	
	// Create OvrFolderBrowserFolderRootComponent root for folder
	VRMenuId_t folderId( ID_CENTER_ROOT.Get() + folderIndex + 1 );
	Array< VRMenuComponent* > comps;
	comps.PushBack( new OvrFolderSwipeComponent( *this, folder ) );
	VRMenuObjectParms folderParms(
		VRMENU_CONTAINER,
		comps,
		VRMenuSurfaceParms(),
		( folder->LocalizedName + " root" ).ToCStr(),
		Posef(),
		Vector3f( 1.0f ),
		fontParms,
		folderId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION )
		);
	parms.PushBack( &folderParms );
	AddItems( menuManager, font, parms, GetRootHandle(), false ); // PARENT: Root
	parms.Clear();

	// grab the folder handle and make sure it was created
	folder->Handle = HandleForId( menuManager, folderId );
	VRMenuObject * folderObject = menuManager.ToObject( folder->Handle );
	OVR_UNUSED( folderObject );

	// Add new OvrFolderBrowserSwipeComponent to folder
	VRMenuId_t swipeFolderId( ID_CENTER_ROOT.Get() + folderIndex + 100000 );
	Array< VRMenuComponent* > swipeComps;
	swipeComps.PushBack( new OvrFolderBrowserSwipeComponent( *this, folder ) );
	VRMenuObjectParms swipeParms(
		VRMENU_CONTAINER, 
		swipeComps,
		VRMenuSurfaceParms(), 
		( folder->LocalizedName + " swipe" ).ToCStr(),
		Posef(), 
		Vector3f( 1.0f ), 
		fontParms,
		swipeFolderId,
		VRMenuObjectFlags_t( VRMENUOBJECT_NO_GAZE_HILIGHT ), 
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) 
		);
	parms.PushBack( &swipeParms );
	AddItems( menuManager, font, parms, folder->Handle, false ); // PARENT: folder->Handle
	parms.Clear();

	// grab the SwipeHandle handle and make sure it was created
	folder->SwipeHandle = folderObject->ChildHandleForId( menuManager, swipeFolderId );
	VRMenuObject * swipeObject = menuManager.ToObject( folder->SwipeHandle );
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
		LoadFolderPanels( category, folderIndex, *folder, parms );
		
		// Add panels to swipehandle
		AddItems( menuManager, font, parms, folder->SwipeHandle, false );
		DeletePointerArray( parms );
		parms.Clear();

		// Assign handles to panels
		for ( int i = 0; i < folder->Panels.GetSizeI(); ++i )
		{
			Panel& panel = folder->Panels.At( i );
			panel.Handle = swipeObject->GetChildHandleForIndex( i );
		}
	}

	// Folder title
	VRMenuId_t folderTitleRootId( ID_CENTER_ROOT.Get() + folderIndex + 1000000 );
	VRMenuObjectParms titleRootParms(
		VRMENU_CONTAINER,
		Array< VRMenuComponent* >(),
		VRMenuSurfaceParms(),
		( folder->LocalizedName + " title root" ).ToCStr(),
		Posef(),
		Vector3f( 1.0f ),
		fontParms,
		folderTitleRootId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION )
		);
	parms.PushBack( &titleRootParms );
	AddItems( menuManager, font, parms, folder->Handle, false ); // PARENT: folder->Handle
	parms.Clear();

	// grab the title root handle and make sure it was created
	folder->TitleRootHandle = folderObject->ChildHandleForId( menuManager, folderTitleRootId );
	VRMenuObject * folderTitleRootObject = menuManager.ToObject( folder->TitleRootHandle );
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
	AddItems( menuManager, font, parms, folder->TitleRootHandle, true ); // PARENT: folder->TitleRootHandle
	parms.Clear();

	// grab folder title handle and make sure it was created
	folder->TitleHandle = folderTitleRootObject->ChildHandleForId( menuManager, folderTitleId );
	VRMenuObject * folderTitleObject = menuManager.ToObject( folder->TitleHandle );
	OVR_UNUSED( folderTitleObject );
	OVR_ASSERT( folderTitleObject != NULL );
	

	// Wrap around indicator
	VRMenuId_t indicatorId( ID_CENTER_ROOT.Get() + folderIndex + 1200000 );
	Posef indicatorPos( Quatf(), -FWD * ( Radius + 0.1f ) + UP * PanelHeight * 0.0f );

	const char * leftIcon = "res/raw/wrap_left.png";
	VRMenuSurfaceParms indicatorSurfaceParms( "leftSurface",
			leftIcon, SURFACE_TEXTURE_DIFFUSE,
			NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );

	VRMenuObjectParms indicatorParms(
			VRMENU_STATIC,
			Array< VRMenuComponent* >(),
			indicatorSurfaceParms,
			"",
			indicatorPos,
			Vector3f( 3.0f ),
			fontParms,
			indicatorId,
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ),
			VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	parms.PushBack( &indicatorParms );

	AddItems( menuManager, font, parms, folder->TitleRootHandle, true ); // PARENT: folder->TitleRootHandle
	folder->WrapIndicatorHandle = folderTitleRootObject->ChildHandleForId( menuManager, indicatorId );
	VRMenuObject * wrapIndicatorObject = menuManager.ToObject( folder->WrapIndicatorHandle );
	OVR_UNUSED( wrapIndicatorObject );
	OVR_ASSERT( wrapIndicatorObject != NULL );

	parms.Clear();

	SetWrapIndicatorVisible( *folder, false );

	// Add folder to local array
	Folders.PushBack( folder );

	UpdateFolderTitle( folderIndex );
	Folders.Back()->MaxRotation = CalcFolderMaxRotation( folderIndex );
}

void OvrFolderBrowser::RebuildFolder( const int folderIndex, const Array< OvrMetaDatum * > & data )
{
	if ( folderIndex >= 0 && Folders.GetSizeI() > folderIndex )
	{		
		OvrVRMenuMgr & menuManager = AppPtr->GetVRMenuMgr();
		BitmapFont & font = AppPtr->GetDefaultFont();

		Folder & folder = GetFolder( folderIndex );
		VRMenuObject * swipeObject = menuManager.ToObject( folder.SwipeHandle );
		OVR_ASSERT( swipeObject );

		swipeObject->FreeChildren( menuManager );
		folder.Panels.Clear();

		Array< VRMenuObjectParms const * > outParms;
		for ( int panelIndex = 0; panelIndex < data.GetSizeI(); ++panelIndex )
		{
			OvrMetaDatum * panelData = data.At( panelIndex );
			if ( panelData )
			{
				AddPanelToFolder( data.At( panelIndex ), folderIndex, folder, outParms );
				AddItems( menuManager, font, outParms, folder.SwipeHandle, false );
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
	const Folder & folder = GetFolder( folderIndex );
	const int numPanels = folder.Panels.GetSizeI();
	
	String folderTitle = folder.LocalizedName;
	
	char buf[ 6 ];
	OVR_itoa( numPanels, buf, 6, 10 );

	folderTitle += " (";
	folderTitle += buf;
	folderTitle += ")";

	VRMenuObject * folderTitleObject = AppPtr->GetVRMenuMgr().ToObject( folder.TitleHandle );

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

	sched_param sparam;
	sparam.sched_priority = Thread::GetOSPriority( Thread::BelowNormalPriority );

	int setSchedparamResult = pthread_setschedparam( pthread_self(), SCHED_NORMAL, &sparam );
	if ( setSchedparamResult != 0 )
	{
		LOG( "FolderBrowser: pthread_setschedparam failed %s", strerror( setSchedparamResult ) );
	}

	OvrFolderBrowser * folderBrowser = ( OvrFolderBrowser * )v;
	for ( ;; )
	{
		folderBrowser->BackgroundCommands.SleepUntilMessage();
		const char * msg = folderBrowser->BackgroundCommands.GetNextMessage();
		//LOG( "BackgroundCommands: %s", msg );
		if ( MatchesHead( "load ", msg ) )
		{
			int folderId;
			int panelId;

			sscanf( msg, "load %i %i", &folderId, &panelId );
			const char * fileName = strstr( msg, ":" ) + 1;

			const String fullPath( fileName );

			int		width;
			int		height;
			unsigned char * data = folderBrowser->LoadThumbAndApplyAA( fullPath, width, height );
			if ( data != NULL )
			{
				folderBrowser->TextureCommands.PostPrintf( "thumb %i %i %p %i %i",
					folderId, panelId, data, width, height );
			}

			free( ( void * )msg );
		}
		else if ( MatchesHead( "processCreates ", msg ) )
		{
			Array< OvrCreateThumbCmd > * ThumbCreateAndLoadCommands;

			sscanf( msg, "processCreates %p", &ThumbCreateAndLoadCommands );

			for ( int i = 0; i < ThumbCreateAndLoadCommands->GetSizeI(); ++i )
			{
				const OvrCreateThumbCmd & cmd = ThumbCreateAndLoadCommands->At( i );
				int	width = 0;
				int height = 0;
				unsigned char * data = folderBrowser->CreateThumbnail( cmd.SourceImagePath, width, height );

				// Should we write out a trivial thumbnail if the create failed?
				if ( data != NULL )
				{
					LOG( "thumb create - writjpeg %s %p %dx%d", cmd.ThumbDestination.ToCStr(), data, width, height );
					// write it out
					WriteJpeg( cmd.ThumbDestination, data, width, height );
					
					// Perform the load
					const unsigned ThumbWidth = folderBrowser->GetThumbWidth();
					const unsigned ThumbHeight = folderBrowser->GetThumbHeight();

					const int numBytes = width * height * 4;
					const int thumbPanelBytes = ThumbWidth * ThumbHeight * 4;
					if ( numBytes != thumbPanelBytes )
					{
						LOG( "Thumbnail image '%s' is the wrong size! Regenerate thumbnails!", cmd.ThumbDestination.ToCStr() );
						free( data );
					}
					else
					{
						// Apply alpha from vrlib/res/raw to alpha channel for anti-aliasing
						for ( int i = 3; i < thumbPanelBytes; i += 4 )
						{
							data[ i ] = ThumbPanelBG[ i ];
						}

						int folderId;
						int panelId;

						sscanf( cmd.LoadCmd, "load %i %i", &folderId, &panelId );

						folderBrowser->TextureCommands.PostPrintf( "thumb %i %i %p %i %i",
							folderId, panelId, data, width, height );
					}
				}
			}
			ThumbCreateAndLoadCommands->Clear();

			free( ( void * )msg );
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
	int folderId;
	int panelId;
	unsigned char * data;
	int width;
	int height;

	sscanf( thumbnailCommand, "thumb %i %i %p %i %i", &folderId, &panelId, &data, &width, &height );
	
	Folder * folder = &GetFolder( folderId );
	OVR_ASSERT( folder );

	Array<Panel> * panels = &folder->Panels;
	OVR_ASSERT( panels );

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
	panelObject = AppPtr->GetVRMenuMgr().ToObject( panel->Handle );
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
	LOG( "Building %d panels for %s", numPanos, category.CategoryTag.ToCStr() );
	for ( int panoIndex = 0; panoIndex < numPanos; panoIndex++ )
	{
		AddPanelToFolder( const_cast< OvrMetaDatum * const >( categoryPanos.At( panoIndex ) ), folderIndex, folder, outParms );
	}
}

//==============================================================
// OvrPanel_OnUp
// Forwards a touch up from Panel to Menu
// Holds pointer to the datum panel represents
class OvrPanel_OnUp : public VRMenuComponent_OnTouchUp
{
public:
	static const int TYPE_ID = 1107;

	OvrPanel_OnUp( VRMenu * menu, OvrMetaDatum * panoData ) :
		VRMenuComponent_OnTouchUp(),
		Menu( menu ),
		Data( panoData )
	{
		OVR_ASSERT( Data );
	}

	void				SetData( OvrMetaDatum * panoData )	{ Data = panoData; }

	virtual int			GetTypeId() const					{ return TYPE_ID; }
	OvrMetaDatum *		GetData() const						{ return Data; }

private:
	virtual eMsgStatus  OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
		VRMenuObject * self, VRMenuEvent const & event )
	{
		OVR_ASSERT( event.EventType == VRMENU_EVENT_TOUCH_UP );
		if ( OvrFolderBrowser * folderBrowser = static_cast< OvrFolderBrowser * >( Menu ) )
		{
			folderBrowser->OnPanelUp( Data );
		}

		return MSG_STATUS_CONSUMED;
	}

private:
	VRMenu *				Menu;	// menu that holds the button
	OvrMetaDatum *			Data;	// Payload for this button
};

void OvrFolderBrowser::AddPanelToFolder( OvrMetaDatum * const panoData, const int folderIndex, Folder & folder, Array< VRMenuObjectParms const * >& outParms )
{
	// Need data in panel to be non const as it will be modified externally
	OVR_ASSERT( panoData );

	if ( !FileExists( panoData->Url ) )
	{
		return;
	}

	Panel panel;
	const int panelIndex = folder.Panels.GetSizeI();
	panel.Id = panelIndex;
	panel.Size.x = PanelWidth;
	panel.Size.y = PanelHeight;

	String panelTitle;
	VrLocale::GetString( AppPtr->GetOvrMobile(), panoData->Title, panoData->Title, panelTitle );

	// Load a panel
	Array< VRMenuComponent* > panelComps;
	VRMenuId_t id( ID_CENTER_ROOT.Get() + panelIndex + 10000000 );

	int  folderIndexShifted = folderIndex << 24;
	VRMenuId_t buttonId( folderIndexShifted | panel.Id );

	panelComps.PushBack( new OvrPanel_OnUp( this, panoData ) );
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

	// single-pass multitexture
	VRMenuSurfaceParms panelSurfParms( "panelSurface",
		panelSrc, SURFACE_TEXTURE_DIFFUSE,
		panelHiSrc, SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX );

	float const factor = ( float )panelIndex / ( float )CircumferencePanelSlots;

	Quatf rot( DOWN, ( Mathf::TwoPi * factor ) );

	Vector3f dir( -FWD * rot );
	Posef panelPose( rot, dir * Radius );
	Vector3f panelScale( 1.0f );

	const Posef textPose( Quatf(), Vector3f( 0.0f, -PanelHeight * PanelTextSpacingScale, 0.0f ) );

	const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 0.5f );
	VRMenuObjectParms * p = new VRMenuObjectParms( VRMENU_BUTTON, panelComps,
		panelSurfParms, panelTitle, panelPose, panelScale, textPose, Vector3f( 1.0f ), fontParms, id,
		VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	outParms.PushBack( p );

	folder.Panels.PushBack( panel );

	const String & panoUrl = panoData->Url;

	// Create or load thumbnail
	const String thumbName = ThumbName( panoUrl );
 	const String fileBase = ExtractFileBase( thumbName );
	
	// Try search paths next to image for retail photos
	String finalThumb;
	if ( !GetFullPath( ThumbSearchPaths, thumbName, finalThumb ) )
	{
		// Try app cache for cached user pano thumbs
		if ( !GetFullPath( ThumbSearchPaths, ThumbName( fileBase ), finalThumb ) )
		{
			const String altThumbPath = AlternateThumbName( panoUrl );
			if ( altThumbPath.IsEmpty() || !GetFullPath( ThumbSearchPaths, altThumbPath, finalThumb ) )
			{
				int pathLen = panoUrl.GetLengthI();
				if ( pathLen > 2 && OVR_stricmp( panoUrl.ToCStr() + pathLen - 2, ".x" ) == 0 )
				{
					LOG( "Thumbnails cannot be generated from encrypted images." );
					return; // No thumb & can't create 
				}

				// Create and write out thumbnail to app cache
				finalThumb = ThumbSearchPaths[ 2 ] + ThumbName( fileBase );

				// Create the path if necessary
				MakePath( finalThumb, S_IRUSR | S_IWUSR );

				if ( HasPermission( finalThumb, W_OK ) )
				{
					if ( FileExists( panoData->Url ) && HasPermission( finalThumb, W_OK ) )
					{
						OvrCreateThumbCmd createCmd;
						createCmd.SourceImagePath = panoUrl;
						createCmd.ThumbDestination = finalThumb;
						char loadCmd[ 1024 ];
						sprintf( loadCmd, "load %i %i:%s", folderIndex, panel.Id, finalThumb.ToCStr() );
						createCmd.LoadCmd = loadCmd;
						ThumbCreateAndLoadCommands.PushBack( createCmd );
					}
					return;
				}
			}
		}
	}
	char cmd[ 1024 ];
	sprintf( cmd, "load %i %i:%s", folderIndex, panel.Id, finalThumb.ToCStr() );
	//LOG( "Thumb cmd: %s", cmd );
	BackgroundCommands.PostString( cmd );
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
	return *Folders.At( index );
}

OvrFolderBrowser::Folder& OvrFolderBrowser::GetFolder( int index )
{
	return *Folders.At( index );
}

bool OvrFolderBrowser::RotateCategory( const CategoryDirection direction )
{
	OvrFolderBrowserSwipeComponent * swipeComp = GetSwipeComponentForActiveFolder();
	return swipeComp->Rotate( direction );
}

void OvrFolderBrowser::SetCategoryRotation( const int folderIndex, const int panelIndex )
{
	OVR_ASSERT( folderIndex >= 0 && folderIndex < Folders.GetSizeI() );
	const Folder& folder = GetFolder( folderIndex );
	VRMenuObject * swipe = AppPtr->GetVRMenuMgr().ToObject( folder.SwipeHandle );
	OVR_ASSERT( swipe );

	OvrFolderBrowserSwipeComponent * swipeComp = swipe->GetComponentById< OvrFolderBrowserSwipeComponent >();
	OVR_ASSERT( swipeComp );

	swipeComp->SetRotationByIndex( panelIndex );
}

void OvrFolderBrowser::OnPanelUp( OvrMetaDatum * const data )
{
	if ( AllowPanelTouchUp )
	{
		OnPanelActivated( data );

		// Hide scroll hint
		if ( !ScrollHintShown )
		{
			SetScrollHintVisible( false );
			ScrollHintShown = true;
		}
	}
}

OvrMetaDatum * OvrFolderBrowser::NextFileInDirectory( const int step )
{
	Folder & folder = GetFolder( GetActiveFolderIndex() );

	const int numPanels = folder.Panels.GetSizeI();

	if ( numPanels == 0 )
	{
		return NULL;
	}
	
	OvrFolderBrowserSwipeComponent * swipeComp = GetSwipeComponentForActiveFolder();
	OVR_ASSERT( swipeComp );

	int nextPanelIndex = swipeComp->CurrentPanelIndex() + step;
	if ( nextPanelIndex >= numPanels )
	{
		nextPanelIndex = 0;
	}
	else if ( nextPanelIndex < 0 )
	{
		nextPanelIndex = numPanels - 1;
	}
		
	Panel & panel = folder.Panels.At( nextPanelIndex );
	VRMenuObject * panelObject = AppPtr->GetVRMenuMgr().ToObject( panel.Handle );
	OVR_ASSERT( panelObject );

	OvrPanel_OnUp * panelUpComp = panelObject->GetComponentById<OvrPanel_OnUp>();
	OVR_ASSERT( panelUpComp );

	OvrMetaDatum * datum = panelUpComp->GetData();
	OVR_ASSERT( datum );

	swipeComp->SetRotationByIndex( nextPanelIndex );

	return datum;
}

float OvrFolderBrowser::CalcFolderMaxRotation( const int folderIndex ) const
{
	const Folder & folder = GetFolder( folderIndex );
	int numPanels = Alg::Clamp( folder.Panels.GetSizeI() - 1, 0, INT_MAX );
	return static_cast< float >( numPanels );
}

void OvrFolderBrowser::SetScrollHintVisible( const bool visible )
{
	if ( ScrollHintHandle.IsValid() )
	{
		VRMenuObject * scrollHintObject = AppPtr->GetVRMenuMgr().ToObject( ScrollHintHandle );
		if ( scrollHintObject )
		{
			scrollHintObject->SetVisible( visible );
		}
	}
}

void OvrFolderBrowser::SetWrapIndicatorVisible( Folder& folder, const bool visible )
{
	if ( folder.WrapIndicatorHandle.IsValid() )
	{
		VRMenuObject * wrapIndicatorObject = AppPtr->GetVRMenuMgr().ToObject( folder.WrapIndicatorHandle );
		if ( wrapIndicatorObject )
		{
			wrapIndicatorObject->SetVisible( visible );
		}
	}
}

OvrFolderBrowserSwipeComponent * OvrFolderBrowser::GetSwipeComponentForActiveFolder()
{
	const Folder & folder = GetFolder( GetActiveFolderIndex() );

	VRMenuObject * swipeObject = AppPtr->GetVRMenuMgr().ToObject( folder.SwipeHandle );
	OVR_ASSERT( swipeObject );

	OvrFolderBrowserSwipeComponent * swipeComp = swipeObject->GetComponentById<OvrFolderBrowserSwipeComponent>();
	OVR_ASSERT( swipeComp );

	return swipeComp;
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

void OvrFolderBrowser::GetFolderData( const int folderIndex, Array< OvrMetaDatum * >& data ) const
{
	if ( Folders.GetSizeI() > 0 && folderIndex >= 0 && folderIndex < Folders.GetSizeI() )
	{
		const Folder & folder = GetFolder( folderIndex );
		const Array<Panel> & panels = folder.Panels;
		for ( int panelIndex = 0; panelIndex < panels.GetSizeI(); ++panelIndex )
		{
			const Panel & panel = folder.Panels.At( panelIndex );
			VRMenuObject * panelObject = AppPtr->GetVRMenuMgr().ToObject( panel.Handle );
			OVR_ASSERT( panelObject );

			OvrPanel_OnUp * panelUpComp = panelObject->GetComponentById<OvrPanel_OnUp>();
			OVR_ASSERT( panelUpComp );

			OvrMetaDatum * panelData = panelUpComp->GetData();
			OVR_ASSERT( panelData );

			data.PushBack( panelData );
		}
	}
}

int OvrFolderBrowser::GetActiveFolderIndex() const
{
	VRMenuObject * rootObject = AppPtr->GetVRMenuMgr().ToObject( GetRootHandle() );
	OVR_ASSERT( rootObject );

	OvrFolderBrowserRootComponent * rootComp = rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
	OVR_ASSERT( rootComp );

	return rootComp->GetCurrentIndex( rootObject );
}

} // namespace OVR
