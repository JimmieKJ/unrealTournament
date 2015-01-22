// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LaunchPrivatePCH.h"
#include "ExceptionHandling.h"

#include "WinRTApplication.h"
#include "GenericApplicationMessageHandler.h"

#include <stdio.h>

#pragma warning(disable : 4946)	// reinterpret_cast used between related classes: 'Platform::Object' and ...

using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::ApplicationModel::Activation;

DEFINE_LOG_CATEGORY_STATIC(LogLaunchWinRT, Log, All);

void appWinRTEarlyInit();
int32 GuardedMain( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, int32 nCmdShow );

ref class ViewProvider sealed : public Windows::ApplicationModel::Core::IFrameworkView
{
public:
	ViewProvider();

	virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
	virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
	virtual void Load(Platform::String^ entryPoint);
	virtual void Run();
	virtual void Uninitialize();

	void appWinPumpMessages();

	void OnKeyDown(_In_ CoreWindow^ sender, _In_ KeyEventArgs^ args);
	void OnKeyUp(_In_ CoreWindow^ sender, _In_ KeyEventArgs^ args);
	void OnCharacterReceived(_In_ CoreWindow^ sender, _In_ CharacterReceivedEventArgs^ args);
	void OnVisibilityChanged(_In_ CoreWindow^ sender, _In_ VisibilityChangedEventArgs^ args);
	void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args);
	void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);

	void OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
	void OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
	void OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);

	bool IsVisible()
	{
		return bVisible;
	}

	bool WasWindowClosed()
	{
		return bWindowClosed;
	}


private:

	void OnActivated( _In_ Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, _In_ Windows::ApplicationModel::Activation::IActivatedEventArgs^ args );
	void OnResuming( _In_ Platform::Object^ sender, _In_ Platform::Object^ args );
	void OnSuspending( _In_ Platform::Object^ sender, _In_ Windows::ApplicationModel::SuspendingEventArgs^ args );

	const TCHAR* GetPointerUpdateKindString(Windows::UI::Input::PointerUpdateKind InKind);
	EMouseButtons::Type PointerUpdateKindToUEKey(Windows::UI::Input::PointerUpdateKind InKind, bool& bWasPressed);
	bool ProcessMouseEvent(Windows::UI::Core::PointerEventArgs^ args);


private:

	bool bVisible;
	bool bWindowClosed;
};

ViewProvider^ GViewProvider = nullptr;

ref class ViewProviderFactory sealed : Windows::ApplicationModel::Core::IFrameworkViewSource 
{
public:
	ViewProviderFactory() {}
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView()
	{
		GViewProvider = ref new ViewProvider();
		return GViewProvider;
	}
};

ViewProvider::ViewProvider()
	: bVisible(true)
	, bWindowClosed(false)
{
}

