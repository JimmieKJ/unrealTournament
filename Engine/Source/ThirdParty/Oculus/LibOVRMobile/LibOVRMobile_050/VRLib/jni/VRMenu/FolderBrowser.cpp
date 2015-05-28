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
#include "AnimComponents.h"
#include "linux/stat.h"
#include "VrCommon.h"
#include "VrLocale.h"
#include "VRMenuObject.h"
#include "ScrollBarComponent.h"
#include "SwipeHintComponent.h"

namespace OVR {

char const * OvrFolderBrowser::MENU_NAME = "FolderBrowser";

const Vector3f FWD( 0.0f, 0.0f, -1.0f );
const Vector3f LEFT( -1.0f, 0.0f, 0.0f );
const Vector3f RIGHT( 1.0f, 0.0f, 0.0f );
const Vector3f UP( 0.0f, 1.0f, 0.0f );
const Vector3f DOWN( 0.0f, -1.0f, 0.0f );
const float SCROLL_REPEAT_TIME 					= 0.5f;
const float EYE_HEIGHT_OFFSET 					= 0.0f;
const float MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ 	= 1800.0f;
const float SCROLL_HITNS_VISIBILITY_TOGGLE_TIME = 5.0f;
const float SCROLL_BAR_LENGTH					= 390;
const int 	HIDE_SCROLLBAR_UNTIL_ITEM_COUNT		= 1; // <= 0 makes scroll bar always visible

// Helper class that guarantees unique ids for VRMenuIds
class OvrUniqueId
{
public: 
	explicit OvrUniqueId( int startId )
		: currentId( startId )
	{}

	int Get( const int increment )
	{
		const int toReturn = currentId;
		currentId += increment;
		return toReturn;
	}

private:
	int currentId;
};
OvrUniqueId uniqueId( 1000 );

VRMenuId_t OvrFolderBrowser::ID_CENTER_ROOT( uniqueId.Get( 1 ) );

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
	Quatf rot( -FWD, rotation );
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
		, ValidFoldersCount( 0 )
	{
		LastInteractionTimeStamp = ovr_GetTimeInSeconds() - SCROLL_HITNS_VISIBILITY_TOGGLE_TIME;
		ScrollMgr.SetScrollPadding(0.5f);
		ScrollMgr.SetWrapAroundEnable( false );
	}

	virtual int		GetTypeId() const
	{ 
		return TYPE_ID;
	}

	int GetCurrentIndex( VRMenuObject * self, OvrVRMenuMgr & menuMgr ) const
	{
		VRMenuObject * foldersRootObject = menuMgr.ToObject( FoldersRootHandle );
		OVR_ASSERT( foldersRootObject != NULL );

		// First calculating index of a folder with in valid folders(folder that has atleast one panel) array based on its position
		const int validNumFolders = GetFolderBrowserValidFolderCount();
		const float deltaHeight = FolderBrowser.GetPanelHeight();
		const float maxHeight = deltaHeight * validNumFolders;
		const float positionRatio = foldersRootObject->GetLocalPosition().y / maxHeight;
		int idx = nearbyintf( positionRatio * validNumFolders );
		idx = Alg::Clamp( idx, 0, FolderBrowser.GetNumFolders() - 1 );

		// Remapping index with in valid folders to index in entire Folder array
		const int numValidFolders = GetFolderBrowserValidFolderCount();
		int counter = 0;
		int remapedIdx = 0;
		for( ; remapedIdx < numValidFolders; ++remapedIdx )
		{
			OvrFolderBrowser::FolderView * folder = FolderBrowser.GetFolderView( remapedIdx );
			if( folder && !folder->Panels.IsEmpty() )
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

	void SetActiveFolder( int folderIdx )
	{
		ScrollMgr.SetPosition( folderIdx - 1 );
	}

	void SetFoldersRootHandle( menuHandle_t handle ) { FoldersRootHandle = handle; }
	void SetScrollDownHintHandle( menuHandle_t handle ) { ScrollDownHintHandle = handle; }
	void SetScrollUpHintHandle( menuHandle_t handle )  { ScrollUpHintHandle = handle; }
	void SetFoldersWrapHandle( menuHandle_t handle ) { FoldersWrapHandle = handle; }
	void SetFoldersWrapHandleTopPosition( Vector3f position ) { FoldersWrapHandleTopPosition = position; }
	void SetFoldersWrapHandleBottomPosition( Vector3f position ) { FoldersWrapHandleBottomPosition = position; }

private:
	const static float ACTIVE_DEPTH_OFFSET = 3.4f;

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
		ValidFoldersCount = 0;
		// Hides invalid folders(folder that doesn't have at least one panel), and rearranges valid folders positions to avoid gaps between folders
		for ( int i = 0; i < folderCount; ++i )
		{
			const OvrFolderBrowser::FolderView * currentFolder = FolderBrowser.GetFolderView( i );
			if ( currentFolder != NULL )
			{
				VRMenuObject * folderObject = menuMgr.ToObject( currentFolder->Handle );
				if ( folderObject != NULL )
				{
					if ( currentFolder->Panels.GetSizeI() > 0 )
					{
						folderObject->SetVisible( true );
						folderObject->SetLocalPosition( ( DOWN * FolderBrowser.GetPanelHeight() * ValidFoldersCount ) );
						++ValidFoldersCount;
					}
					else
					{
						folderObject->SetVisible( false );
					}
				}
			}
		}

		bool restrictedScrolling = ValidFoldersCount > 1;
		eScrollDirectionLockType touchDirLock = FolderBrowser.GetTouchDirectionLock();
		eScrollDirectionLockType controllerDirLock = FolderBrowser.GetControllerDirectionLock();

		ScrollMgr.SetMaxPosition( ValidFoldersCount - 1 );
		ScrollMgr.SetRestrictedScrollingData( restrictedScrolling, touchDirLock, controllerDirLock );

		unsigned int controllerInput = 0;
		if ( ValidFoldersCount > 1 ) // Need at least one folder in order to enable vertical scrolling
		{
			controllerInput = vrFrame.Input.buttonState;
			if ( controllerInput & ( BUTTON_LSTICK_UP | BUTTON_DPAD_UP | BUTTON_LSTICK_DOWN | BUTTON_DPAD_DOWN | BUTTON_LSTICK_LEFT | BUTTON_DPAD_LEFT | BUTTON_LSTICK_RIGHT | BUTTON_DPAD_RIGHT ) )
			{
				LastInteractionTimeStamp = ovr_GetTimeInSeconds();
			}

			if ( restrictedScrolling )
			{
				if ( touchDirLock == HORIZONTAL_LOCK )
				{
					if ( ScrollMgr.IsTouchIsDown() )
					{
						// Restricted scrolling enabled, but locked horizontal scrolling so ignore vertical scrolling
						ScrollMgr.TouchUp();
					}
				}

				if ( controllerDirLock != VERTICAL_LOCK )
				{
					// Restricted scrolling enabled, but not locked to vertical scrolling so lose the controller input
					controllerInput = 0;
				}
			}
		}
		ScrollMgr.Frame( vrFrame.DeltaSeconds, controllerInput );

		VRMenuObject * foldersRootObject = menuMgr.ToObject( FoldersRootHandle );
		const Vector3f & rootPosition = foldersRootObject->GetLocalPosition();
		foldersRootObject->SetLocalPosition( Vector3f( rootPosition.x, FolderBrowser.GetPanelHeight() * ScrollMgr.GetPosition(), rootPosition.z ) );

		const float alphaSpace = FolderBrowser.GetPanelHeight() * 2.0f;
		const float rootOffset = rootPosition.y - EYE_HEIGHT_OFFSET;

		// Fade + hide each category/folder based on distance from origin
		for ( int index = 0; index < FolderBrowser.GetNumFolders(); ++index )
		{
			//const OvrFolderBrowser::Folder & folder = FolderBrowser.GetFolder( index );
			VRMenuObject * child = menuMgr.ToObject( foldersRootObject->GetChildHandleForIndex( index ) );
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
				float ratioSq = ratio * ratio;
				float finalAlpha = 1.0f - ratioSq;
				color.w = Alg::Clamp( finalAlpha, 0.0f, 1.0f );
				flags &= ~( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL );

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

		if ( 0 )
		{	
			// Updating folder wrap around indicator logic/effect
			VRMenuObject * wrapIndicatorObject = menuMgr.ToObject( FoldersWrapHandle );
			if ( wrapIndicatorObject != NULL && ScrollMgr.IsWrapAroundEnabled() )
			{
				if ( ScrollMgr.IsScrolling() && ScrollMgr.IsOutOfBounds() )
				{
					wrapIndicatorObject->SetVisible( true );
					bool scrollingAtStart = ( ScrollMgr.GetPosition() < 0.0f );
					if ( scrollingAtStart )
					{
						UpdateWrapAroundIndicator( ScrollMgr, menuMgr, wrapIndicatorObject, FoldersWrapHandleTopPosition, -Mathf::PiOver2 );
					}
					else
					{
						FoldersWrapHandleBottomPosition.y = -FolderBrowser.GetPanelHeight() * ValidFoldersCount;
						UpdateWrapAroundIndicator( ScrollMgr, menuMgr, wrapIndicatorObject, FoldersWrapHandleBottomPosition, Mathf::PiOver2 );
					}

					Vector3f position = wrapIndicatorObject->GetLocalPosition();
					float ratio;
					if ( scrollingAtStart )
					{
						ratio = LinearRangeMapFloat( ScrollMgr.GetPosition(), -1.0f, 0.0f, 0.0f, 1.0f );
					}
					else
					{
						ratio = LinearRangeMapFloat( ScrollMgr.GetPosition(), ValidFoldersCount - 1.0f, ValidFoldersCount, 1.0f, 0.0f );
					}
					float finalAlpha = -( ratio * ratio ) + 1.0f;

					position.z += finalAlpha;
					wrapIndicatorObject->SetLocalPosition( position );
				}
				else
				{
					wrapIndicatorObject->SetVisible( false );
				}
			}
		}

		// Updating Scroll Suggestions
		bool 		showScrollUpIndicator 	= false;
		bool 		showBottomIndicator 	= false;
		Vector4f 	finalCol( 1.0f, 1.0f, 1.0f, 1.0f );
		if( LastInteractionTimeStamp > 0.0f ) // is user interaction currently going on? ( during interacion LastInteractionTimeStamp value will be negative )
		{
			float timeDiff = ovr_GetTimeInSeconds() - LastInteractionTimeStamp;
			if( timeDiff > SCROLL_HITNS_VISIBILITY_TOGGLE_TIME ) // is user not interacting for a while?
			{
				// Show Indicators
				showScrollUpIndicator = ( ScrollMgr.GetPosition() >  0.8f );
				showBottomIndicator = ( ScrollMgr.GetPosition() < ( ( ValidFoldersCount - 1 ) - 0.8f ) );

				finalCol.w = Alg::Clamp( timeDiff, 5.0f, 6.0f ) - 5.0f;
			}
		}

		VRMenuObject * scrollUpHintObject = menuMgr.ToObject( ScrollUpHintHandle );
		if( scrollUpHintObject != NULL )
		{
			scrollUpHintObject->SetVisible( showScrollUpIndicator );
			scrollUpHintObject->SetColor( finalCol );
		}

		VRMenuObject * scrollDownHintObject = menuMgr.ToObject( ScrollDownHintHandle );
		if( scrollDownHintObject != NULL )
		{
			scrollDownHintObject->SetVisible(showBottomIndicator);
			scrollDownHintObject->SetColor( finalCol );
		}

		return  MSG_STATUS_ALIVE;
    }

	eMsgStatus TouchDown( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}

		FolderBrowser.TouchDown();
		if ( ValidFoldersCount > 1 )
		{
			ScrollMgr.TouchDown();
		}

		TotalTouchDistance = 0.0f;
		StartTouchPosition.x = event.FloatValue.x;
		StartTouchPosition.y = event.FloatValue.y;

		LastInteractionTimeStamp = -1.0f;

		return MSG_STATUS_ALIVE;	// don't consume -- we're just listening
	}

	eMsgStatus TouchUp( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}

		FolderBrowser.TouchUp();
		if ( ValidFoldersCount > 1 )
		{
			ScrollMgr.TouchUp();
		}

		bool allowPanelTouchUp = false;
		if ( TotalTouchDistance < MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ )
		{
			allowPanelTouchUp = true;
		}

		FolderBrowser.SetAllowPanelTouchUp( allowPanelTouchUp );
		LastInteractionTimeStamp = ovr_GetTimeInSeconds();

		return MSG_STATUS_ALIVE;
	}

