// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IHeadMountedDisplay.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

#if PLATFORM_WINDOWS
#define OVR_D3D
#define OVR_GL
#elif PLATFORM_MAC
#define OVR_VISION_ENABLED
#define OVR_GL
#endif

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#include <OVR_Version.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_Keys.h>
#include <OVR_CAPI_Audio.h>
#include <Extras/OVR_Math.h>

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

//////////////////////////////////////////////////////////////////////////
// A shared object that contains ovrSession and controls its life cycle.
// This class allows to recieve notifications when ovrSession is created or destroyed.
class FOvrSessionShared : public TSharedFromThis<FOvrSessionShared, ESPMode::ThreadSafe>
{
public:
	FOvrSessionShared() : Session(nullptr) {}
	~FOvrSessionShared() { Destroy(); }

	ovrResult Create(ovrGraphicsLuid& luid);
	void Destroy();

	ovrSession	Lock()
	{
		LockCnt.Increment();
		return Session;
	}

	void		Unlock()
	{
		LockCnt.Decrement();
	}

	bool		IsActive() const { return Session != nullptr; }

	struct AutoSession
	{
		AutoSession(FOvrSessionShared* InSession)
		{ 
			Session = (InSession) ? InSession->AsShared() : TSharedPtr<FOvrSessionShared, ESPMode::ThreadSafe>();
			if (Session.IsValid())
			{
				Session->Lock();
			}
		}
		AutoSession(TSharedPtr<FOvrSessionShared, ESPMode::ThreadSafe> InSession) : Session(InSession) 
		{
			if (Session.IsValid())
			{
				Session->Lock();
			}
		}
		~AutoSession() 
		{ 
			if (Session.IsValid())
			{
				Session->Unlock();
			}
		}

		ovrSession operator->() const { return (Session.IsValid()) ? Session->Session : nullptr; }
		ovrSession operator*() const { return (Session.IsValid()) ? Session->Session : nullptr; }
		operator ovrSession() const { return (Session.IsValid()) ? Session->Session : nullptr; }
	private:
		TSharedPtr<FOvrSessionShared, ESPMode::ThreadSafe> Session;
	};

	template <typename _This, typename _Func>
	FDelegateHandle AddRawCreateDelegate(_This* _this, _Func _func)
	{
		DelegatesLock.Lock();
		FDelegateHandle Handle = CreateDelegate.AddRaw(_this, _func);
		DelegatesLock.Unlock();
		return Handle;
	}
	template <typename _This, typename _Func>
	FDelegateHandle AddTSafeSPCreateDelegate(_This* _this, _Func _func)
	{
		DelegatesLock.Lock();
		FDelegateHandle Handle = CreateDelegate.AddThreadSafeSP(_this, _func);
		DelegatesLock.Unlock();
		return Handle;
	}
	void RemoveCreateDelegate(FDelegateHandle Handle)
	{
		DelegatesLock.Lock();
		CreateDelegate.Remove(Handle);
		DelegatesLock.Unlock();
	}

	template <typename _This, typename _Func>
	FDelegateHandle AddRawDestroyDelegate(_This* _this, _Func _func)
	{
		DelegatesLock.Lock();
		FDelegateHandle Handle = DestoryDelegate.AddRaw(_this, _func);
		DelegatesLock.Unlock();
		return Handle;
	}
	template <typename _This, typename _Func>
	FDelegateHandle AddTSafeSPDestroyDelegate(_This* _this, _Func _func)
	{
		DelegatesLock.Lock();
		FDelegateHandle Handle = DestoryDelegate.AddThreadSafeSP(_this, _func);
		DelegatesLock.Unlock();
		return Handle;
	}
	void RemoveDestroyDelegate(FDelegateHandle Handle)
	{
		DelegatesLock.Lock();
		DestoryDelegate.Remove(Handle);
		DelegatesLock.Unlock();
	}
private:
	ovrSession			Session;
	FThreadSafeCounter	LockCnt;
	FCriticalSection	DelegatesLock;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOvrSessionSharedDelegate, ovrSession);

	FOvrSessionSharedDelegate CreateDelegate;

	FOvrSessionSharedDelegate DestoryDelegate;

};
typedef TSharedPtr<FOvrSessionShared, ESPMode::ThreadSafe> FOvrSessionSharedPtr;

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
