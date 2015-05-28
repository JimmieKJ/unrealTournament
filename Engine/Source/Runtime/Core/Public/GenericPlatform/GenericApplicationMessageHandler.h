// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"


class FGenericWindow;


namespace EMouseButtons
{
	enum Type
	{
		Left = 0,
		Middle,
		Right,
		Thumb01,
		Thumb02,

		Invalid,
	};
}


namespace EControllerButtons
{
	enum Type
	{
		LeftAnalogY = 0,
		LeftAnalogX,
		
		RightAnalogY,
		RightAnalogX,

		LeftTriggerAnalog,
		RightTriggerAnalog,

		FaceButtonBottom,
		FaceButtonRight,
		FaceButtonLeft,
		FaceButtonTop,
		LeftShoulder,
		RightShoulder,
		SpecialLeft,
		SpecialRight,
		LeftThumb,
		RightThumb,
		LeftTriggerThreshold,
		RightTriggerThreshold,
		DPadUp,
		DPadDown,
		DPadLeft,
		DPadRight,

		LeftStickUp,
		LeftStickDown,
		LeftStickLeft,
		LeftStickRight,

		RightStickUp,
		RightStickDown,
		RightStickLeft,
		RightStickRight,

		// Steam Controller
		Touch0,
		Touch1,
		Touch2,
		Touch3,
		BackLeft,
		BackRight,

		// global speech commands
		GlobalMenu,
		GlobalView,
		GlobalPause,
		GlobalPlay,
		GlobalBack,

		AndroidBack,
		AndroidVolumeUp,
		AndroidVolumeDown,
		AndroidMenu,

		Invalid,
	};
}


namespace EWindowActivation
{
	enum Type
	{
		Activate = 0,
		ActivateByMouse,
		Deactivate
	};
}


namespace EWindowZone
{
	/**
	 * The Window Zone is the window area we are currently over to send back to the operating system
	 * for operating system compliance.
	 */
	enum Type
	{
		NotInWindow			= 0,
		TopLeftBorder		= 1,
		TopBorder			= 2,
		TopRightBorder		= 3,
		LeftBorder			= 4,
		ClientArea			= 5,
		RightBorder			= 6,
		BottomLeftBorder	= 7,
		BottomBorder		= 8,
		BottomRightBorder	= 9,
		TitleBar			= 10,
		MinimizeButton		= 11,
		MaximizeButton		= 12,
		CloseButton			= 13,
		SysMenu				= 14,

		/** No zone specified */
		Unspecified	= 0,
	};
}


namespace EWindowAction
{
	enum Type
	{
		ClickedNonClientArea	= 1,
		Maximize				= 2,
		Restore					= 3,
		WindowMenu				= 4,
	};
}


namespace EDropEffect
{
	enum Type
	{
		None   = 0,
		Copy   = 1,
		Move   = 2,
		Link   = 3,
	};
}


namespace EGestureEvent
{
	enum Type
	{
		None,
		Scroll,
		Magnify,
		Swipe,
		Rotate,
		Count
	};
}


/** Defines the minimum and maximum dimensions that a window can take on. */
struct FWindowSizeLimits
{
public:
	FWindowSizeLimits& SetMinWidth(TOptional<float> InValue){ MinWidth = InValue; return *this; }
	const TOptional<float>& GetMinWidth() const { return MinWidth; }

	FWindowSizeLimits& SetMinHeight(TOptional<float> InValue){ MinHeight = InValue; return *this; }
	const TOptional<float>& GetMinHeight() const { return MinHeight; }

	FWindowSizeLimits& SetMaxWidth(TOptional<float> InValue){ MaxWidth = InValue; return *this; }
	const TOptional<float>& GetMaxWidth() const { return MaxWidth; }

	FWindowSizeLimits& SetMaxHeight(TOptional<float> InValue){ MaxHeight = InValue; return *this; }
	const TOptional<float>& GetMaxHeight() const { return MaxHeight; }

private:
	TOptional<float> MinWidth;
	TOptional<float> MinHeight;
	TOptional<float> MaxWidth;
	TOptional<float> MaxHeight;
};


class FGenericApplicationMessageHandler
{
public:

	virtual ~FGenericApplicationMessageHandler() {}

	virtual bool ShouldProcessUserInputMessages( const TSharedPtr< FGenericWindow >& PlatformWindow ) const
	{
		return false;
	}

	virtual bool OnKeyChar( const TCHAR Character, const bool IsRepeat )
	{
		return false;
	}

