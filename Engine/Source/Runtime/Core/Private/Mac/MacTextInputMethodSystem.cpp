// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "MacWindow.h"
#include "MacTextInputMethodSystem.h"
#include "CocoaTextView.h"
#include "CocoaThread.h"

DEFINE_LOG_CATEGORY_STATIC(LogMacTextInputMethodSystem, Log, All);

namespace
{
	class FTextInputMethodChangeNotifier : public ITextInputMethodChangeNotifier
	{
	public:
		FTextInputMethodChangeNotifier(const TSharedRef<ITextInputMethodContext>& InContext)
		:	Context(InContext)
		{
		}
		
		virtual ~FTextInputMethodChangeNotifier() {}
		
		void SetContextWindow(TSharedPtr<FGenericWindow> Window);
		TSharedPtr<FGenericWindow> GetContextWindow();
		
		virtual void NotifyLayoutChanged(const ELayoutChangeType ChangeType) override;
		virtual void NotifySelectionChanged() override;
		virtual void NotifyTextChanged(const uint32 BeginIndex, const uint32 OldLength, const uint32 NewLength) override;
		virtual void CancelComposition() override;
		
	private:
		TWeakPtr<ITextInputMethodContext> Context;
		TSharedPtr<FGenericWindow> ContextWindow;
	};
	
	void FTextInputMethodChangeNotifier::SetContextWindow(TSharedPtr<FGenericWindow> Window)
	{
		ContextWindow = Window;
	}
	
	TSharedPtr<FGenericWindow> FTextInputMethodChangeNotifier::GetContextWindow()
	{
		if (!ContextWindow.IsValid())
		{
			ContextWindow = Context.Pin()->GetWindow();
		}
		return ContextWindow;
	}
	
	void FTextInputMethodChangeNotifier::NotifyLayoutChanged(const ELayoutChangeType ChangeType)
	{
		if(ChangeType != ELayoutChangeType::Created && GetContextWindow().IsValid())
		{
			FCocoaWindow* CocoaWindow = (FCocoaWindow*)GetContextWindow()->GetOSWindowHandle();
			MainThreadCall(^{
				SCOPED_AUTORELEASE_POOL;
				if(CocoaWindow && [CocoaWindow openGLView])
				{
					FCocoaTextView* TextView = (FCocoaTextView*)[CocoaWindow openGLView];
					[[TextView inputContext] invalidateCharacterCoordinates];
				}
			}, UE4IMEEventMode, true);
		}
	}
	
	void FTextInputMethodChangeNotifier::NotifySelectionChanged()
	{
		if(GetContextWindow().IsValid())
		{
			FCocoaWindow* CocoaWindow = (FCocoaWindow*)GetContextWindow()->GetOSWindowHandle();
			MainThreadCall(^{
				SCOPED_AUTORELEASE_POOL;
				if(CocoaWindow && [CocoaWindow openGLView])
				{
					FCocoaTextView* TextView = (FCocoaTextView*)[CocoaWindow openGLView];
					[[TextView inputContext] invalidateCharacterCoordinates];
				}
			}, UE4IMEEventMode, true);
		}
	}
	
	void FTextInputMethodChangeNotifier::NotifyTextChanged(const uint32 BeginIndex, const uint32 OldLength, const uint32 NewLength)
	{
		if(GetContextWindow().IsValid())
		{
			FCocoaWindow* CocoaWindow = (FCocoaWindow*)GetContextWindow()->GetOSWindowHandle();
			MainThreadCall(^{
				SCOPED_AUTORELEASE_POOL;
				if(CocoaWindow && [CocoaWindow openGLView])
				{
					FCocoaTextView* TextView = (FCocoaTextView*)[CocoaWindow openGLView];
					[[TextView inputContext] invalidateCharacterCoordinates];
				}
			}, UE4IMEEventMode, true);
		}
	}
	
	void FTextInputMethodChangeNotifier::CancelComposition()
	{
		if(GetContextWindow().IsValid())
		{
			FCocoaWindow* CocoaWindow = (FCocoaWindow*)GetContextWindow()->GetOSWindowHandle();
			MainThreadCall(^{
				SCOPED_AUTORELEASE_POOL;
				if(CocoaWindow && [CocoaWindow openGLView])
				{
					FCocoaTextView* TextView = (FCocoaTextView*)[CocoaWindow openGLView];
					[[TextView inputContext] discardMarkedText];
					[TextView unmarkText];
				}
			}, UE4IMEEventMode, true);
		}
	}
}

bool FMacTextInputMethodSystem::Initialize()
{
	// Nothing to do here for Cocoa
	return true;
}

void FMacTextInputMethodSystem::Terminate()
{
	// Nothing to do here for Cocoa
}

// ITextInputMethodSystem Interface Begin
void FMacTextInputMethodSystem::ApplyDefaults(const TSharedRef<FGenericWindow>& InWindow)
{
}

TSharedPtr<ITextInputMethodChangeNotifier> FMacTextInputMethodSystem::RegisterContext(const TSharedRef<ITextInputMethodContext>& Context)
{
	TSharedRef<ITextInputMethodChangeNotifier> Notifier = MakeShareable( new FTextInputMethodChangeNotifier(Context) );
	TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap.Add(Context);
	NotifierRef = Notifier;
	return Notifier;
}