uint32 TranslateWinRTKey(Windows::System::VirtualKey WinRTKeyCode)
{
	uint32 TranslatedKeyCode = (uint32)WinRTKeyCode;
	switch( WinRTKeyCode )
	{
	case Windows::System::VirtualKey::LeftButton:	TranslatedKeyCode = VK_LBUTTON;		break;
	case Windows::System::VirtualKey::RightButton:	TranslatedKeyCode = VK_RBUTTON;		break;
	case Windows::System::VirtualKey::MiddleButton:	TranslatedKeyCode = VK_MBUTTON;		break;

	case Windows::System::VirtualKey::XButton1:		TranslatedKeyCode = VK_XBUTTON1;	break;
	case Windows::System::VirtualKey::XButton2:		TranslatedKeyCode = VK_XBUTTON2;	break;

	case Windows::System::VirtualKey::Back:			TranslatedKeyCode = VK_BACK;		break;
	case Windows::System::VirtualKey::Tab:			TranslatedKeyCode = VK_TAB;			break;
	case Windows::System::VirtualKey::Enter:		TranslatedKeyCode = VK_RETURN;		break;
	case Windows::System::VirtualKey::Pause:		TranslatedKeyCode = VK_PAUSE;		break;

	case Windows::System::VirtualKey::CapitalLock:	TranslatedKeyCode = VK_CAPITAL;		break;
	case Windows::System::VirtualKey::Escape:		TranslatedKeyCode = VK_ESCAPE;		break;
	case Windows::System::VirtualKey::Space:		TranslatedKeyCode = VK_SPACE;		break;
	//case Windows::System::VirtualKey::Tab:		TranslatedKeyCode = VK_PRIOR;		break;
	//case Windows::System::VirtualKey::Tab:		TranslatedKeyCode = VK_NEXT;		break;
	case Windows::System::VirtualKey::End:			TranslatedKeyCode = VK_END;			break;
	case Windows::System::VirtualKey::Home:			TranslatedKeyCode = VK_HOME;		break;

	case Windows::System::VirtualKey::Left:			TranslatedKeyCode = VK_LEFT;		break;
	case Windows::System::VirtualKey::Up:			TranslatedKeyCode = VK_UP;			break;
	case Windows::System::VirtualKey::Right:		TranslatedKeyCode = VK_RIGHT;		break;
	case Windows::System::VirtualKey::Down:			TranslatedKeyCode = VK_DOWN;		break;

	case Windows::System::VirtualKey::Insert:		TranslatedKeyCode = VK_INSERT;		break;
	case Windows::System::VirtualKey::Delete:		TranslatedKeyCode = VK_DELETE;		break;

	// VK_0 - VK_9 are the same as ASCII '0' - '9' (0x30 - 0x39)
	// 0x40 : unassigned
	// VK_A - VK_Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
	case Windows::System::VirtualKey::Number0:		TranslatedKeyCode = 0x30;			break;
	case Windows::System::VirtualKey::Number1:		TranslatedKeyCode = 0x31;			break;
	case Windows::System::VirtualKey::Number2:		TranslatedKeyCode = 0x32;			break;
	case Windows::System::VirtualKey::Number3:		TranslatedKeyCode = 0x33;			break;
	case Windows::System::VirtualKey::Number4:		TranslatedKeyCode = 0x34;			break;
	case Windows::System::VirtualKey::Number5:		TranslatedKeyCode = 0x35;			break;
	case Windows::System::VirtualKey::Number6:		TranslatedKeyCode = 0x36;			break;
	case Windows::System::VirtualKey::Number7:		TranslatedKeyCode = 0x37;			break;
	case Windows::System::VirtualKey::Number8:		TranslatedKeyCode = 0x38;			break;
	case Windows::System::VirtualKey::Number9:		TranslatedKeyCode = 0x39;			break;
	case Windows::System::VirtualKey::A:			TranslatedKeyCode = 0x41;			break;
	case Windows::System::VirtualKey::B:			TranslatedKeyCode = 0x42;			break;
	case Windows::System::VirtualKey::C:			TranslatedKeyCode = 0x43;			break;
	case Windows::System::VirtualKey::D:			TranslatedKeyCode = 0x44;			break;
	case Windows::System::VirtualKey::E:			TranslatedKeyCode = 0x45;			break;
	case Windows::System::VirtualKey::F:			TranslatedKeyCode = 0x46;			break;
	case Windows::System::VirtualKey::G:			TranslatedKeyCode = 0x47;			break;
	case Windows::System::VirtualKey::H:			TranslatedKeyCode = 0x48;			break;
	case Windows::System::VirtualKey::I:			TranslatedKeyCode = 0x49;			break;
	case Windows::System::VirtualKey::J:			TranslatedKeyCode = 0x4A;			break;
	case Windows::System::VirtualKey::K:			TranslatedKeyCode = 0x4B;			break;
	case Windows::System::VirtualKey::L:			TranslatedKeyCode = 0x4C;			break;
	case Windows::System::VirtualKey::M:			TranslatedKeyCode = 0x4D;			break;
	case Windows::System::VirtualKey::N:			TranslatedKeyCode = 0x4E;			break;
	case Windows::System::VirtualKey::O:			TranslatedKeyCode = 0x4F;			break;
	case Windows::System::VirtualKey::P:			TranslatedKeyCode = 0x50;			break;
	case Windows::System::VirtualKey::Q:			TranslatedKeyCode = 0x51;			break;
	case Windows::System::VirtualKey::R:			TranslatedKeyCode = 0x52;			break;
	case Windows::System::VirtualKey::S:			TranslatedKeyCode = 0x53;			break;
	case Windows::System::VirtualKey::T:			TranslatedKeyCode = 0x54;			break;
	case Windows::System::VirtualKey::U:			TranslatedKeyCode = 0x55;			break;
	case Windows::System::VirtualKey::V:			TranslatedKeyCode = 0x56;			break;
	case Windows::System::VirtualKey::W:			TranslatedKeyCode = 0x57;			break;
	case Windows::System::VirtualKey::X:			TranslatedKeyCode = 0x58;			break;
	case Windows::System::VirtualKey::Y:			TranslatedKeyCode = 0x59;			break;
	case Windows::System::VirtualKey::Z:			TranslatedKeyCode = 0x5A;			break;

	case Windows::System::VirtualKey::NumberPad0:	TranslatedKeyCode = VK_NUMPAD0;		break;
	case Windows::System::VirtualKey::NumberPad1:	TranslatedKeyCode = VK_NUMPAD1;		break;
	case Windows::System::VirtualKey::NumberPad2:	TranslatedKeyCode = VK_NUMPAD2;		break;
	case Windows::System::VirtualKey::NumberPad3:	TranslatedKeyCode = VK_NUMPAD3;		break;
	case Windows::System::VirtualKey::NumberPad4:	TranslatedKeyCode = VK_NUMPAD4;		break;
	case Windows::System::VirtualKey::NumberPad5:	TranslatedKeyCode = VK_NUMPAD5;		break;
	case Windows::System::VirtualKey::NumberPad6:	TranslatedKeyCode = VK_NUMPAD6;		break;
	case Windows::System::VirtualKey::NumberPad7:	TranslatedKeyCode = VK_NUMPAD7;		break;
	case Windows::System::VirtualKey::NumberPad8:	TranslatedKeyCode = VK_NUMPAD8;		break;
	case Windows::System::VirtualKey::NumberPad9:	TranslatedKeyCode = VK_NUMPAD9;		break;

	case Windows::System::VirtualKey::Multiply:		TranslatedKeyCode = VK_MULTIPLY;	break;
	case Windows::System::VirtualKey::Add:			TranslatedKeyCode = VK_ADD;			break;
	case Windows::System::VirtualKey::Subtract:		TranslatedKeyCode = VK_SUBTRACT;	break;
	case Windows::System::VirtualKey::Decimal:		TranslatedKeyCode = VK_DECIMAL;		break;
	case Windows::System::VirtualKey::Divide:		TranslatedKeyCode = VK_DIVIDE;		break;

	case Windows::System::VirtualKey::F1:			TranslatedKeyCode = VK_F1;			break;
	case Windows::System::VirtualKey::F2:			TranslatedKeyCode = VK_F2;			break;
	case Windows::System::VirtualKey::F3:			TranslatedKeyCode = VK_F3;			break;
	case Windows::System::VirtualKey::F4:			TranslatedKeyCode = VK_F4;			break;
	case Windows::System::VirtualKey::F5:			TranslatedKeyCode = VK_F5;			break;
	case Windows::System::VirtualKey::F6:			TranslatedKeyCode = VK_F6;			break;
	case Windows::System::VirtualKey::F7:			TranslatedKeyCode = VK_F7;			break;
	case Windows::System::VirtualKey::F8:			TranslatedKeyCode = VK_F8;			break;
	case Windows::System::VirtualKey::F9:			TranslatedKeyCode = VK_F9;			break;
	case Windows::System::VirtualKey::F10:			TranslatedKeyCode = VK_F10;			break;
	case Windows::System::VirtualKey::F11:			TranslatedKeyCode = VK_F11;			break;
	case Windows::System::VirtualKey::F12:			TranslatedKeyCode = VK_F12;			break;

	case Windows::System::VirtualKey::NumberKeyLock:TranslatedKeyCode = VK_NUMLOCK;		break;

	case Windows::System::VirtualKey::Scroll:		TranslatedKeyCode = VK_SCROLL;		break;

	case Windows::System::VirtualKey::LeftShift:	TranslatedKeyCode = VK_LSHIFT;		break;
	case Windows::System::VirtualKey::RightShift:	TranslatedKeyCode = VK_RSHIFT;		break;
	case Windows::System::VirtualKey::LeftControl:	TranslatedKeyCode = VK_LCONTROL;	break;
	case Windows::System::VirtualKey::RightControl:	TranslatedKeyCode = VK_RCONTROL;	break;
	case Windows::System::VirtualKey::LeftMenu:		TranslatedKeyCode = VK_LMENU;		break;
	case Windows::System::VirtualKey::RightMenu:	TranslatedKeyCode = VK_RMENU;		break;
	}
	return TranslatedKeyCode;
}


