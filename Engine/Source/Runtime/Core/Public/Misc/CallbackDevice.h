// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScopedCallback.h"


class AActor;


/**
 * The list of events that can be fired from the engine to unknown listeners.
 */
enum ECallbackQueryType
{
	CALLBACK_ModalErrorMessage,				//Sent when FMessageDialog::Open or appDebugMesgf need to display a modal
	CALLBACK_LocalizationExportFilter,		// A query sent to find out if the provided object passes the localization export filter
	CALLBACK_QueryCount,
};


// delegates for hotfixes
namespace EHotfixDelegates
{
	enum Type
	{
		Test,
	};
}


// this is an example of a hotfix arg and return value structure. Once we have other examples, it can be deleted.
struct FTestHotFixPayload
{
	FString Message;
	bool ValueToReturn;
	bool Result;
};

// Parameters passed to CrashOverrideParamsChanged used to customize crash report client behavior/appearance
struct FCrashOverrideParameters
{
	FString CrashReportClientMessageText;
};


class CORE_API FCoreDelegates
{
public:
	//hot fix delegate
	DECLARE_DELEGATE_TwoParams(FHotFixDelegate, void *, int32);

	// Callback for object property modifications
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnActorLabelChanged, AActor*);

	// delegate type for prompting the pak system to mount a new pak
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnMountPak, const FString&, uint32, IPlatformFile::FDirectoryVisitor*);

	// delegate type for prompting the pak system to mount a new pak
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnUnmountPak, const FString&);

	/** delegate type for opening a modal message box ( Params: EAppMsgType::Type MessageType, const FText& Text, const FText& Title ) */
	DECLARE_DELEGATE_RetVal_ThreeParams(EAppReturnType::Type, FOnModalMessageBox, EAppMsgType::Type, const FText&, const FText&);

	// Callback for PER_MODULE_BOILERPLATE macro's GObjectArrayForDebugVisualizers
	DECLARE_DELEGATE_RetVal(class FFixedUObjectArray*, FObjectArrayForDebugVisualizersDelegate);

	// Called in PER_MODULE_BOILERPLATE macro.
	static FObjectArrayForDebugVisualizersDelegate& GetObjectArrayForDebugVisualizersDelegate();

	// Callback for handling an ensure
	DECLARE_MULTICAST_DELEGATE(FOnHandleSystemEnsure);

	// Callback for handling an error
	DECLARE_MULTICAST_DELEGATE(FOnHandleSystemError);

	// Callback for handling user login/logout.  first int is UserID, second int is UserIndex
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUserLoginChangedEvent, bool, int32, int32);

	// Callback for handling safe frame area size changes
	DECLARE_MULTICAST_DELEGATE(FOnSafeFrameChangedEvent);

	// Callback for handling accepting invitations - generally for engine code
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInviteAccepted, const FString&, const FString&);

	// Callback for handling the Controller connection / disconnection
	// first param is true for a connection, false for a disconnection.
	// second param is UserID, third is UserIndex / ControllerId.
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUserControllerConnectionChange, bool, int32, int32);

	// Callback for platform handling when flushing async loads.
	DECLARE_MULTICAST_DELEGATE(FOnAsyncLoadingFlush);
	static FOnAsyncLoadingFlush OnAsyncLoadingFlush;

	// get a hotfix delegate
	static FHotFixDelegate& GetHotfixDelegate(EHotfixDelegates::Type HotFix);

	// Callback when a user logs in/out of the platform.
	static FOnUserLoginChangedEvent OnUserLoginChangedEvent;

	// Callback when controllers disconnected / reconnected
	static FOnUserControllerConnectionChange OnControllerConnectionChange;

	// Callback when a user changes the safe frame size
	static FOnSafeFrameChangedEvent OnSafeFrameChangedEvent;

	// Callback for mounting a new pak file.
	static FOnMountPak OnMountPak;

	// Callback for unmounting a pak file.
	static FOnUnmountPak OnUnmountPak;

	// Callback when an ensure has occurred
	static FOnHandleSystemEnsure OnHandleSystemEnsure;
	// Callback when an error (crash) has occurred
	static FOnHandleSystemError OnHandleSystemError;

	// Called when an actor label is changed
	static FOnActorLabelChanged OnActorLabelChanged;