	virtual bool OnKeyDown( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat ) 
	{
		return false;
	}

	virtual bool OnKeyUp( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat )
	{
		return false;
	}

	virtual bool OnMouseDown( const TSharedPtr< FGenericWindow >& Window, const EMouseButtons::Type Button )
	{
		return false;
	}

	virtual bool OnMouseUp( const EMouseButtons::Type Button )
	{
		return false;
	}

	virtual bool OnMouseDoubleClick( const TSharedPtr< FGenericWindow >& Window, const EMouseButtons::Type Button )
	{
		return false;
	}

	virtual bool OnMouseWheel( const float Delta )
	{
		return false;
	}

	virtual bool OnMouseMove()
	{
		return false;
	}

	virtual bool OnRawMouseMove( const int32 X, const int32 Y )
	{
		return false;
	}

	virtual bool OnCursorSet()
	{
		return false;
	}

	virtual bool OnControllerAnalog( EControllerButtons::Type Button, int32 ControllerId, float AnalogValue )
	{
		return false;
	}

	virtual bool OnControllerButtonPressed( EControllerButtons::Type Button, int32 ControllerId, bool IsRepeat )
	{
		return false;
	}

	virtual bool OnControllerButtonReleased( EControllerButtons::Type Button, int32 ControllerId, bool IsRepeat )
	{
		return false;
	}
    
    virtual void OnBeginGesture()
    {
    }

	virtual bool OnTouchGesture( EGestureEvent::Type GestureType, const FVector2D& Delta, float WheelDelta )
	{
		return false;
	}
    
    virtual void OnEndGesture()
    {
    }

	virtual bool OnTouchStarted( const TSharedPtr< FGenericWindow >& Window, const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
	{
		return false;
	}

	virtual bool OnTouchMoved( const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
	{
		return false;
	}

	virtual bool OnTouchEnded( const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
	{
		return false;
	}

	virtual bool OnMotionDetected( const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration, int32 ControllerId )
	{
		return false;
	}

	virtual bool OnSizeChanged( const TSharedRef< FGenericWindow >& Window, const int32 Width, const int32 Height, bool bWasMinimized = false )
	{
		return false;
	}

	virtual void OnOSPaint( const TSharedRef<FGenericWindow>& Window )
	{
	
	}

	virtual FWindowSizeLimits GetSizeLimitsForWindow( const TSharedRef<FGenericWindow>& Window ) const
	{
		return FWindowSizeLimits();
	}

	virtual void OnResizingWindow( const TSharedRef< FGenericWindow >& Window )
	{

	}

	virtual bool BeginReshapingWindow( const TSharedRef< FGenericWindow >& Window )
	{
		return true;
	}

	virtual void FinishedReshapingWindow( const TSharedRef< FGenericWindow >& Window )
	{

	}

	virtual void OnMovedWindow( const TSharedRef< FGenericWindow >& Window, const int32 X, const int32 Y )
	{

	}

	virtual bool OnWindowActivationChanged( const TSharedRef< FGenericWindow >& Window, const EWindowActivation::Type ActivationType )
	{
		return false;
	}

	virtual bool OnApplicationActivationChanged( const bool IsActive )
	{
		return false;
	}

	virtual bool OnConvertibleLaptopModeChanged()
	{
		return false;
	}

	virtual EWindowZone::Type GetWindowZoneForPoint( const TSharedRef< FGenericWindow >& Window, const int32 X, const int32 Y )
	{
		return EWindowZone::NotInWindow;
	}

	virtual void OnWindowClose( const TSharedRef< FGenericWindow >& Window )
	{

	}

	virtual EDropEffect::Type OnDragEnterText( const TSharedRef< FGenericWindow >& Window, const FString& Text )
	{
		return EDropEffect::None;
	}

	virtual EDropEffect::Type OnDragEnterFiles( const TSharedRef< FGenericWindow >& Window, const TArray< FString >& Files )
	{
		return EDropEffect::None;
	}

	virtual EDropEffect::Type OnDragOver( const TSharedPtr< FGenericWindow >& Window )
	{
		return EDropEffect::None;
	}

	virtual void OnDragLeave( const TSharedPtr< FGenericWindow >& Window )
	{

	}

	virtual EDropEffect::Type OnDragDrop( const TSharedPtr< FGenericWindow >& Window )
	{
		return EDropEffect::None;
	}

	virtual bool OnWindowAction( const TSharedRef< FGenericWindow >& Window, const EWindowAction::Type InActionType)
	{
		return true;
	}
};