extern void appWinRTKeyEvent(uint32 InCode, bool bInWasPressed);
extern void appWinRTCharEvent(uint32 InCode, bool bInWasPressed);

void ViewProvider::OnKeyDown(
	_In_ CoreWindow^ sender,
	_In_ KeyEventArgs^ args 
	)
{
	uint32 Code = TranslateWinRTKey(args->VirtualKey);
	if ((Code & 0xFFFFFF00) == 0)
	{
		//appOutputDebugStringf(TEXT("KEYDOWN: VirtualKey = 0x%08x    Status = 0x%08x\n"), args->VirtualKey, args->KeyStatus);
		appWinRTKeyEvent(Code, true);
	}
}
void ViewProvider::OnKeyUp(
	_In_ CoreWindow^ sender,
	_In_ KeyEventArgs^ args 
	)
{
	uint32 Code = TranslateWinRTKey(args->VirtualKey);
	if ((Code & 0xFFFFFF00) == 0)
	{
		//appOutputDebugStringf(TEXT("KEYUP  : VirtualKey = 0x%08x    Status = 0x%08x\n"), args->VirtualKey, args->KeyStatus);
		appWinRTKeyEvent(Code, false);
	}
}

void ViewProvider::OnCharacterReceived(
	_In_ CoreWindow^ sender,
	_In_ CharacterReceivedEventArgs^ args)
{
// 	appOutputDebugStringf(TEXT("CHAR   : KeyCode    = 0x%08x    IsExtendedKey = %d    IsKeyReleased = %d    IsMenuKeyDown = %d    RepeatCount = %3d    ScanCode = 0x%08x    WasKeyDown = %d\n"),
// 		args->KeyCode, 
// 		args->KeyStatus.IsExtendedKey ? 1 : 0,
// 		args->KeyStatus.IsKeyReleased ? 1 : 0,
// 		args->KeyStatus.IsMenuKeyDown ? 1 : 0,		// Doesn't work
// 		args->KeyStatus.RepeatCount,				// Doesn't work
// 		args->KeyStatus.ScanCode,					// Doesn't work
// 		args->KeyStatus.WasKeyDown ? 1 : 0);

	if (args->KeyStatus.IsExtendedKey == false)
	{
		appWinRTCharEvent(args->KeyCode, true);
	}
}