#if WITH_EDITOR
	// Called before the editor displays a modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
	static FSimpleMulticastDelegate PreModal;

	// Called after the editor dismisses a modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
	static FSimpleMulticastDelegate PostModal;
#endif	//WITH_EDITOR
	
	// Called when an error occurred.
	static FSimpleMulticastDelegate OnShutdownAfterError;

	// Called when appInit is called.
	static FSimpleMulticastDelegate OnInit;

	// Called when the application is about to exit.
	static FSimpleMulticastDelegate OnExit;

	// Called when before the application is exiting.
	static FSimpleMulticastDelegate OnPreExit;

	/** Color picker color has changed, please refresh as needed*/
	static FSimpleMulticastDelegate ColorPickerChanged;

	/** requests to open a message box */
	static FOnModalMessageBox ModalErrorMessage;

	/** Called when the user accepts an invitation to the current game */
	static FOnInviteAccepted OnInviteAccepted;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FWorldOriginOffset, class UWorld*, FIntVector, FIntVector);
	/** called before world origin shifting */
	static FWorldOriginOffset PreWorldOriginOffset;
	/** called after world origin shifting */
	static FWorldOriginOffset PostWorldOriginOffset;

	/** called when the main loop would otherwise starve. */
	DECLARE_DELEGATE(FStarvedGameLoop);
	static FStarvedGameLoop StarvedGameLoop;

	/** IOS-style application lifecycle delegates */
	DECLARE_MULTICAST_DELEGATE(FApplicationLifetimeDelegate);

	// This is called when the application is about to be deactivated (e.g., due to a phone call or SMS or the sleep button).
	// The game should be paused if possible, etc...
	static FApplicationLifetimeDelegate ApplicationWillDeactivateDelegate;
	
	// Called when the application has been reactivated (reverse any processing done in the Deactivate delegate)
	static FApplicationLifetimeDelegate ApplicationHasReactivatedDelegate;

	// This is called when the application is being backgrounded (e.g., due to switching
	// to another app or closing it via the home button)
	// The game should release shared resources, save state, etc..., since it can be
	// terminated from the background state without any further warning.
	static FApplicationLifetimeDelegate ApplicationWillEnterBackgroundDelegate; // for instance, hitting the home button

	// Called when the application is returning to the foreground (reverse any processing done in the EnterBackground delegate)
	static FApplicationLifetimeDelegate ApplicationHasEnteredForegroundDelegate;

	// This *may* be called when the application is getting terminated by the OS.
	// There is no guarantee that this will ever be called on a mobile device,
	// save state when ApplicationWillEnterBackgroundDelegate is called instead.
	static FApplicationLifetimeDelegate ApplicationWillTerminateDelegate;

	/** IOS-style push notification delegates */
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationRegisteredForRemoteNotificationsDelegate, TArray<uint8>);
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationRegisteredForUserNotificationsDelegate, int);
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationFailedToRegisterForRemoteNotificationsDelegate, FString);
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationReceivedRemoteNotificationDelegate, FString);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FApplicationReceivedLocalNotificationDelegate, FString, int);

	// called when the user grants permission to register for remote notifications
	static FApplicationRegisteredForRemoteNotificationsDelegate ApplicationRegisteredForRemoteNotificationsDelegate;

	// called when the user grants permission to register for notifications
	static FApplicationRegisteredForUserNotificationsDelegate ApplicationRegisteredForUserNotificationsDelegate;

	// called when the application fails to register for remote notifications
	static FApplicationFailedToRegisterForRemoteNotificationsDelegate ApplicationFailedToRegisterForRemoteNotificationsDelegate;

	// called when the application receives a remote notification
	static FApplicationReceivedRemoteNotificationDelegate ApplicationReceivedRemoteNotificationDelegate;

	// called when the application receives a local notification
	static FApplicationReceivedLocalNotificationDelegate ApplicationReceivedLocalNotificationDelegate;

	/** Sent when a device screen orientation changes */
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationReceivedOnScreenOrientationChangedNotificationDelegate, int32);
	static FApplicationReceivedOnScreenOrientationChangedNotificationDelegate ApplicationReceivedScreenOrientationChangedNotificationDelegate;

	/** Checks to see if the stat is already enabled */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FStatCheckEnabled, const TCHAR*, bool&, bool&);
	static FStatCheckEnabled StatCheckEnabled;

	/** Sent after each stat is enabled */
	DECLARE_MULTICAST_DELEGATE_OneParam(FStatEnabled, const TCHAR*);
	static FStatEnabled StatEnabled;

	/** Sent after each stat is disabled */
	DECLARE_MULTICAST_DELEGATE_OneParam(FStatDisabled, const TCHAR*);
	static FStatDisabled StatDisabled;

	/** Sent when all stats need to be disabled */
	DECLARE_MULTICAST_DELEGATE_OneParam(FStatDisableAll, const bool);
	static FStatDisableAll StatDisableAll;

	// Called when an application is notified that the application license info has been updated.
	// The new license data should be polled and steps taken based on the results (i.e. halt application if license is no longer valid).
	DECLARE_MULTICAST_DELEGATE(FApplicationLicenseChange);
	static FApplicationLicenseChange ApplicationLicenseChange;

	/** Sent when the platform changed its laptop mode (for convertible laptops).*/
	DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformChangedLaptopMode, EConvertibleLaptopMode);
	static FPlatformChangedLaptopMode PlatformChangedLaptopMode;

	DECLARE_DELEGATE_RetVal_OneParam(bool, FLoadStringAssetReferenceInCook, const FString&);
	static FLoadStringAssetReferenceInCook LoadStringAssetReferenceInCook;

	DECLARE_DELEGATE_RetVal_OneParam(bool, FStringAssetReferenceLoaded, const FName&);
	static FStringAssetReferenceLoaded StringAssetReferenceLoaded;

	/** Sent when the platform requests a low-level VR recentering */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetRecenter);
	static FVRHeadsetRecenter VRHeadsetRecenter;

	/** Sent when connection to VR HMD is lost */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetLost);
	static FVRHeadsetLost VRHeadsetLost;

	/** Sent when connection to VR HMD is restored */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetReconnected);
	static FVRHeadsetReconnected VRHeadsetReconnected;

	/** Sent when application code changes the user activity hint string for analytics, crash reports, etc */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserActivityStringChanged, const FString&);
	static FOnUserActivityStringChanged UserActivityStringChanged;

	/** Sent when application code changes the currently active game session. The exact semantics of this will vary between games but it is useful for analytics, crash reports, etc  */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameSessionIDChange, const FString&);
	static FOnGameSessionIDChange GameSessionIDChanged;

	/** Sent by application code to set params that customize crash reporting behavior */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrashOverrideParamsChanged, const FCrashOverrideParameters&);
	static FOnCrashOverrideParamsChanged CrashOverrideParamsChanged;
	
		// Callback for platform specific very early init code.
	DECLARE_MULTICAST_DELEGATE(FOnPreMainInit);
	static FOnPreMainInit& GetPreMainInitDelegate();
	
	/** Sent when GConfig is finished initializing */
	DECLARE_MULTICAST_DELEGATE(FConfigReadyForUse);
	static FConfigReadyForUse ConfigReadyForUse;

	/** Callback for notifications regarding changes of the rendering thread. */
	DECLARE_MULTICAST_DELEGATE(FRenderingThreadChanged)

	/** Sent just after the rendering thread has been created. */
	static FRenderingThreadChanged PostRenderingThreadCreated;
	/* Sent just before the rendering thread is destroyed. */
	static FRenderingThreadChanged PreRenderingThreadDestroyed;

	// Called when appInit is called.
	static FSimpleMulticastDelegate OnFEngineLoopInitComplete;

	// Callback to allow custom resolution of package names. Arguments are InRequestedName, OutResolvedName.
	// Should return True of resolution occured.
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FResolvePackageNameDelegate, const FString&, FString&);
	static TArray<FResolvePackageNameDelegate> PackageNameResolvers;

private:

	// Callbacks for hotfixes
	static TArray<FHotFixDelegate> HotFixDelegates;

	// This class is only for namespace use
	FCoreDelegates() {}
};