	eMsgStatus TouchRelative( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}

		FolderBrowser.TouchRelative(event.FloatValue);
		Vector2f currentTouchPosition( event.FloatValue.x, event.FloatValue.y );
		TotalTouchDistance += ( currentTouchPosition - StartTouchPosition ).LengthSq();
		if ( ValidFoldersCount > 1 )
		{
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
			const OvrFolderBrowser::FolderView * currentFolder = FolderBrowser.GetFolderView( i );
			if ( currentFolder && currentFolder->Panels.GetSizeI() > 0 )
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
	int					ValidFoldersCount;

	menuHandle_t 		FoldersRootHandle;
	menuHandle_t		ScrollDownHintHandle;
	menuHandle_t		ScrollUpHintHandle;
	menuHandle_t		FoldersWrapHandle;
	Vector3f			FoldersWrapHandleTopPosition;
	Vector3f			FoldersWrapHandleBottomPosition;
	double				LastInteractionTimeStamp;
};

//==============================================================
// OvrFolderRootComponent
// Component attached to the root object of each folder 
class OvrFolderRootComponent : public VRMenuComponent
{
public:
	OvrFolderRootComponent( OvrFolderBrowser & folderBrowser, OvrFolderBrowser::FolderView * folder )
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
		OVR_ASSERT( FolderPtr );
		if ( FolderBrowser.GetFolderView( FolderBrowser.GetActiveFolderIndex() ) != FolderPtr )
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
	OvrFolderBrowser::FolderView * FolderPtr;
};

//==============================================================
// OvrFolderSwipeComponent
// Component that holds panel sub-objects and manages swipe left/right
class OvrFolderSwipeComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 58524;

