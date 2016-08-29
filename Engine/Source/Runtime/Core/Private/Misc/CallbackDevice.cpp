// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Core includes.
#include "CorePrivatePCH.h"

const FVector DefaultRefVector(0);


//////////////////////////////////////////////////////////////////////////
// FCoreDelegates

TArray<FCoreDelegates::FHotFixDelegate> FCoreDelegates::HotFixDelegates;
TArray<FCoreDelegates::FResolvePackageNameDelegate> FCoreDelegates::PackageNameResolvers;

FCoreDelegates::FHotFixDelegate& FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Type HotFix)
{
	if (HotFix >= HotFixDelegates.Num())
	{
		HotFixDelegates.SetNum(HotFix + 1);
	}
	return HotFixDelegates[HotFix];
}

FCoreDelegates::FObjectArrayForDebugVisualizersDelegate& FCoreDelegates::GetObjectArrayForDebugVisualizersDelegate()
{
	static FObjectArrayForDebugVisualizersDelegate StaticDelegate;
	return StaticDelegate;
}

FCoreDelegates::FOnPreMainInit& FCoreDelegates::GetPreMainInitDelegate()
{
	static FOnPreMainInit StaticDelegate;
	return StaticDelegate;
}

FCoreDelegates::FOnMountPak FCoreDelegates::OnMountPak;
FCoreDelegates::FOnUnmountPak FCoreDelegates::OnUnmountPak;
FCoreDelegates::FOnUserLoginChangedEvent FCoreDelegates::OnUserLoginChangedEvent; 
FCoreDelegates::FOnUserControllerConnectionChange FCoreDelegates::OnControllerConnectionChange;
FCoreDelegates::FOnSafeFrameChangedEvent FCoreDelegates::OnSafeFrameChangedEvent;
FCoreDelegates::FOnHandleSystemEnsure FCoreDelegates::OnHandleSystemEnsure;
FCoreDelegates::FOnHandleSystemError FCoreDelegates::OnHandleSystemError;
FCoreDelegates::FOnActorLabelChanged FCoreDelegates::OnActorLabelChanged;

#if WITH_EDITOR
	FSimpleMulticastDelegate FCoreDelegates::PreModal;
	FSimpleMulticastDelegate FCoreDelegates::PostModal;
#endif	//WITH_EDITOR
FSimpleMulticastDelegate FCoreDelegates::OnShutdownAfterError;
FSimpleMulticastDelegate FCoreDelegates::OnInit;
FSimpleMulticastDelegate FCoreDelegates::OnExit;
FSimpleMulticastDelegate FCoreDelegates::OnPreExit;
FSimpleMulticastDelegate FCoreDelegates::ColorPickerChanged;
FCoreDelegates::FOnModalMessageBox FCoreDelegates::ModalErrorMessage;
FCoreDelegates::FOnInviteAccepted FCoreDelegates::OnInviteAccepted;
FCoreDelegates::FWorldOriginOffset FCoreDelegates::PreWorldOriginOffset;
FCoreDelegates::FWorldOriginOffset FCoreDelegates::PostWorldOriginOffset;
FCoreDelegates::FStarvedGameLoop FCoreDelegates::StarvedGameLoop;

FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationWillDeactivateDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationHasReactivatedDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationWillEnterBackgroundDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationHasEnteredForegroundDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationWillTerminateDelegate; 

FCoreDelegates::FApplicationRegisteredForRemoteNotificationsDelegate FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate;
FCoreDelegates::FApplicationRegisteredForUserNotificationsDelegate FCoreDelegates::ApplicationRegisteredForUserNotificationsDelegate;
FCoreDelegates::FApplicationFailedToRegisterForRemoteNotificationsDelegate FCoreDelegates::ApplicationFailedToRegisterForRemoteNotificationsDelegate;
FCoreDelegates::FApplicationReceivedRemoteNotificationDelegate FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate;
FCoreDelegates::FApplicationReceivedLocalNotificationDelegate FCoreDelegates::ApplicationReceivedLocalNotificationDelegate;

FCoreDelegates::FStatCheckEnabled FCoreDelegates::StatCheckEnabled;
FCoreDelegates::FStatEnabled FCoreDelegates::StatEnabled;
FCoreDelegates::FStatDisabled FCoreDelegates::StatDisabled;
FCoreDelegates::FStatDisableAll FCoreDelegates::StatDisableAll;

FCoreDelegates::FApplicationLicenseChange FCoreDelegates::ApplicationLicenseChange;
FCoreDelegates::FPlatformChangedLaptopMode FCoreDelegates::PlatformChangedLaptopMode;

FCoreDelegates::FLoadStringAssetReferenceInCook FCoreDelegates::LoadStringAssetReferenceInCook;

FCoreDelegates::FVRHeadsetRecenter FCoreDelegates::VRHeadsetRecenter;
FCoreDelegates::FVRHeadsetLost FCoreDelegates::VRHeadsetLost;
FCoreDelegates::FVRHeadsetReconnected FCoreDelegates::VRHeadsetReconnected;

FCoreDelegates::FOnUserActivityStringChanged FCoreDelegates::UserActivityStringChanged;
FCoreDelegates::FOnGameSessionIDChange FCoreDelegates::GameSessionIDChanged;
FCoreDelegates::FOnCrashOverrideParamsChanged FCoreDelegates::CrashOverrideParamsChanged;

FCoreDelegates::FOnAsyncLoadingFlush FCoreDelegates::OnAsyncLoadingFlush;
FCoreDelegates::FRenderingThreadChanged FCoreDelegates::PostRenderingThreadCreated;
FCoreDelegates::FRenderingThreadChanged FCoreDelegates::PreRenderingThreadDestroyed;
FSimpleMulticastDelegate FCoreDelegates::OnFEngineLoopInitComplete;
FCoreDelegates::FApplicationReceivedOnScreenOrientationChangedNotificationDelegate FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate;

FCoreDelegates::FConfigReadyForUse FCoreDelegates::ConfigReadyForUse;
