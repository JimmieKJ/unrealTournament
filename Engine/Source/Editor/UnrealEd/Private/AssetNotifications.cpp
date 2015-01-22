// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetNotifications.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "AssetNotifications"

void FAssetNotifications::SkeletonNeedsToBeSaved(USkeleton* Skeleton)
{
	FFormatNamedArguments Args;
	Args.Add( TEXT("SkeletonName"), FText::FromString( Skeleton->GetName() ) );
	FNotificationInfo Info( FText::Format( LOCTEXT("SkeletonNeedsToBeSaved", "Skeleton {SkeletonName} needs to be saved"), Args ) );
	Info.ExpireDuration = 5.0f;
	Info.bUseLargeFont = false;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_None );
	}
}

#undef LOCTEXT_NAMESPACE
