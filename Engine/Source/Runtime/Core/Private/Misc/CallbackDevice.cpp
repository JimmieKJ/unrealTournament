// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// Core includes.
#include "CorePrivatePCH.h"

const FVector DefaultRefVector(0);


//////////////////////////////////////////////////////////////////////////
// FCoreDelegates

TArray<FCoreDelegates::FHotFixDelegate> FCoreDelegates::HotFixDelegates;

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

FCoreDelegates::FGetSerialNumberBlocksForDebugVisualizersDelegate& FCoreDelegates::GetSerialNumberBlocksForDebugVisualizersDelegate()
{
	static FGetSerialNumberBlocksForDebugVisualizersDelegate StaticDelegate;
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

FCoreDelegates::FStatCheckEnabled FCoreDelegates::StatCheckEnabled;
FCoreDelegates::FStatEnabled FCoreDelegates::StatEnabled;
FCoreDelegates::FStatDisabled FCoreDelegates::StatDisabled;
FCoreDelegates::FStatDisableAll FCoreDelegates::StatDisableAll;

FCoreDelegates::FApplicationLicenseChange FCoreDelegates::ApplicationLicenseChange;
FCoreDelegates::FPlatformChangedLaptopMode FCoreDelegates::PlatformChangedLaptopMode;