void ViewProvider::OnVisibilityChanged(_In_ CoreWindow^ sender, _In_ VisibilityChangedEventArgs^ args)
{
	bVisible = args->Visible;
}

void ViewProvider::OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OnWindowSizeChanged CALLED\n"));
}

void ViewProvider::OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args)
{
	bWindowClosed = true;
}

void ViewProvider::OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	ProcessMouseEvent(args);

	Windows::UI::Input::PointerPoint^ Point = args->CurrentPoint;
	Windows::UI::Input::PointerUpdateKind Kind = Point->Properties->PointerUpdateKind;

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OnPointerPressed : %5.2f, %5.2f - %s\n"),
		Point->Position.X, Point->Position.Y, GetPointerUpdateKindString(Kind));
}

void ViewProvider::OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	ProcessMouseEvent(args);

	Windows::UI::Input::PointerPoint^ Point = args->CurrentPoint;
	Windows::UI::Input::PointerUpdateKind Kind = Point->Properties->PointerUpdateKind;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OnPointerReleased: %5.2f, %5.2f - %s\n"),
		Point->Position.X, Point->Position.Y, GetPointerUpdateKindString(Kind));
}

void ViewProvider::OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	Windows::UI::Input::PointerPoint^ Point = args->CurrentPoint;

	FWinRTApplication* const Application = FWinRTApplication::GetWinRTApplication();

	if ( Application != NULL )
	{
		const FVector2D CurrentCursorPosition(Point->Position.X, Point->Position.Y);
		Application->GetCursor()->UpdatePosition( CurrentCursorPosition );
		Application->GetMessageHandler()->OnMouseMove();
	}

	Windows::UI::Input::PointerUpdateKind Kind = Point->Properties->PointerUpdateKind;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OnPointerMoved   : %5.2f, %5.2f - %s\n"),
		Point->Position.X, Point->Position.Y, GetPointerUpdateKindString(Kind));
}

