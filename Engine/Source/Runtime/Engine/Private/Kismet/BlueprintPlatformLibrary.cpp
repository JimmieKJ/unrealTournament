// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/GameEngine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/BlueprintPlatformLibrary.h"
#include "Engine/Console.h"
#include "AssertionMacros.h"
#include "OutputDevice.h"

void UPlatformGameInstance::PostInitProperties()

{
    Super::PostInitProperties();

    FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationWillDeactivateDelegate_Handler);
    FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationHasReactivatedDelegate_Handler);
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationWillEnterBackgroundDelegate_Handler);
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationHasEnteredForegroundDelegate_Handler);
    FCoreDelegates::ApplicationWillTerminateDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationWillTerminateDelegate_Handler);
    FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationRegisteredForRemoteNotificationsDelegate_Handler);
    FCoreDelegates::ApplicationRegisteredForUserNotificationsDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationRegisteredForUserNotificationsDelegate_Handler);
    FCoreDelegates::ApplicationFailedToRegisterForRemoteNotificationsDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationFailedToRegisterForRemoteNotificationsDelegate_Handler);
    FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationReceivedRemoteNotificationDelegate_Handler);
    FCoreDelegates::ApplicationReceivedLocalNotificationDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationReceivedLocalNotificationDelegate_Handler);
    FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationReceivedScreenOrientationChangedNotificationDelegate_Handler);
}

void UPlatformGameInstance::BeginDestroy()

{
	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillTerminateDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationRegisteredForUserNotificationsDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationFailedToRegisterForRemoteNotificationsDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.RemoveAll(this);

    Super::BeginDestroy();
}

void UPlatformGameInstance::ApplicationReceivedScreenOrientationChangedNotificationDelegate_Handler(int32 inScreenOrientation)
{
	ApplicationReceivedScreenOrientationChangedNotificationDelegate.Broadcast((EScreenOrientation::Type)inScreenOrientation);
}


//////////////////////////////////////////////////////////////////////////
// UBlueprintPlatformLibrary

UBlueprintPlatformLibrary::UBlueprintPlatformLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	platformService = NULL;

	FString ModuleName;
	GConfig->GetString(TEXT("LocalNotification"), TEXT("DefaultPlatformService"), ModuleName, GEngineIni);

	if (ModuleName.Len() > 0)
	{
		// load the module by name from the .ini
		ILocalNotificationModule* module = FModuleManager::LoadModulePtr<ILocalNotificationModule>(*ModuleName);

		// did the module exist?
		if (module != nullptr)
		{
			platformService = module->GetLocalNotificationService();
		}
	}
}

void UBlueprintPlatformLibrary::ClearAllLocalNotifications()
{
	if(platformService == nullptr)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("No local notification service"));
		return;
	}
	
	platformService->ClearAllLocalNotifications();
}

void UBlueprintPlatformLibrary::ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool inLocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
	if(platformService == nullptr)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("No local notification service"));
		return;
	}

	UE_LOG(LogBlueprintUserMessages, Log, TEXT("Scheduling notification %s at %d/%d/%d %d:%d:%d %s"), *(Title.ToString()), FireDateTime.GetMonth(), FireDateTime.GetDay(), FireDateTime.GetYear(), FireDateTime.GetHour(), FireDateTime.GetMinute(), FireDateTime.GetSecond(), inLocalTime ? TEXT("Local") : TEXT("UTC"));
	
	platformService->ScheduleLocalNotificationAtTime(FireDateTime, inLocalTime, Title, Body, Action, ActivationEvent);
}
       
void UBlueprintPlatformLibrary::ScheduleLocalNotificationFromNow(int32 inSecondsFromNow, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
	FDateTime	dateTime = FDateTime::Now();
	dateTime += FTimespan(0, 0, inSecondsFromNow);

	ScheduleLocalNotificationAtTime(dateTime, true, Title, Body, Action, ActivationEvent);
}

void UBlueprintPlatformLibrary::GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate)
{
	if(platformService == nullptr)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("No local notification service"));
		return;
	}
	
	platformService->GetLaunchNotification(NotificationLaunchedApp, ActivationEvent, FireDate);
}

ILocalNotificationService*	UBlueprintPlatformLibrary::platformService;