	OvrFolderSwipeComponent( OvrFolderBrowser & folderBrowser, OvrFolderBrowser::FolderView * folder )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_TOUCH_RELATIVE | VRMENU_EVENT_TOUCH_DOWN | VRMENU_EVENT_TOUCH_UP )
		, FolderBrowser( folderBrowser )
		, ScrollMgr( HORIZONTAL_SCROLL )
		, FolderPtr( folder )
		, TouchDown( false )
    {
		OVR_ASSERT( FolderBrowser.GetCircumferencePanelSlots() > 0 );
		ScrollMgr.SetScrollPadding(0.5f);
		ScrollMgr.SetWrapAroundEnable( false );
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
    	OVR_ASSERT( FolderPtr );
    	bool const isActiveFolder = ( FolderPtr == FolderBrowser.GetFolderView( FolderBrowser.GetActiveFolderIndex() ) );
		if ( !isActiveFolder )
		{
			TouchDown = false;
		}

		OvrFolderBrowser::FolderView & folder = *FolderPtr;
		const int numPanels = folder.Panels.GetSizeI();
		eScrollDirectionLockType touchDirLock = FolderBrowser.GetTouchDirectionLock();
		eScrollDirectionLockType conrollerDirLock = FolderBrowser.GetControllerDirectionLock();

		ScrollMgr.SetMaxPosition( static_cast<float>( numPanels - 1 ) );
		ScrollMgr.SetRestrictedScrollingData( FolderBrowser.GetNumFolders() > 1, touchDirLock, conrollerDirLock );

		unsigned int controllerInput = 0;
		if( isActiveFolder )
		{
			controllerInput = vrFrame.Input.buttonState;
			bool restrictedScrolling = FolderBrowser.GetNumFolders() > 1;
			if( restrictedScrolling )
			{
				if ( touchDirLock == VERTICAL_LOCK )
				{
					if ( ScrollMgr.IsTouchIsDown() )
					{
						// Restricted scrolling enabled, but locked to vertical scrolling so lose the touch input
						ScrollMgr.TouchUp();
					}
				}

				if ( conrollerDirLock != HORIZONTAL_LOCK )
				{
					// Restricted scrolling enabled, but not locked to horizontal scrolling so lose the controller input
					controllerInput = 0;
				}
			}
		}
		else
		{
			if( ScrollMgr.IsTouchIsDown() )
			{
				// While touch down this specific folder became inactive so perform touch up on this folder.
				ScrollMgr.TouchUp();
			}
		}

		ScrollMgr.Frame( vrFrame.DeltaSeconds, controllerInput );

    	if( numPanels <= 0 )
    	{
    		return MSG_STATUS_ALIVE;
    	}

		if ( 0 && numPanels > 1 ) // Make sure has atleast 1 panel to show wrap around indicators.
		{
			VRMenuObject * wrapIndicatorObject = menuMgr.ToObject( folder.WrapIndicatorHandle );
			if ( ScrollMgr.IsScrolling() && ScrollMgr.IsOutOfBounds() )
			{
				static const float WRAP_INDICATOR_X_OFFSET = 0.2f;
				wrapIndicatorObject->SetVisible( true );
				bool scrollingAtStart = ( ScrollMgr.GetPosition() < 0.0f );
				Vector3f position = wrapIndicatorObject->GetLocalPosition();
				if ( scrollingAtStart )
				{
					position.x = -WRAP_INDICATOR_X_OFFSET;
					UpdateWrapAroundIndicator( ScrollMgr, menuMgr, wrapIndicatorObject, position, 0.0f );
				}
				else
				{
					position.x = WRAP_INDICATOR_X_OFFSET;
					UpdateWrapAroundIndicator( ScrollMgr, menuMgr, wrapIndicatorObject, position, Mathf::Pi );
				}
			}
			else
			{
				wrapIndicatorObject->SetVisible( false );
			}
		}

		const float position = ScrollMgr.GetPosition();
		
		// Send the position to the ScrollBar
		VRMenuObject * scrollBarObject = menuMgr.ToObject( folder.ScrollBarHandle );
		OVR_ASSERT( scrollBarObject != NULL );
		if ( isActiveFolder )
		{
			bool isHidden = false;
			if ( HIDE_SCROLLBAR_UNTIL_ITEM_COUNT > 0 )
			{
				isHidden = ScrollMgr.GetMaxPosition() - (float)( HIDE_SCROLLBAR_UNTIL_ITEM_COUNT - 1 )  < Mathf::SmallestNonDenormal;
			}
			scrollBarObject->SetVisible( !isHidden );
		}
		OvrScrollBarComponent * scrollBar = scrollBarObject->GetComponentByName< OvrScrollBarComponent >();
		if ( scrollBar != NULL )
		{
			scrollBar->SetScrollFrac(  menuMgr, scrollBarObject, position );
		}

		// hide the scrollbar if not active 
		const float velocity = ScrollMgr.GetVelocity();
		if ( ( numPanels > 1 && TouchDown ) || fabsf( velocity ) >= 0.01f )
		{
			scrollBar->SetScrollState( scrollBarObject, OvrScrollBarComponent::SCROLL_STATE_FADE_IN );
		}
		else if ( !TouchDown && ( !isActiveFolder || fabsf( velocity ) < 0.01f ) ) 
		{			
			scrollBar->SetScrollState( scrollBarObject, OvrScrollBarComponent::SCROLL_STATE_FADE_OUT );
		}	

    	const float curRot = position * ( Mathf::TwoPi / FolderBrowser.GetCircumferencePanelSlots() );
		Quatf rot( UP, curRot );
		rot.Normalize();
		self->SetLocalRotation( rot );

		float alphaVal = ScrollMgr.GetWrapAroundAlphaChange();

		// show or hide panels based on current position
		//
		// for rendering, we want the switch to occur between panels - hence nearbyint
		const int curPanelIndex = nearbyintf( ScrollMgr.GetPosition() );
		const int extraPanels = FolderBrowser.GetNumSwipePanels() / 2;
		for ( int i = 0; i < numPanels; ++i )
		{
			const OvrFolderBrowser::PanelView & panel = folder.Panels.At( i );
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

			Vector4f color = panelObject->GetColor();
			color.w = alphaVal;
			panelObject->SetColor( color );
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
				return OnTouchDown_Impl( app, vrFrame, menuMgr, self, event );
    		case VRMENU_EVENT_TOUCH_UP:
				return OnTouchUp_Impl( app, vrFrame, menuMgr, self, event );
    		case VRMENU_EVENT_TOUCH_RELATIVE:
    			ScrollMgr.TouchRelative( event.FloatValue );
    			break;
    		default:
    			OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
    	}

    	return MSG_STATUS_ALIVE;
    }

	eMsgStatus OnTouchDown_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		ScrollMgr.TouchDown();
		TouchDown = true;
		return MSG_STATUS_ALIVE;
	}

	eMsgStatus OnTouchUp_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		ScrollMgr.TouchUp();
		TouchDown = false;
		return MSG_STATUS_ALIVE;
	}

private:
	OvrFolderBrowser&			FolderBrowser;
	OvrScrollManager			ScrollMgr;
	OvrFolderBrowser::FolderView *	FolderPtr;			// Correlates the folder browser component to the folder it belongs to
	bool						TouchDown;
};

//==============================
// OvrFolderBrowser
unsigned char * OvrFolderBrowser::ThumbPanelBG = NULL;

