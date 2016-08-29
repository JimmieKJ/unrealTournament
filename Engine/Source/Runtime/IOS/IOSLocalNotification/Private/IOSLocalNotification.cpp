// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	IOSLocalNotification.cpp: Unreal IOSLocalNotification service interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "IOSLocalNotification.h"
#include "Engine.h"

#include "IOSApplication.h"
#include "IOSAppDelegate.h"

DEFINE_LOG_CATEGORY(LogIOSLocalNotification);

class FIOSLocalNotificationModule : public ILocalNotificationModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual ILocalNotificationService* GetLocalNotificationService() override
	{
		static ILocalNotificationService*	oneTrueLocalNotificationService = nullptr;
		
		if(oneTrueLocalNotificationService == nullptr)
		{
			oneTrueLocalNotificationService = new FIOSLocalNotificationService;
		}
		
		return oneTrueLocalNotificationService;
	}
};

IMPLEMENT_MODULE(FIOSLocalNotificationModule, IOSLocalNotification);

/*------------------------------------------------------------------------------------
	FIOSLocalNotification
 ------------------------------------------------------------------------------------*/
FIOSLocalNotificationService::FIOSLocalNotificationService()
{
	AppLaunchedWithNotification = false;
	LaunchNotificationFireDate = 0;
}

void FIOSLocalNotificationService::ClearAllLocalNotifications()
{
#if !PLATFORM_TVOS
	UIApplication* application = [UIApplication sharedApplication];
	
	[application cancelAllLocalNotifications];
#endif
}

void FIOSLocalNotificationService::ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
#if !PLATFORM_TVOS
	UIApplication* application = [UIApplication sharedApplication];
	
	NSCalendar *calendar = [NSCalendar autoupdatingCurrentCalendar];
	NSDateComponents *dateComps = [[NSDateComponents alloc] init];
	[dateComps setDay:FireDateTime.GetDay()];
	[dateComps setMonth:FireDateTime.GetMonth()];
	[dateComps setYear:FireDateTime.GetYear()];
	[dateComps setHour:FireDateTime.GetHour()];
	[dateComps setMinute:FireDateTime.GetMinute()];
	[dateComps setSecond:FireDateTime.GetSecond()];
	NSDate *itemDate = [calendar dateFromComponents:dateComps];

	UILocalNotification *localNotif = [[UILocalNotification alloc] init];
	if (localNotif == nil)
		return;
	localNotif.fireDate = itemDate;
	
	if(LocalTime)
	{
		localNotif.timeZone = [NSTimeZone defaultTimeZone];
	}
	else
	{
		localNotif.timeZone = nil;
	}
	
	NSString*	alertBody = [NSString stringWithFString:Body.ToString()];
	if(alertBody != nil)
	{
		localNotif.alertBody = alertBody;
	}

	NSString*	alertAction = [NSString stringWithFString:Action.ToString()];
	if(alertAction != nil)
	{
		localNotif.alertAction = alertAction;
	}
	
	if([IOSAppDelegate GetDelegate].OSVersion >= 8.2f)
	{
		NSString*	alertTitle = [NSString stringWithFString:Title.ToString()];
		if(alertTitle != nil)
		{
			localNotif.alertTitle = alertTitle;
		}
	}
	
	localNotif.soundName = UILocalNotificationDefaultSoundName;
	localNotif.applicationIconBadgeNumber = 1;

	NSString*	activateEventNSString = [NSString stringWithFString:ActivationEvent];
	if(activateEventNSString != nil)
	{
		NSDictionary*	infoDict = [NSDictionary dictionaryWithObject:activateEventNSString forKey:@"ActivationEvent"];

		if(infoDict != nil)
		{
			localNotif.userInfo = infoDict;
		}
	}

	[[UIApplication sharedApplication] scheduleLocalNotification:localNotif];
#endif
}

void FIOSLocalNotificationService::GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate)
{
	NotificationLaunchedApp = AppLaunchedWithNotification;
	ActivationEvent = LaunchNotificationActivationEvent;
	FireDate = LaunchNotificationFireDate;
}

void FIOSLocalNotificationService::SetLaunchNotification(FString const& ActivationEvent, int32 FireDate)
{
	AppLaunchedWithNotification = true;
	LaunchNotificationActivationEvent = ActivationEvent;
	LaunchNotificationFireDate = FireDate;
}