void FMacTextInputMethodSystem::UnregisterContext(const TSharedRef<ITextInputMethodContext>& Context)
{
	SCOPED_AUTORELEASE_POOL;
	TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap[Context];
	if(!NotifierRef.IsValid())
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Unregistering a context failed when its registration couldn't be found."));
		return;
	}
	
	TSharedPtr<ITextInputMethodChangeNotifier> Notifier(NotifierRef.Pin());
	FTextInputMethodChangeNotifier* MacNotifier = (FTextInputMethodChangeNotifier*)Notifier.Get();
	TSharedPtr<FGenericWindow> GenericWindow = MacNotifier->GetContextWindow();
	if(GenericWindow.IsValid())
	{
		DeactivateContext(Context);
	}
	
	ContextMap.Remove(Context);
}

void FMacTextInputMethodSystem::ActivateContext(const TSharedRef<ITextInputMethodContext>& Context)
{
	SCOPED_AUTORELEASE_POOL;
	TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap[Context];
	if(!NotifierRef.IsValid())
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Activating a context failed when its registration couldn't be found."));
		return;
	}
	
	TSharedPtr<ITextInputMethodChangeNotifier> Notifier(NotifierRef.Pin());
	FTextInputMethodChangeNotifier* MacNotifier = (FTextInputMethodChangeNotifier*)Notifier.Get();
	TSharedPtr<FGenericWindow> GenericWindow = Context->GetWindow();
	bool bActivatedContext = false;
	if(GenericWindow.IsValid())
	{
		MacNotifier->SetContextWindow(GenericWindow);
		FCocoaWindow* CocoaWindow = (FCocoaWindow*)GenericWindow->GetOSWindowHandle();
		bActivatedContext = MainThreadReturn(^{
			bool bSuccess = false;
			if(CocoaWindow && [CocoaWindow openGLView])
			{
				NSView* GLView = [CocoaWindow openGLView];
				if([GLView isKindOfClass:[FCocoaTextView class]])
				{
					FCocoaTextView* TextView = (FCocoaTextView*)GLView;
					[TextView activateInputMethod:Context];
					bSuccess = true;
				}
			}
			return bSuccess;
		}, UE4IMEEventMode);
	}
	if(!bActivatedContext)
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Activating a context failed when its window couldn't be found."));
	}
}

void FMacTextInputMethodSystem::DeactivateContext(const TSharedRef<ITextInputMethodContext>& Context)
{
	SCOPED_AUTORELEASE_POOL;
	TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap[Context];
	if(!NotifierRef.IsValid())
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Deactivating a context failed when its registration couldn't be found."));
		return;
	}
	
	TSharedPtr<ITextInputMethodChangeNotifier> Notifier(NotifierRef.Pin());
	FTextInputMethodChangeNotifier* MacNotifier = (FTextInputMethodChangeNotifier*)Notifier.Get();
	TSharedPtr<FGenericWindow> GenericWindow = MacNotifier->GetContextWindow();
	bool bDeactivatedContext = false;
	if(GenericWindow.IsValid())
	{
		FCocoaWindow* CocoaWindow = (FCocoaWindow*)GenericWindow->GetOSWindowHandle();
		bDeactivatedContext = MainThreadReturn(^{
			bool bSuccess = false;
			if(CocoaWindow && [CocoaWindow openGLView])
			{
				NSView* GLView = [CocoaWindow openGLView];
				if([GLView isKindOfClass:[FCocoaTextView class]])
				{
					FCocoaTextView* TextView = (FCocoaTextView*)GLView;
					[TextView deactivateInputMethod];
					bSuccess = true;
				}
			}
			return bSuccess;
		}, UE4IMEEventMode);
	}
	if(!bDeactivatedContext)
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Deactivating a context failed when its window couldn't be found."));
	}
}

bool FMacTextInputMethodSystem::IsActiveContext(const TSharedRef<ITextInputMethodContext>& Context) const
{
	SCOPED_AUTORELEASE_POOL;
	const TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap[Context];
	if(!NotifierRef.IsValid())
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Checking for an active context failed when its registration couldn't be found."));
		return false;
	}
	
	TSharedPtr<ITextInputMethodChangeNotifier> Notifier(NotifierRef.Pin());
	FTextInputMethodChangeNotifier* MacNotifier = (FTextInputMethodChangeNotifier*)Notifier.Get();
	TSharedPtr<FGenericWindow> GenericWindow = MacNotifier->GetContextWindow();
	if(GenericWindow.IsValid())
	{
		FCocoaWindow* CocoaWindow = (FCocoaWindow*)GenericWindow->GetOSWindowHandle();
		if(CocoaWindow && [CocoaWindow openGLView])
		{
			NSView* GLView = [CocoaWindow openGLView];
			if([GLView isKindOfClass:[FCocoaTextView class]])
			{
				FCocoaTextView* TextView = (FCocoaTextView*)GLView;
				return [TextView isActiveInputMethod:Context];
			}
		}
	}
	return false;
}
// ITextInputMethodSystem Interface End