OvrFolderBrowser::OvrFolderBrowser(
		App * app,
		OvrMetaData & metaData,
		float panelWidth,
		float panelHeight,
		float radius_,
		unsigned numSwipePanels, 
		unsigned thumbWidth,
		unsigned thumbHeight )
	: VRMenu( MENU_NAME )
	, AppPtr( app )
	, ThumbnailThreadId( -1 )
	, PanelWidth( 0.0f )
	, PanelHeight( 0.0f )
	, ThumbWidth( thumbWidth )
	, ThumbHeight( thumbHeight )
	, PanelTextSpacingScale( 0.5f )
	, FolderTitleSpacingScale( 0.5f )
	, ScrollBarSpacingScale( 0.5f )
	, ScrollBarRadiusScale( 0.97f )
	, NumSwipePanels( numSwipePanels )
	, NoMedia( false )
	, AllowPanelTouchUp( false )
	, TextureCommands( 10000 )
	, BackgroundCommands( 10000 )
	, ControllerDirectionLock( NO_LOCK )
	, LastControllerInputTimeStamp( 0.0f )
	, IsTouchDownPosistionTracked( false )
	, TouchDirectionLocked( NO_LOCK )
	, MediaCount( 0 )
{
	DefaultPanelTextureIds[ 0 ] = 0;
	DefaultPanelTextureIds[ 1 ] = 0;

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

	// load up the default panel textures once
	{
		const char * panelSrc[ 2 ] = {};

		if ( ThumbWidth == ThumbHeight )
		{
			panelSrc[ 0 ] = "res/raw/panel_square.tga";
			panelSrc[ 1 ] = "res/raw/panel_hi_square.tga";
		}
		else
		{
			panelSrc[ 0 ]  = "res/raw/panel.tga";
			panelSrc[ 1 ]  = "res/raw/panel_hi.tga";
		}

		for ( int t = 0; t < 2; ++t )
		{
			int width = 0;
			int height = 0;
			DefaultPanelTextureIds[ t ] = LoadTextureFromApplicationPackage( panelSrc[ t ], TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), width, height );
			OVR_ASSERT( DefaultPanelTextureIds[ t ] && ( width == ThumbWidth ) && ( height == ThumbHeight ) );
		}
	}

	// spawn the thumbnail loading thread with the command list
	pthread_attr_t loadingThreadAttr;
	pthread_attr_init( &loadingThreadAttr );
	sched_param sparam;
	sparam.sched_priority = Thread::GetOSPriority( Thread::BelowNormalPriority );
	pthread_attr_setschedparam( &loadingThreadAttr, &sparam );

	const int createErr = pthread_create( &ThumbnailThreadId, &loadingThreadAttr, &ThumbnailThread, this );
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
	LOG( "OvrFolderBrowser::~OvrFolderBrowser" );
	BackgroundCommands.PostString( "shutDown" );

	if ( ThumbPanelBG != NULL )
	{
		free( ThumbPanelBG );
		ThumbPanelBG = NULL;
	}

	for ( int t = 0; t < 2; ++t )
	{
		glDeleteTextures( 1, &DefaultPanelTextureIds[ t ] );
		DefaultPanelTextureIds[ t ] = 0;
	}

	int numFolders = Folders.GetSizeI();
	for ( int i = 0; i < numFolders; ++i )
	{
		FolderView * folder = Folders.At( i );
		if ( folder )
		{
			delete folder;
		}
	}

	LOG( "OvrFolderBrowser::~OvrFolderBrowser COMPLETE" );
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

	// --
	// Logic for restricted scrolling
	unsigned int controllerInput = vrFrame.Input.buttonState;
	bool rightPressed 	= controllerInput & ( BUTTON_LSTICK_RIGHT | BUTTON_DPAD_RIGHT );
	bool leftPressed 	= controllerInput & ( BUTTON_LSTICK_LEFT | BUTTON_DPAD_LEFT );
	bool downPressed 	= controllerInput & ( BUTTON_LSTICK_DOWN | BUTTON_DPAD_DOWN );
	bool upPressed 		= controllerInput & ( BUTTON_LSTICK_UP | BUTTON_DPAD_UP );

	if( !( rightPressed || leftPressed || downPressed || upPressed ) )
	{
		if( ControllerDirectionLock != NO_LOCK )
		{
			if( ovr_GetTimeInSeconds() - LastControllerInputTimeStamp >= CONTROLER_COOL_DOWN )
			{
				// Didn't receive any input for last 'CONTROLER_COOL_DOWN' seconds, so release direction lock
				ControllerDirectionLock = NO_LOCK;
			}
		}
	}
	else
	{
		if( rightPressed || leftPressed )
		{
			if( ControllerDirectionLock == VERTICAL_LOCK )
			{
				rightPressed = false;
				leftPressed = false;
			}
			else
			{
				if( ControllerDirectionLock != HORIZONTAL_LOCK )
				{
					ControllerDirectionLock = HORIZONTAL_LOCK;
				}
				LastControllerInputTimeStamp = ovr_GetTimeInSeconds();
			}
		}

		if( downPressed || upPressed )
		{
			if( ControllerDirectionLock == HORIZONTAL_LOCK )
			{
				downPressed = false;
				upPressed = false;
			}
			else
			{
				if( ControllerDirectionLock != VERTICAL_LOCK )
				{
					ControllerDirectionLock = VERTICAL_LOCK;
				}
				LastControllerInputTimeStamp = ovr_GetTimeInSeconds();
			}
		}
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

void OvrFolderBrowser::OneTimeInit( const OvrMetaData & metaData )
{
	const OvrStoragePaths & storagePaths = AppPtr->GetStoragePaths();
	storagePaths.GetPathIfValidPermission( EST_PRIMARY_EXTERNAL_STORAGE, EFT_CACHE, "", W_OK, AppCachePath );
	OVR_ASSERT( !AppCachePath.IsEmpty() );

	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", ThumbSearchPaths );
	OVR_ASSERT( !ThumbSearchPaths.IsEmpty() );

	// move the root up to eye height
	OvrVRMenuMgr & menuManager = AppPtr->GetVRMenuMgr();
	BitmapFont & font = AppPtr->GetDefaultFont();
	VRMenuObject * root = menuManager.ToObject( GetRootHandle() );
	OVR_ASSERT( root );
	if ( root != NULL )
	{
		Vector3f pos = root->GetLocalPosition();
		pos.y += EYE_HEIGHT_OFFSET;
		root->SetLocalPosition( pos );
	}

	Array< VRMenuComponent* > comps;
	Array< VRMenuObjectParms const * > parms;

	// Folders root folder
	FoldersRootId = VRMenuId_t( uniqueId.Get( 1 ) );
	VRMenuObjectParms foldersRootParms(
		VRMENU_CONTAINER,
		comps,
		VRMenuSurfaceParms(),
		"Folder Browser Folders",
		Posef(),
		Vector3f( 1.0f ),
		VRMenuFontParms(),
		FoldersRootId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t()
		);
	parms.PushBack( &foldersRootParms );
	AddItems( menuManager, font, parms, GetRootHandle(), false );
	parms.Clear();
	comps.Clear();

	// Build scroll up/down hints attached to root
	VRMenuId_t scrollSuggestionRootId( uniqueId.Get( 1 ) );

	VRMenuObjectParms scrollSuggestionParms(
		VRMENU_CONTAINER,
		comps,
		VRMenuSurfaceParms(),
		"scroll hints",
		Posef(),
		Vector3f( 1.0f ),
		VRMenuFontParms(),
		scrollSuggestionRootId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t()
		);
	parms.PushBack( &scrollSuggestionParms );
	AddItems( menuManager, font, parms, GetRootHandle(), false );
	parms.Clear();

	ScrollSuggestionRootHandle = root->ChildHandleForId( menuManager, scrollSuggestionRootId );
	OVR_ASSERT( ScrollSuggestionRootHandle.IsValid() );

	VRMenuId_t suggestionDownId( uniqueId.Get( 1 ) );
	VRMenuId_t suggestionUpId( uniqueId.Get( 1 ) );

	const Posef swipeDownPose( Quatf(), FWD * ( 0.33f * Radius ) + DOWN * PanelHeight * 0.5f );
	menuHandle_t scrollDownHintHandle = OvrSwipeHintComponent::CreateSwipeSuggestionIndicator( AppPtr, this, ScrollSuggestionRootHandle, suggestionDownId.Get(),
		"res/raw/swipe_suggestion_arrow_down.png", swipeDownPose, DOWN );

	const Posef swipeUpPose( Quatf(), FWD * ( 0.33f * Radius ) + UP * PanelHeight * 0.5f );
	menuHandle_t scrollUpHintHandle = OvrSwipeHintComponent::CreateSwipeSuggestionIndicator( AppPtr, this, ScrollSuggestionRootHandle, suggestionUpId.Get(),
		"res/raw/swipe_suggestion_arrow_up.png", swipeUpPose, UP );

	OvrFolderBrowserRootComponent * rootComp = root->GetComponentById<OvrFolderBrowserRootComponent>();
	OVR_ASSERT( rootComp );

	menuHandle_t foldersRootHandle = root->ChildHandleForId( menuManager, FoldersRootId );
	OVR_ASSERT( foldersRootHandle.IsValid() );
	rootComp->SetFoldersRootHandle( foldersRootHandle );

	OVR_ASSERT( scrollUpHintHandle.IsValid() );
	rootComp->SetScrollDownHintHandle( scrollDownHintHandle );

	OVR_ASSERT( scrollDownHintHandle.IsValid() );
	rootComp->SetScrollUpHintHandle( scrollUpHintHandle );
}

void OvrFolderBrowser::BuildDirtyMenu( OvrMetaData & metaData )
{
	OvrVRMenuMgr & menuManager = AppPtr->GetVRMenuMgr();
	BitmapFont & font = AppPtr->GetDefaultFont();
	VRMenuObject * root = menuManager.ToObject( GetRootHandle() );
	OVR_ASSERT( root );

	Array< VRMenuComponent* > comps;
	Array< const VRMenuObjectParms * > parms;
	
	Array< OvrMetaData::Category > categories = metaData.GetCategories();
	const int numCategories = categories.GetSizeI();

	// load folders and position
	for ( int catIndex = 0; catIndex < numCategories; ++catIndex )
	{
		// Load a category to build swipe folder
		OvrMetaData::Category & currentCategory = metaData.GetCategory( catIndex );
		if ( currentCategory.Dirty ) // Only build if dirty
		{
			LOG( "Loading folder %i named %s", catIndex, currentCategory.CategoryTag.ToCStr() );
			FolderView * folder = GetFolderView( currentCategory.CategoryTag );

			// if building for the first time
			if ( folder == NULL )
			{
				// Create internal folder struct
				String localizedCategoryName;

				// Get localized tag (folder title)
				VrLocale::GetString( AppPtr->GetVrJni(), AppPtr->GetJavaObject(), 
					VrLocale::MakeStringIdFromANSI( currentCategory.CategoryTag ), currentCategory.CategoryTag, localizedCategoryName );

				folder = new FolderView( localizedCategoryName, currentCategory.CategoryTag );
				Folders.PushBack( folder );
				
				BuildFolder( currentCategory, folder, metaData, FoldersRootId, catIndex );

				UpdateFolderTitle( folder );
				folder->MaxRotation = CalcFolderMaxRotation( folder );
			}
			else // already have this folder - rebuild it with the new metadata
			{
				Array< const OvrMetaDatum * > folderMetaData;
				if ( metaData.GetMetaData( currentCategory, folderMetaData ) )
				{
					RebuildFolder( metaData, catIndex, folderMetaData );
				}
				else
				{
					LOG( "Failed to get any metaData for folder %i named %s", catIndex, currentCategory.CategoryTag.ToCStr() );
				}
			}

			currentCategory.Dirty = false;

			// Set up initial positions - 0 in center, the rest ascending in order below it
			MediaCount += folder->Panels.GetSizeI();
			VRMenuObject * folderObject = menuManager.ToObject( folder->Handle );
			OVR_ASSERT( folderObject != NULL );
			folderObject->SetLocalPosition( ( DOWN * PanelHeight * catIndex ) + folderObject->GetLocalPosition() );
		}
	}

	const VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 0.5f );

	// Process any thumb creation commands
	char cmd[ 1024 ];
	OVR_sprintf( cmd, 1024, "processCreates %p", &ThumbCreateAndLoadCommands );
	BackgroundCommands.PostString( cmd );

	// Show no media menu if no media found
	if ( MediaCount == 0 )
	{
		String title;
		String imageFile;
		String message;
		OnMediaNotFound( AppPtr, title, imageFile, message );

		// Create a folder if we didn't create at least one to display no media info
		if ( Folders.GetSizeI() < 1 )
		{
			const String noMediaTag( "No Media" );
			const_cast< OvrMetaData & >( metaData ).AddCategory( noMediaTag );
			OvrMetaData::Category & noMediaCategory = metaData.GetCategory( 0 );
			FolderView * noMediaView = new FolderView( noMediaTag, noMediaTag );
			BuildFolder( noMediaCategory, noMediaView, metaData, FoldersRootId, 0 );
			Folders.PushBack( noMediaView );
		}

		// Set title 
		const FolderView * folder = GetFolderView( 0 );
		OVR_ASSERT ( folder != NULL );
		VRMenuObject * folderTitleObject = menuManager.ToObject( folder->TitleHandle );
		OVR_ASSERT( folderTitleObject != NULL );
		folderTitleObject->SetText( title );
		VRMenuObjectFlags_t flags = folderTitleObject->GetFlags();
		flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
		folderTitleObject->SetFlags( flags );

		// Add no media panel
		const Vector3f dir( FWD );
		const Posef panelPose( Quatf(), dir * Radius );
		const Vector3f panelScale( 1.0f );
		const Posef textPose( Quatf(), Vector3f( 0.0f, -0.3f, 0.0f ) );

		VRMenuSurfaceParms panelSurfParms( "panelSurface",
			imageFile, SURFACE_TEXTURE_DIFFUSE,
			NULL, SURFACE_TEXTURE_MAX,
			NULL, SURFACE_TEXTURE_MAX );

		VRMenuObjectParms * p = new VRMenuObjectParms( VRMENU_STATIC, Array< VRMenuComponent* >(),
			panelSurfParms, message, panelPose, panelScale, textPose, Vector3f( 1.3f ), fontParms, VRMenuId_t(),
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

		parms.PushBack( p );
		AddItems( menuManager, font, parms, folder->SwipeHandle, true ); // PARENT: folder.TitleRootHandle
		parms.Clear();

		NoMedia = true;

		// Hide scroll hints while no media
		VRMenuObject * scrollHintRootObject = menuManager.ToObject( ScrollSuggestionRootHandle );
		OVR_ASSERT( scrollHintRootObject  );
		scrollHintRootObject->SetVisible( false );

		return;
	}

	if ( 0 ) // DISABLED UP/DOWN wrap indicators 
	{
		// Show  wrap around indicator if we have more than one non empty folder
		const FolderView * topFolder = GetFolderView( 0 );
		if( topFolder && ( ( Folders.GetSizeI() > 3 ) ||
			( Folders.GetSizeI() == 4 && !topFolder->Panels.IsEmpty() ) ) )
		{
			const char * indicatorLeftIcon = "res/raw/wrap_left.png";
			VRMenuSurfaceParms indicatorSurfaceParms( "leftSurface",
					indicatorLeftIcon, SURFACE_TEXTURE_DIFFUSE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			// Wrap around indicator - used for indicating all folders/category wrap around.
			VRMenuId_t indicatorId( uniqueId.Get( 1 ) );
			Posef indicatorPos( Quatf(), FWD * ( Radius + 0.1f ) + UP * PanelHeight * 0.0f );
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

			OvrFolderBrowserRootComponent * rootComp = root->GetComponentById<OvrFolderBrowserRootComponent>();
			OVR_ASSERT( rootComp );
			rootComp->SetFoldersWrapHandle( foldersWrapHandle );
			rootComp->SetFoldersWrapHandleTopPosition( FWD * ( 0.52f * Radius) + UP * PanelHeight * 1.0f );
			rootComp->SetFoldersWrapHandleBottomPosition( FWD * ( 0.52f * Radius) + DOWN * PanelHeight * numCategories );

			wrapIndicatorObject->SetVisible( false );
			parms.Clear();
		}
	}
}

void OvrFolderBrowser::BuildFolder( OvrMetaData::Category & category, FolderView * const folder, const OvrMetaData & metaData, VRMenuId_t foldersRootId, int folderIndex )
{
	OVR_ASSERT( folder );

	OvrVRMenuMgr & menuManager = AppPtr->GetVRMenuMgr();
	BitmapFont & font = AppPtr->GetDefaultFont();

	Array< const VRMenuObjectParms * > parms;
	const VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 1.0f );

	const int numPanels = category.DatumIndicies.GetSizeI();

	VRMenuObject * root = menuManager.ToObject( GetRootHandle() );
	menuHandle_t foldersRootHandle = root ->ChildHandleForId( menuManager, foldersRootId );

	// Create OvrFolderRootComponent for folder root
	const VRMenuId_t folderId( uniqueId.Get( 1 ) );
	LOG( "Building Folder %s id: %d with %d panels", category.CategoryTag.ToCStr(), folderId.Get(), numPanels );
	Array< VRMenuComponent* > comps;
	comps.PushBack( new OvrFolderRootComponent( *this, folder ) );
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
	AddItems( menuManager, font, parms, foldersRootHandle, false ); // PARENT: Root
	parms.Clear();

	// grab the folder handle and make sure it was created
	folder->Handle = HandleForId( menuManager, folderId );
	VRMenuObject * folderObject = menuManager.ToObject( folder->Handle );
	OVR_UNUSED( folderObject );

	// Add horizontal scrollbar to folder
	Posef scrollBarPose( Quatf(), FWD * Radius * ScrollBarRadiusScale );

	// Create unique ids for the scrollbar objects
	const VRMenuId_t scrollRootId( uniqueId.Get( 1 ) );
	const VRMenuId_t scrollXFormId( uniqueId.Get( 1 ) );
	const VRMenuId_t scrollBaseId( uniqueId.Get( 1 ) );
	const VRMenuId_t scrollThumbId( uniqueId.Get( 1 ) );

	// Set the border of the thumb image for 9-slicing
	const Vector4f scrollThumbBorder( 0.0f, 0.0f, 0.0f, 0.0f );	
	const Vector3f xFormPos = DOWN * ThumbHeight * VRMenuObject::DEFAULT_TEXEL_SCALE * ScrollBarSpacingScale;

	// Build the scrollbar
	OvrScrollBarComponent::GetScrollBarParms( *this, SCROLL_BAR_LENGTH, folderId, scrollRootId, scrollXFormId, scrollBaseId, scrollThumbId,
		scrollBarPose, Posef( Quatf(), xFormPos ), 0, numPanels, false, scrollThumbBorder, parms );	
	AddItems( menuManager, font, parms, folder->Handle, false ); // PARENT: folder->Handle
	parms.Clear();

	// Cache off the handle and verify successful creation
	folder->ScrollBarHandle = folderObject->ChildHandleForId( menuManager, scrollRootId );
	VRMenuObject * scrollBarObject = menuManager.ToObject( folder->ScrollBarHandle );
	OVR_ASSERT( scrollBarObject != NULL );
	OvrScrollBarComponent * scrollBar = scrollBarObject->GetComponentByName< OvrScrollBarComponent >();
	if ( scrollBar != NULL )
	{
		scrollBar->UpdateScrollBar( menuManager, scrollBarObject, numPanels );
		scrollBar->SetScrollFrac( menuManager, scrollBarObject, 0.0f );
		scrollBar->SetBaseColor( menuManager, scrollBarObject, Vector4f( 1.0f, 1.0f, 1.0f, 1.0f ) );

		// Hide the scrollbar
		VRMenuObjectFlags_t flags = scrollBarObject->GetFlags();
		scrollBarObject->SetFlags( flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
	}

	// Add OvrFolderSwipeComponent to folder - manages panel swiping
	VRMenuId_t swipeFolderId( uniqueId.Get( 1 ) );
	Array< VRMenuComponent* > swipeComps;
	swipeComps.PushBack( new OvrFolderSwipeComponent( *this, folder ) );
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
		LoadFolderPanels( metaData, category, folderIndex, *folder, parms );
		
		// Add panels to swipehandle
		AddItems( menuManager, font, parms, folder->SwipeHandle, false );
		DeletePointerArray( parms );
		parms.Clear();

		// Assign handles to panels
		for ( int i = 0; i < folder->Panels.GetSizeI(); ++i )
		{
			PanelView& panel = folder->Panels.At( i );
			panel.Handle = swipeObject->GetChildHandleForIndex( i );
		}
	}

	// Folder title
	VRMenuId_t folderTitleRootId( uniqueId.Get( 1 ) );
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

	VRMenuId_t folderTitleId( uniqueId.Get( 1 ) );
	Posef titlePose( Quatf(), FWD * Radius + UP * PanelHeight * FolderTitleSpacingScale );
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
	VRMenuId_t indicatorId( uniqueId.Get( 1 ) );
	Posef indicatorPos( Quatf(), FWD * ( Radius + 0.1f ) + UP * PanelHeight * 0.0f );

	const char * leftIcon = "res/raw/wrap_left.png";
	VRMenuSurfaceParms indicatorSurfaceParms( "leftSurface",
			leftIcon, SURFACE_TEXTURE_DIFFUSE,
			NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );

	VRMenuObjectParms indicatorParms(
			VRMENU_STATIC,
			Array< VRMenuComponent* >(),
			indicatorSurfaceParms,
			"wrap indicator",
			indicatorPos,
			Vector3f( 3.0f ),
			fontParms,
			indicatorId,
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) | VRMENUOBJECT_DONT_RENDER_TEXT,
			VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	parms.PushBack( &indicatorParms );

	AddItems( menuManager, font, parms, folder->TitleRootHandle, true ); // PARENT: folder->TitleRootHandle
	folder->WrapIndicatorHandle = folderTitleRootObject->ChildHandleForId( menuManager, indicatorId );
	VRMenuObject * wrapIndicatorObject = menuManager.ToObject( folder->WrapIndicatorHandle );
	OVR_UNUSED( wrapIndicatorObject );
	OVR_ASSERT( wrapIndicatorObject != NULL );

	parms.Clear();

	SetWrapIndicatorVisible( *folder, false );
}

void OvrFolderBrowser::RebuildFolder( OvrMetaData & metaData, const int folderIndex, const Array< const OvrMetaDatum * > & data )
{
	if ( folderIndex >= 0 && Folders.GetSizeI() > folderIndex )
	{		
		OvrVRMenuMgr & menuManager = AppPtr->GetVRMenuMgr();
		BitmapFont & font = AppPtr->GetDefaultFont();

		FolderView * folder = GetFolderView( folderIndex );
		if ( folder == NULL )
		{
			LOG( "OvrFolderBrowser::RebuildFolder failed to Folder for folderIndex %d", folderIndex );
			return;
		}

		VRMenuObject * swipeObject = menuManager.ToObject( folder->SwipeHandle );
		OVR_ASSERT( swipeObject );

		swipeObject->FreeChildren( menuManager );
		folder->Panels.Clear();

		const int numPanels = data.GetSizeI();
		Array< const VRMenuObjectParms * > outParms;
		Array< int > newDatumIndicies;
		for ( int panelIndex = 0; panelIndex < numPanels; ++panelIndex )
		{
			const OvrMetaDatum * panelData = data.At( panelIndex );
			if ( panelData )
			{
				AddPanelToFolder( data.At( panelIndex ), folderIndex, *folder, outParms );
				AddItems( menuManager, font, outParms, folder->SwipeHandle, false );
				DeletePointerArray( outParms );
				outParms.Clear();

				// Assign handle to panel
				PanelView& panel = folder->Panels.At( panelIndex );
				panel.Handle = swipeObject->GetChildHandleForIndex( panelIndex );

				newDatumIndicies.PushBack( panelData->Id );
			}
		}

		metaData.SetCategoryDatumIndicies( folderIndex, newDatumIndicies );

		OvrFolderSwipeComponent * swipeComp = swipeObject->GetComponentById< OvrFolderSwipeComponent >();
		OVR_ASSERT( swipeComp );		
		UpdateFolderTitle( folder );

		// Recalculate accumulated rotation in the swipe component based on ratio of where user left off before adding/removing favorites
		const float currentMaxRotation = folder->MaxRotation > 0.0f ? folder->MaxRotation : 1.0f;
		const float positionRatio = Alg::Clamp( swipeComp->GetAccumulatedRotation() / currentMaxRotation, 0.0f, 1.0f );
		folder->MaxRotation = CalcFolderMaxRotation( folder );
		swipeComp->SetAccumulatedRotation( folder->MaxRotation * positionRatio );

		// Update the scroll bar on new element count
		VRMenuObject * scrollBarObject = menuManager.ToObject( folder->ScrollBarHandle );
		if ( scrollBarObject != NULL )
		{
			OvrScrollBarComponent * scrollBar = scrollBarObject->GetComponentByName< OvrScrollBarComponent >();
			if ( scrollBar != NULL )
			{
				scrollBar->UpdateScrollBar( menuManager, scrollBarObject, numPanels );
			}
		}
	}
}

void OvrFolderBrowser::UpdateFolderTitle( const FolderView * folder )
{
	if ( folder != NULL )
	{
		const int numPanels = folder->Panels.GetSizeI();

		String folderTitle = folder->LocalizedName;
		VRMenuObject * folderTitleObject = AppPtr->GetVRMenuMgr().ToObject( folder->TitleHandle );
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
}

void * OvrFolderBrowser::ThumbnailThread( void * v )
{
	OvrFolderBrowser * folderBrowser = ( OvrFolderBrowser * )v;

	int result = pthread_setname_np( pthread_self(), "FolderBrowser" );
	if ( result != 0 )
	{
		WARN( "FolderBrowser: pthread_setname_np failed %s", strerror( result ) );
	}

	sched_param sparam;
	sparam.sched_priority = Thread::GetOSPriority( Thread::BelowNormalPriority );

	int setSchedparamResult = pthread_setschedparam( pthread_self(), SCHED_NORMAL, &sparam );
	if ( setSchedparamResult != 0 )
	{
		WARN( "FolderBrowser: pthread_setschedparam failed %s", strerror( setSchedparamResult ) );
	}

	for ( ;; )
	{
		folderBrowser->BackgroundCommands.SleepUntilMessage();
		const char * msg = folderBrowser->BackgroundCommands.GetNextMessage();
		//LOG( "BackgroundCommands: %s", msg );

		if ( MatchesHead( "shutDown", msg ) )
		{
			LOG( "OvrFolderBrowser::ThumbnailThread shutting down" );
			folderBrowser->BackgroundCommands.ClearMessages();
			break;
		}
		else if ( MatchesHead( "load ", msg ) )
		{
			int folderId = -1;
			int panelId = -1;

			sscanf( msg, "load %i %i", &folderId, &panelId );
			OVR_ASSERT( folderId >= 0 && panelId >= 0 );

			const char * fileName = strstr( msg, ":" ) + 1;

			const String fullPath( fileName );

			int		width;
			int		height;
			unsigned char * data = folderBrowser->LoadThumbnail( fileName, width, height );
			if ( data != NULL )
			{
				if ( folderBrowser->ApplyThumbAntialiasing( data, width, height ) )
				{
					folderBrowser->TextureCommands.PostPrintf( "thumb %i %i %p %i %i",
						folderId, panelId, data, width, height );
				}
			}
			else
			{
				WARN( "Thumbnail load fail for: %s", fileName );
			}
		}
		else if ( MatchesHead( "httpThumb", msg ) )
		{
			int folderId = -1;
			int panelId = -1;
			char panoUrl[ 1024 ] = {};
			char cacheDestination[ 1024 ] = {};

			sscanf( msg, "httpThumb %s %s %d %d", panoUrl, cacheDestination, &folderId, &panelId );
			OVR_ASSERT( folderId >= 0 && panelId >= 0 );

			int		width;
			int		height;
			unsigned char * data = folderBrowser->RetrieveRemoteThumbnail( panoUrl, cacheDestination, width, height );

			if ( data != NULL )
			{
				if ( folderBrowser->ApplyThumbAntialiasing( data, width, height ) )
				{
					folderBrowser->TextureCommands.PostPrintf( "thumb %i %i %p %i %i",
						folderId, panelId, data, width, height );
				}
			}
			else
			{
				WARN( "Thumbnail download fail for: %s", panoUrl );
			}
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
				unsigned char * data = folderBrowser->CreateAndCacheThumbnail( cmd.SourceImagePath, cmd.ThumbDestination.ToCStr(), width, height );

				if ( data != NULL )
				{			
					// load to panel
					const unsigned ThumbWidth = folderBrowser->GetThumbWidth();
					const unsigned ThumbHeight = folderBrowser->GetThumbHeight();

					const int numBytes = width * height * 4;
					const int thumbPanelBytes = ThumbWidth * ThumbHeight * 4;
					if ( numBytes != thumbPanelBytes )
					{
						WARN( "Thumbnail image '%s' is the wrong size! Regenerate thumbnails!", cmd.ThumbDestination.ToCStr() );
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
		}
		else
		{
			LOG( "OvrFolderBrowser::ThumbnailThread received unhandled message: %s", msg );
			OVR_ASSERT( false );
		}

		free( ( void * )msg );
	}
	LOG( "OvrFolderBrowser::ThumbnailThread returned" );
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
	
	FolderView * folder = GetFolderView( folderId );
	OVR_ASSERT( folder );

	Array<PanelView> * panels = &folder->Panels;
	OVR_ASSERT( panels );

	PanelView * panel = NULL;

	// find panel using panelId
	const int numPanels = panels->GetSizeI();
	for ( int index = 0; index < numPanels; ++index )
	{
		PanelView& currentPanel = panels->At( index );
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

void OvrFolderBrowser::LoadFolderPanels( const OvrMetaData & metaData, const OvrMetaData::Category & category, const int folderIndex, FolderView & folder,
		Array< VRMenuObjectParms const * >& outParms )
{
	// Build panels 
	Array< const OvrMetaDatum * > categoryPanos;
	metaData.GetMetaData( category, categoryPanos );
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

	OvrPanel_OnUp( VRMenu * menu, const OvrMetaDatum * panoData ) :
		VRMenuComponent_OnTouchUp(),
		Menu( menu ),
		Data( panoData )
	{
		OVR_ASSERT( Data );
	}

	void					SetData( const OvrMetaDatum * panoData )	{ Data = panoData; }

	virtual int				GetTypeId() const					{ return TYPE_ID; }
	const OvrMetaDatum *	GetData() const						{ return Data; }

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
	const OvrMetaDatum *	Data;	// Payload for this button
};

void OvrFolderBrowser::AddPanelToFolder( const OvrMetaDatum * panoData, const int folderIndex, FolderView & folder, Array< VRMenuObjectParms const * >& outParms )
{
	OVR_ASSERT( panoData );

	PanelView panel;
	const int panelIndex = folder.Panels.GetSizeI();
	panel.Id = panelIndex;
	panel.Size.x = PanelWidth;
	panel.Size.y = PanelHeight;

	String panelTitle;
	const char * panoTitle = GetPanelTitle( *panoData );
	VrLocale::GetString(  AppPtr->GetVrJni(), AppPtr->GetJavaObject(), panoTitle, panoTitle, panelTitle );

	// Load a panel
	Array< VRMenuComponent* > panelComps;
	VRMenuId_t id( uniqueId.Get( 1 ) );

	//int  folderIndexShifted = folderIndex << 24;
	VRMenuId_t buttonId( uniqueId.Get( 1 ) );

	panelComps.PushBack( new OvrPanel_OnUp( this, panoData ) );
	panelComps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.25f, Vector4f( 0.f ) ) );

	// single-pass multitexture
	VRMenuSurfaceParms panelSurfParms( "panelSurface",
		DefaultPanelTextureIds[ 0 ],
		ThumbWidth,
		ThumbHeight,
		SURFACE_TEXTURE_DIFFUSE,
		DefaultPanelTextureIds[ 1 ],
		ThumbWidth,
		ThumbHeight,
		SURFACE_TEXTURE_DIFFUSE,
		0,
		0,
		0,
		SURFACE_TEXTURE_MAX );
	
	// Panel placement - based on index which determines position within the circumference
	const float factor = ( float )panelIndex / ( float )CircumferencePanelSlots;
	Quatf rot( DOWN, ( Mathf::TwoPi * factor ) );
	Vector3f dir( FWD * rot );
	Posef panelPose( rot, dir * Radius );
	Vector3f panelScale( 1.0f );

	// Title text placed below thumbnail
	const Posef textPose( Quatf(), Vector3f( 0.0f, -PanelHeight * PanelTextSpacingScale, 0.0f ) );

	const VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 0.5f );
	VRMenuObjectParms * p = new VRMenuObjectParms( VRMENU_BUTTON, panelComps,
		panelSurfParms, panelTitle, panelPose, panelScale, textPose, Vector3f( 1.0f ), fontParms, id,
		VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	outParms.PushBack( p );
	folder.Panels.PushBack( panel );

	OVR_ASSERT( folderIndex < Folders.GetSizeI() );
	
	// Create or load thumbnail - request built up here to be processed ThumbnailThread
	const String & panoUrl = panoData->Url;
	const String thumbName = ThumbName( panoUrl );
	String finalThumb;
	char relativeThumbPath[ 1024 ];
	ToRelativePath( ThumbSearchPaths, panoUrl, relativeThumbPath, 1024 );

	char appCacheThumbPath[ 1024 ];
	OVR_sprintf( appCacheThumbPath, 1024, "%s%s", AppCachePath.ToCStr(), ThumbName( relativeThumbPath ).ToCStr() );

	// if this url doesn't exist locally
	if ( !FileExists( panoUrl ) )
	{
		// Check app cache to see if we already downloaded it
		if ( FileExists( appCacheThumbPath ) )
		{
			finalThumb = appCacheThumbPath;
		}
		else // download and cache it 
		{
			BackgroundCommands.PostPrintf( "httpThumb %s %s %d %d", panoUrl.ToCStr(), appCacheThumbPath, folderIndex, panel.Id );
			return;
		}
	}
	
	if ( finalThumb.IsEmpty() )
	{
		// Try search paths next to image for retail photos
		if ( !GetFullPath( ThumbSearchPaths, thumbName, finalThumb ) )
		{
			// Try app cache for cached user pano thumbs
			if ( FileExists( appCacheThumbPath ) )
			{
				finalThumb = appCacheThumbPath;
			}
			else
			{
				const String altThumbPath = AlternateThumbName( panoUrl );
				if ( altThumbPath.IsEmpty() || !GetFullPath( ThumbSearchPaths, altThumbPath, finalThumb ) )
				{
					int pathLen = panoUrl.GetLengthI();
					if ( pathLen > 2 && OVR_stricmp( panoUrl.ToCStr() + pathLen - 2, ".x" ) == 0 )
					{
						WARN( "Thumbnails cannot be generated from encrypted images." );
						return; // No thumb & can't create 
					}

					// Create and write out thumbnail to app cache
					finalThumb = appCacheThumbPath;

					// Create the path if necessary
					MakePath( finalThumb, S_IRUSR | S_IWUSR );

					if ( HasPermission( finalThumb, W_OK ) )
					{
						if ( FileExists( panoData->Url ) )
						{
							OvrCreateThumbCmd createCmd;
							createCmd.SourceImagePath = panoUrl;
							createCmd.ThumbDestination = finalThumb;
							char loadCmd[ 1024 ];
							OVR_sprintf( loadCmd, 1024, "load %i %i:%s", folderIndex, panel.Id, finalThumb.ToCStr() );
							createCmd.LoadCmd = loadCmd;
							ThumbCreateAndLoadCommands.PushBack( createCmd );
						}
						return;
					}
				}
			}
		}
	}

	char cmd[ 1024 ];
	OVR_sprintf( cmd, 1024, "load %i %i:%s", folderIndex, panel.Id, finalThumb.ToCStr() );
	//LOG( "Thumb cmd: %s", cmd );
	BackgroundCommands.PostString( cmd );
}

bool OvrFolderBrowser::ApplyThumbAntialiasing( unsigned char * inOutBuffer, int width, int height ) const
{
	if ( inOutBuffer != NULL )
	{
		if ( ThumbPanelBG != NULL )
		{
			const unsigned ThumbWidth = GetThumbWidth();
			const unsigned ThumbHeight = GetThumbHeight();

			const int numBytes = width * height * 4;
			const int thumbPanelBytes = ThumbWidth * ThumbHeight * 4;
			if ( numBytes != thumbPanelBytes )
			{
				WARN( "OvrFolderBrowser::ApplyAA - Thumbnail image '%s' is the wrong size!" );
			}
			else
			{
				// Apply alpha from vrlib/res/raw to alpha channel for anti-aliasing
				for ( int i = 3; i < thumbPanelBytes; i += 4 )
				{
					inOutBuffer[ i ] = ThumbPanelBG[ i ];
				}
				return true;
			}
		}
	}
	return false;
}

const OvrFolderBrowser::FolderView * OvrFolderBrowser::GetFolderView( int index ) const
{
	if ( Folders.IsEmpty() )
	{
		return NULL;
	}

	if ( index < 0 || index >= Folders.GetSizeI() )
	{
		return NULL;
	}

	return Folders.At( index );
}

OvrFolderBrowser::FolderView * OvrFolderBrowser::GetFolderView( int index )
{
	if ( Folders.IsEmpty() )
	{
		return NULL;
	}

	if ( index < 0 || index >= Folders.GetSizeI() )
	{
		return NULL;
	}

	return Folders.At( index );
}

OvrFolderBrowser::FolderView * OvrFolderBrowser::GetFolderView( const String & categoryTag )
{
	if ( Folders.IsEmpty() )
	{
		return NULL;
	}

	for ( int i = 0; i < Folders.GetSizeI(); ++i )
	{
		FolderView * currentFolder = Folders.At( i );
		if ( currentFolder->CategoryTag == categoryTag )
		{
			return currentFolder;
		}
	}

	return NULL;
}

bool OvrFolderBrowser::RotateCategory( const CategoryDirection direction )
{
	OvrFolderSwipeComponent * swipeComp = GetSwipeComponentForActiveFolder();
	return swipeComp->Rotate( direction );
}

void OvrFolderBrowser::SetCategoryRotation( const int folderIndex, const int panelIndex )
{
	OVR_ASSERT( folderIndex >= 0 && folderIndex < Folders.GetSizeI() );
	const FolderView * folder = GetFolderView( folderIndex );
	if ( folder != NULL )
	{
		VRMenuObject * swipe = AppPtr->GetVRMenuMgr().ToObject( folder->SwipeHandle );
		OVR_ASSERT( swipe );

		OvrFolderSwipeComponent * swipeComp = swipe->GetComponentById< OvrFolderSwipeComponent >();
		OVR_ASSERT( swipeComp );

		swipeComp->SetRotationByIndex( panelIndex );
	}
}

void OvrFolderBrowser::OnPanelUp( const OvrMetaDatum * data )
{
	if ( AllowPanelTouchUp )
	{
		OnPanelActivated( data );
	}
}

const OvrMetaDatum * OvrFolderBrowser::NextFileInDirectory( const int step )
{
	FolderView * folder = GetFolderView( GetActiveFolderIndex() );
	if ( folder == NULL )
	{
		return NULL;
	}

	const int numPanels = folder->Panels.GetSizeI();

	if ( numPanels == 0 )
	{
		return NULL;
	}
	
	OvrFolderSwipeComponent * swipeComp = GetSwipeComponentForActiveFolder();
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
		
	PanelView & panel = folder->Panels.At( nextPanelIndex );
	VRMenuObject * panelObject = AppPtr->GetVRMenuMgr().ToObject( panel.Handle );
	OVR_ASSERT( panelObject );

	OvrPanel_OnUp * panelUpComp = panelObject->GetComponentById<OvrPanel_OnUp>();
	OVR_ASSERT( panelUpComp );

	const OvrMetaDatum * datum = panelUpComp->GetData();
	OVR_ASSERT( datum );

	swipeComp->SetRotationByIndex( nextPanelIndex );

	return datum;
}

float OvrFolderBrowser::CalcFolderMaxRotation( const FolderView * folder ) const
{
	if ( folder == NULL )
	{
		OVR_ASSERT( false );
		return 0.0f;
	}
	int numPanels = Alg::Clamp( folder->Panels.GetSizeI() - 1, 0, INT_MAX );
	return static_cast< float >( numPanels );
}

void OvrFolderBrowser::SetWrapIndicatorVisible( FolderView& folder, const bool visible )
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

OvrFolderSwipeComponent * OvrFolderBrowser::GetSwipeComponentForActiveFolder()
{
	const FolderView * folder = GetFolderView( GetActiveFolderIndex() );
	if ( folder == NULL )
	{
		OVR_ASSERT( false );
		return NULL;
	}

	VRMenuObject * swipeObject = AppPtr->GetVRMenuMgr().ToObject( folder->SwipeHandle );
	OVR_ASSERT( swipeObject );

	OvrFolderSwipeComponent * swipeComp = swipeObject->GetComponentById<OvrFolderSwipeComponent>();
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
		const float dot = viewForwardFlat.Dot( -FWD * GetMenuPose().Orientation );
		
		if ( dot >= cosHalf )
		{
			return true;
		}
	}
	return false;
}

int OvrFolderBrowser::GetActiveFolderIndex() const
{
	VRMenuObject * rootObject = AppPtr->GetVRMenuMgr().ToObject( GetRootHandle() );
	OVR_ASSERT( rootObject );

	OvrFolderBrowserRootComponent * rootComp = rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
	OVR_ASSERT( rootComp );

	return rootComp->GetCurrentIndex( rootObject, AppPtr->GetVRMenuMgr() );
}

void OvrFolderBrowser::SetActiveFolder( int folderIdx )
{
	VRMenuObject * rootObject = AppPtr->GetVRMenuMgr().ToObject( GetRootHandle() );
	OVR_ASSERT( rootObject );

	OvrFolderBrowserRootComponent * rootComp = rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
	OVR_ASSERT( rootComp );

	rootComp->SetActiveFolder( folderIdx );
}

void OvrFolderBrowser::TouchDown()
{
	IsTouchDownPosistionTracked = false;
	TouchDirectionLocked = NO_LOCK;
}

void OvrFolderBrowser::TouchUp()
{
	IsTouchDownPosistionTracked = false;
	TouchDirectionLocked = NO_LOCK;
}

void OvrFolderBrowser::TouchRelative( Vector3f touchPos )
{
	if ( !IsTouchDownPosistionTracked )
	{
		IsTouchDownPosistionTracked = true;
		TouchDownPosistion = touchPos;
	}

	if ( TouchDirectionLocked == NO_LOCK )
	{
		float xAbsChange = fabsf( TouchDownPosistion.x - touchPos.x );
		float yAbsChange = fabsf( TouchDownPosistion.y - touchPos.y );

		if ( xAbsChange >= SCROLL_DIRECTION_DECIDING_DISTANCE || yAbsChange >= SCROLL_DIRECTION_DECIDING_DISTANCE )
		{
			if ( xAbsChange >= yAbsChange )
			{
				TouchDirectionLocked = HORIZONTAL_LOCK;
			}
			else
			{
				TouchDirectionLocked = VERTICAL_LOCK;
			}
		}
	}
}

} // namespace OVR