void ViewProvider::appWinPumpMessages()
{
	CoreDispatcher^ disp = CoreWindow::GetForCurrentThread()->Dispatcher;
	if (disp != nullptr)
	{
		disp->ProcessEvents(CoreProcessEventsOption::ProcessOneIfPresent);
	}
}

void appWinPumpMessages()
{
	GViewProvider->appWinPumpMessages();
}

void ViewProvider::Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView)
{
	applicationView->Activated += ref new Windows::Foundation::TypedEventHandler< CoreApplicationView^, IActivatedEventArgs^ >( this, &ViewProvider::OnActivated );
	CoreApplication::Suspending += ref new Windows::Foundation::EventHandler< Windows::ApplicationModel::SuspendingEventArgs^ >( this, &ViewProvider::OnSuspending );
	CoreApplication::Resuming += ref new Windows::Foundation::EventHandler< Platform::Object^>( this, & ViewProvider::OnResuming );

	appWinRTEarlyInit();
}

void ViewProvider::OnActivated(_In_ Windows::ApplicationModel::Core::CoreApplicationView^ /*applicationView*/, _In_ Windows::ApplicationModel::Activation::IActivatedEventArgs^ args)
{
	// Query this to find out if the application was shut down gracefully last time
	ApplicationExecutionState lastState = args->PreviousExecutionState;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OnActivated: Last application state: %d\n"), lastState);

	CoreWindow::GetForCurrentThread()->Activate();
}

void ViewProvider::OnResuming(_In_ Platform::Object^ /*sender*/, _In_ Platform::Object^ /*args*/)
{
//	reinterpret_cast< SampleFramework* >( m_renderer )->FrameworkResume();
}

void ViewProvider::OnSuspending(_In_ Platform::Object^ /*sender*/, _In_ Windows::ApplicationModel::SuspendingEventArgs^ /*args*/)
{
//	reinterpret_cast< SampleFramework* >( m_renderer )->FrameworkSuspend();
}

const TCHAR* ViewProvider::GetPointerUpdateKindString(Windows::UI::Input::PointerUpdateKind InKind)
{
	switch (InKind)
	{
	case Windows::UI::Input::PointerUpdateKind::Other:
		return TEXT("Other");
	case Windows::UI::Input::PointerUpdateKind::LeftButtonPressed:
		return TEXT("LeftButtonPressed");
	case Windows::UI::Input::PointerUpdateKind::LeftButtonReleased:
		return TEXT("LeftButtonReleased");
	case Windows::UI::Input::PointerUpdateKind::RightButtonPressed:
		return TEXT("RightButtonPressed");
	case Windows::UI::Input::PointerUpdateKind::RightButtonReleased:
		return TEXT("RightButtonReleased");
	case Windows::UI::Input::PointerUpdateKind::MiddleButtonPressed:
		return TEXT("MiddleButtonPressed");
	case Windows::UI::Input::PointerUpdateKind::MiddleButtonReleased:
		return TEXT("MiddleButtonReleased");
	case Windows::UI::Input::PointerUpdateKind::XButton1Pressed:
		return TEXT("XButton1Pressed");
	case Windows::UI::Input::PointerUpdateKind::XButton1Released:
		return TEXT("XButton1Released");
	case Windows::UI::Input::PointerUpdateKind::XButton2Pressed:
		return TEXT("XButton2Pressed");
	case Windows::UI::Input::PointerUpdateKind::XButton2Released:
		return TEXT("XButton2Released");
	}

	return TEXT("*** UNKNOWN ***");
}

