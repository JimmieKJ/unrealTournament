// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineSubsystemPackage.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineNotificationTransportInterface.h"

// forward declarations
class IOnlineNotificationTransport;


/** Whether a handler function handled a particular notification */
enum class EOnlineNotificationResult
{
	None,		// No handling occurred
	Handled,	// Notification was handled
};

/**
* Macro to parse a notification struct into a passed in UStruct.
* Example:
*
* FNotificationData Data;
*
* if (READ_NOTIFICATION_JSON(Data,Notification)) {}
*
* You'll need to include JsonUtilities.h to use it
*/
#define READ_NOTIFICATION_JSON(Struct, Notification) FJsonObjectConverter::JsonObjectToUStruct(Notification.Payload->AsObject().ToSharedRef(), Struct.StaticStruct(), &Struct, 0, 0)

/**
* Delegate type for handling a notification
*
* The first parameter is a notification structure
* Return result code to indicate if notification has been handled
*/
DECLARE_DELEGATE_RetVal_OneParam(EOnlineNotificationResult, FHandleOnlineNotificationSignature, const FOnlineNotification&);


/** Struct to keep track of bindings */
struct FOnlineNotificationBinding
{
	/** Delegate to call when this binding is activated */
	FHandleOnlineNotificationSignature NotificationDelegate;

	FOnlineNotificationBinding()
	{}

	FOnlineNotificationBinding(const FHandleOnlineNotificationSignature& InNotificationDelegate)
		: NotificationDelegate(InNotificationDelegate)
	{}
};

/** This class is a static manager used to track notification transports and map the delivered notifications to subscribed notification handlers */
class ONLINESUBSYSTEM_API FOnlineNotificationHandler
{

protected:

	typedef TMap< FString, TArray<FOnlineNotificationBinding> > NotificationTypeBindingsMap;

	/** Map from type of notification to the delegate to call */
	NotificationTypeBindingsMap SystemBindingMap;

	/** Map from player and type of notification to the delegate to call */
	TMap< FString, NotificationTypeBindingsMap > PlayerBindingMap;

public:

	/** Lifecycle is managed by OnlineSubSystem, all access should be made through there */
	FOnlineNotificationHandler()
	{
	}

	// SYSTEM NOTIFICATION BINDINGS

	/** Add a notification binding for a type */
	DELEGATE_DEPRECATED("AddSystemNotificationBinding is deprecated because it relies on deprecated delegate comparison to ignore multiple registrations, instead use AddSystemNotificationBinding_Handle and avoid multiple registrations.")
	void AddSystemNotificationBinding(FString NotificationType, const FOnlineNotificationBinding& NewBinding);

	/** Add a notification binding for a type */
	FDelegateHandle AddSystemNotificationBinding_Handle(FString NotificationType, const FOnlineNotificationBinding& NewBinding);

	/** Remove the notification handler for a type */
	DELEGATE_DEPRECATED("This overload of RemoveSystemNotificationBinding is deprecated, instead use RemoveSystemNotificationBinding passing the result of AddSystemNotificationBinding_Handle.")
	void RemoveSystemNotificationBinding(FString NotificationType, const FOnlineNotificationBinding& RemoveBinding);

	/** Remove the notification handler for a type */
	void RemoveSystemNotificationBinding(FString NotificationType, FDelegateHandle RemoveHandle);

	/** Resets all system notification handlers */
	void ResetSystemNotificationBindings();

	// PLAYER NOTIFICATION BINDINGS

	/** Add a notification binding for a type */
	DELEGATE_DEPRECATED("AddPlayerNotificationBinding is deprecated because it relies on deprecated delegate comparison to ignore multiple registrations, instead use AddPlayerNotificationBinding_Handle and avoid multiple registrations.")
	void AddPlayerNotificationBinding(const FUniqueNetId& PlayerId, FString NotificationType, const FOnlineNotificationBinding& NewBinding);

	/** Add a notification binding for a type */
	FDelegateHandle AddPlayerNotificationBinding_Handle(const FUniqueNetId& PlayerId, FString NotificationType, const FOnlineNotificationBinding& NewBinding);

	/** Remove the player notification handler for a type */
	DELEGATE_DEPRECATED("This overload of RemovePlayerNotificationBinding is deprecated, instead use RemovePlayerNotificationBinding passing the result of AddPlayerNotificationBinding_Handle.")
	void RemovePlayerNotificationBinding(const FUniqueNetId& PlayerId, FString NotificationType, const FOnlineNotificationBinding& RemoveBinding);

	/** Remove the player notification handler for a type */
	void RemovePlayerNotificationBinding(const FUniqueNetId& PlayerId, FString NotificationType, FDelegateHandle RemoveHandle);

	/** Resets a player's notification handlers */
	void ResetPlayerNotificationBindings(const FUniqueNetId& PlayerId);

	/** Resets all player notification handlers */
	void ResetAllPlayerNotificationBindings();

	// RECEIVING NOTIFICATIONS

	/** Deliver a notification to the appropriate handler for that player/msg type.  Called by NotificationTransport implementations. */
	void DeliverNotification(const FOnlineNotification& Notification);
};

typedef TSharedPtr<FOnlineNotificationHandler, ESPMode::ThreadSafe> FOnlineNotificationHandlerPtr;