EMouseButtons::Type ViewProvider::PointerUpdateKindToUEKey(Windows::UI::Input::PointerUpdateKind InKind, bool& bWasPressed)
{
	switch (InKind)
	{
	case Windows::UI::Input::PointerUpdateKind::Other:
		return EMouseButtons::Left;
	case Windows::UI::Input::PointerUpdateKind::LeftButtonPressed:
		bWasPressed = true;
		return EMouseButtons::Left;
	case Windows::UI::Input::PointerUpdateKind::LeftButtonReleased:
		bWasPressed = false;
		return EMouseButtons::Left;
	case Windows::UI::Input::PointerUpdateKind::RightButtonPressed:
		bWasPressed = true;
		return EMouseButtons::Right;
	case Windows::UI::Input::PointerUpdateKind::RightButtonReleased:
		bWasPressed = false;
		return EMouseButtons::Right;
	case Windows::UI::Input::PointerUpdateKind::MiddleButtonPressed:
		bWasPressed = true;
		return EMouseButtons::Middle;
	case Windows::UI::Input::PointerUpdateKind::MiddleButtonReleased:
		bWasPressed = false;
		return EMouseButtons::Middle;
	case Windows::UI::Input::PointerUpdateKind::XButton1Pressed:
		bWasPressed = true;
		return EMouseButtons::Thumb01;
	case Windows::UI::Input::PointerUpdateKind::XButton1Released:
		bWasPressed = false;
		return EMouseButtons::Thumb01;
	case Windows::UI::Input::PointerUpdateKind::XButton2Pressed:
		bWasPressed = true;
		return EMouseButtons::Thumb02;
	case Windows::UI::Input::PointerUpdateKind::XButton2Released:
		bWasPressed = false;
		return EMouseButtons::Thumb02;
	}

	return EMouseButtons::Left;
}

bool ViewProvider::ProcessMouseEvent(Windows::UI::Core::PointerEventArgs^ args)
{
	Windows::UI::Input::PointerPoint^ Point = args->CurrentPoint;
//	args->KeyModifiers.Control
	Windows::UI::Input::PointerUpdateKind Kind = Point->Properties->PointerUpdateKind;

	FWinRTApplication* const Application = FWinRTApplication::GetWinRTApplication();

	if ( Application != NULL )
	{
		const FVector2D CurrentCursorPosition(Point->Position.X, Point->Position.Y);
		Application->GetCursor()->UpdatePosition( CurrentCursorPosition );

		bool bPressed = false;
		EMouseButtons::Type MouseButton = PointerUpdateKindToUEKey(Kind, bPressed);

		if (bPressed == true)
		{
			Application->GetMessageHandler()->OnMouseDown( NULL, MouseButton );
		}
		else
		{
			Application->GetMessageHandler()->OnMouseUp( MouseButton );
		}
	}

	return true;
}

void ViewProvider::SetWindow(Windows::UI::Core::CoreWindow^ window)
{
	window->KeyDown += ref new Windows::Foundation::TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &ViewProvider::OnKeyDown);
	window->KeyUp += ref new Windows::Foundation::TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &ViewProvider::OnKeyUp);
	window->CharacterReceived += ref new Windows::Foundation::TypedEventHandler<CoreWindow^, CharacterReceivedEventArgs^>(this, &ViewProvider::OnCharacterReceived);
	window->VisibilityChanged += ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &ViewProvider::OnVisibilityChanged);
	window->SizeChanged += ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &ViewProvider::OnWindowSizeChanged);
	window->Closed += ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &ViewProvider::OnWindowClosed);
	window->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);
	window->PointerPressed += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &ViewProvider::OnPointerPressed);
	window->PointerReleased += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &ViewProvider::OnPointerReleased);
	window->PointerMoved += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &ViewProvider::OnPointerMoved);
}

// this method is called after Initialize
void ViewProvider::Load(Platform::String^ /*entryPoint*/)
{
}

// this method is called after Load
void ViewProvider::Run()
{
	if (FCommandLine::Get()[0] == 0)
	{
		const LONG MaxCmdLineSize = 65536;
		char AnsiCmdLine[MaxCmdLineSize] = {0};

		// Default command-line - just in case
		FCStringAnsi::Strcpy(AnsiCmdLine, MaxCmdLineSize, "UDKGame.exe UDK DM-Deck?fixedplayerstart=1");

		//@todo.WinRT: Remove this once commandline parameters work correctly
		FILE* stream;
		if (fopen_s(&stream, "WinRTCmdLine.txt", "r+t") == 0)
		{
			// Seek to the end
			fseek(stream, 0, SEEK_END);
			LONG TotalSize = ftell(stream);
			// Return to the start
			fseek(stream, 0, SEEK_SET);

			if (TotalSize < MaxCmdLineSize)
			{
				fread(AnsiCmdLine, sizeof(char), TotalSize, stream);
			}
			fclose(stream);
		}

		TCHAR CmdLine[MaxCmdLineSize] = {0};
		FCString::Strcpy(CmdLine, ANSI_TO_TCHAR(AnsiCmdLine));
		while (CmdLine[FCString::Strlen(CmdLine) - 1] == TEXT('\n'))
		{
			CmdLine[FCString::Strlen(CmdLine) - 1] = 0;
		}
		// We have to do this here for WinRT due to the pre-init needing the BaseDir...
		FCommandLine::Set(CmdLine); 
	}
	TCHAR TempCmdLine[16384];
	FCString::Strncpy(TempCmdLine, FCommandLine::Get(), 16384);

	Platform::String^ LocationPath = Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LocationPath = %s\n"), LocationPath->Data());

	GuardedMain(TempCmdLine, NULL, NULL, 0 );

	Windows::ApplicationModel::Core::CoreApplication::Exit();
}

void ViewProvider::Uninitialize()
{
}

[Platform::MTAThread]                                                                           
int main(Platform::Array<Platform::String^>^)
{
	auto viewProviderFactory = ref new ViewProviderFactory();
	Windows::ApplicationModel::Core::CoreApplication::Run(viewProviderFactory);
}

// The following functions are also located in Launch.cpp in a #if _WINDOWS block...
/**
 * Handler for CRT parameter validation. Triggers error
 *
 * @param Expression - the expression that failed crt validation
 * @param Function - function which failed crt validation
 * @param File - file where failure occured
 * @param Line - line number of failure
 * @param Reserved - not used
 */
void InvalidParameterHandler(const TCHAR* Expression,
							 const TCHAR* Function, 
							 const TCHAR* File, 
							 uint32 Line, 
							 uintptr_t Reserved)
{
	UE_LOG(LogLaunchWinRT, Fatal,
		TEXT("SECURE CRT: Invalid parameter detected.\nExpression: %s Function: %s. File: %s Line: %d\n"), 
		Expression ? Expression : TEXT("Unknown"), 
		Function ? Function : TEXT("Unknown"), 
		File ? File : TEXT("Unknown"), 
		Line );
}

/**
 * Setup the common debug settings 
 */
void SetupWindowsEnvironment( void )
{
	// all crt validation should trigger the callback
	_set_invalid_parameter_handler(InvalidParameterHandler);
}

void appWinRTEarlyInit()
{
	// Setup common Windows settings
	SetupWindowsEnvironment();

	int32 ErrorLevel			= 0;
}

/** The global EngineLoop instance */
FEngineLoop	GEngineLoop;

/** 
 * PreInits the engine loop 
 */
int32 EnginePreInit( const TCHAR* CmdLine )
{
	return GEngineLoop.PreInit( CmdLine );
}

/** 
 * Inits the engine loop 
 */
int32 EngineInit( const TCHAR* SplashName )
{
	return GEngineLoop.Init();
}

/** 
 * Ticks the engine loop 
 */
void EngineTick( void )
{
	if (GViewProvider->IsVisible() == true)
	{
		GEngineLoop.Tick();
		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
	}
	else
	{
		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
	}
}

/**
 * Shuts down the engine
 */
void EngineExit( void )
{
	// Make sure this is set
	GIsRequestingExit = true;
	GEngineLoop.Exit();
}

/**
 * Static guarded main function. Rolled into own function so we can have error handling for debug/ release builds depending
 * on whether a debugger is attached or not.
 */
int32 GuardedMain( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, int32 nCmdShow )
{
	// make sure GEngineLoop::Exit() is always called.
	struct EngineLoopCleanupGuard 
	{ 
		~EngineLoopCleanupGuard()
		{
			EngineExit();
		}
	} CleanupGuard;

	// Set up minidump filename. We cannot do this directly inside main as we use an FString that requires 
	// destruction and main uses SEH.
	// These names will be updated as soon as the Filemanager is set up so we can write to the log file.
	// That will also use the user folder for installed builds so we don't write into program files or whatever.
	FCString::Strcpy(MiniDumpFilenameW, *FString::Printf( TEXT("unreal-v%i-%s.dmp"), GEngineVersion.GetChangelist(), *FDateTime::Now().ToString()));
	CmdLine = FCommandLine::RemoveExeName(CmdLine);

	// Call PreInit and exit if failed.
	int32 ErrorLevel = EnginePreInit(CmdLine);
	if ((ErrorLevel != 0) || GIsRequestingExit)
	{
		return ErrorLevel;
	}

	// Game without wxWindows.
	ErrorLevel = EngineInit(TEXT("PC/Splash.bmp"));
	while (!GIsRequestingExit)
	{
		EngineTick();
	}

	return ErrorLevel;
}
