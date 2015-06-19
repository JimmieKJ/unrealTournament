// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TargetDeviceServiceMessages.generated.h"


/* Application deployment messages
 *****************************************************************************/

/**
 * Implements a message for committing a deployment transaction.
 *
 * @see FTargetDeviceServiceDeployFile, FTargetDeviceServiceDeployFinished
 */
USTRUCT()
struct FTargetDeviceServiceDeployCommit
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use. */
	UPROPERTY()
	FName Variant;

	/** Holds the identifier of the deployment transaction to commit. */
	UPROPERTY()
	FGuid TransactionId;


	/** Default constructor. */
	FTargetDeviceServiceDeployCommit() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceDeployCommit(FName InVariant, const FGuid& InTransactionId)
		: Variant(InVariant)
		, TransactionId(InTransactionId)
	{ }
};


/**
 * Implements a message for deploying a single file to a target device.
 *
 * The actual file data must be attached to the message.
 *
 * @see FTargetDeviceServiceDeployCommit, FTargetDeviceServiceDeployFinished
 */
USTRUCT()
struct FTargetDeviceServiceDeployFile
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name and path of the file as it will be stored on the target device. */
	UPROPERTY()
	FString TargetFileName;

	/** Holds the identifier of the deployment transaction that this file is part of. */
	UPROPERTY()
	FGuid TransactionId;


	/** Default constructor. */
	FTargetDeviceServiceDeployFile() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceDeployFile(const FString& InTargetFileName, const FGuid& InTransactionId)
		: TargetFileName(InTargetFileName)
		, TransactionId(InTransactionId)
	{ }
};


/**
 * Implements a message for notifying a target device proxy that a deployment transaction has finished.
 *
 * @see FTargetDeviceServiceDeployFile, FTargetDeviceServiceDeployCommit
 */
USTRUCT()
struct FTargetDeviceServiceDeployFinished
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use. */
	UPROPERTY()
	FName Variant;

	/**
	 * Holds the created identifier for the deployed application.
	 *
	 * The semantics of this identifier are target platform specific. In some cases it may be
	 * a GUID, in other cases it may be the path to the application or some other means of
	 * identifying the application. Application identifiers are returned from target device
	 * services as result of successful deployment transactions.
	 */
	UPROPERTY()
	FString AppID;

	/** Holds a flag indicating whether the deployment transaction finished successfully. */
	UPROPERTY()
	bool Succeeded;

	/** Holds the identifier of the deployment transaction that this file is part of. */
	UPROPERTY()
	FGuid TransactionId;


	/** Default constructor. */
	FTargetDeviceServiceDeployFinished() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceDeployFinished(FName InVariant, const FString& InAppId, bool InSucceeded, FGuid InTransactionId)
		: Variant(InVariant)
		, AppID(InAppId)
		, Succeeded(InSucceeded)
		, TransactionId(InTransactionId)
	{ }
};


/* Application launch messages
*****************************************************************************/

/**
 * Implements a message for committing a deployment transaction.
 *
 * To launch an arbitrary executable on a device use the FTargetDeviceServiceRunExecutable message instead.
 *
 * @see FTargetDeviceServiceLaunchFinished
 */
USTRUCT()
struct FTargetDeviceServiceLaunchApp
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use. */
	UPROPERTY()
	FName Variant;

	/**
	 * Holds the identifier of the application to launch.
	 *
	 * The semantics of this identifier are target platform specific. In some cases it may be
	 * a GUID, in other cases it may be the path to the application or some other means of
	 * identifying the application. Application identifiers are returned from target device
	 * services as result of successful deployment transactions.
	 */
	UPROPERTY()
	FString AppID;

	/** The application's build configuration, i.e. Debug or Shipping. */
	UPROPERTY()
	uint8 BuildConfiguration;

	/** Holds optional command line parameters for the application. */
	UPROPERTY()
	FString Params;


	/** Default constructor. */
	FTargetDeviceServiceLaunchApp() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceLaunchApp(FName InVariant, const FString& InAppId, uint8 InBuildConfiguration, const FString& InParams)
		: Variant(InVariant)
		, AppID(InAppId)
		, BuildConfiguration(InBuildConfiguration)
		, Params(InParams)
	{ }
};


/**
 * Implements a message for notifying a target device proxy that launching an application has finished.
 *
 * @see FTargetDeviceServiceLaunchApp
 */
USTRUCT()
struct FTargetDeviceServiceLaunchFinished
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds the identifier of the launched application.
	 *
	 * The semantics of this identifier are target platform specific. In some cases it may be
	 * a GUID, in other cases it may be the path to the application or some other means of
	 * identifying the application. Application identifiers are returned from target device
	 * services as result of successful deployment transactions.
	 */
	UPROPERTY()
	FString AppID;

	/** Holds the process identifier for the launched application. */
	UPROPERTY()
	int32 ProcessId;

	/** Holds a flag indicating whether the application was launched successfully. */
	UPROPERTY()
	bool Succeeded;


	/** Default constructor. */
	FTargetDeviceServiceLaunchFinished() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceLaunchFinished(const FString& InAppId, int32 InProcessId, bool InSucceeded)
		: AppID(InAppId)
		, ProcessId(InProcessId)
		, Succeeded(InSucceeded)
	{ }
};


/* Device claiming messages
 *****************************************************************************/

/**
 * Implements a message that is sent when a device is already claimed by someone else.
 *
 * @see FTargetDeviceClaimDropped
 * @see FTargetDeviceClaimRequest
 */
USTRUCT()
struct FTargetDeviceClaimDenied
{
	GENERATED_USTRUCT_BODY()

	/** Holds the identifier of the device that is already claimed. */
	UPROPERTY()
	FString DeviceName;

	/** Holds the name of the host computer that claimed the device. */
	UPROPERTY()
	FString HostName;

	/** Holds the name of the user that claimed the device. */
	UPROPERTY()
	FString HostUser;


	/** Default constructor. */
	FTargetDeviceClaimDenied( ) { }

	/** Creates and initializes a new instance. */
	FTargetDeviceClaimDenied(const FString& InDeviceName, const FString& InHostName, const FString& InHostUser)
		: DeviceName(InDeviceName)
		, HostName(InHostName)
		, HostUser(InHostUser)
	{ }
};


/**
 * Implements a message that is sent when a service claimed a device.
 *
 * @see FTargetDeviceClaimDenied
 * @see FTargetDeviceClaimDropped
 */
USTRUCT()
struct FTargetDeviceClaimed
{
	GENERATED_USTRUCT_BODY()

	/** Holds the identifier of the device that is being claimed. */
	UPROPERTY()
	FString DeviceName;

	/** Holds the name of the host computer that is claiming the device. */
	UPROPERTY()
	FString HostName;

	/** Holds the name of the user that is claiming the device. */
	UPROPERTY()
	FString HostUser;


	/** Default constructor. */
	FTargetDeviceClaimed( ) { }

	/** Creates and initializes a new instance. */
	FTargetDeviceClaimed(const FString& InDeviceName, const FString& InHostName, const FString& InHostUser)
		: DeviceName(InDeviceName)
		, HostName(InHostName)
		, HostUser(InHostUser)
	{ }
};


/**
 * Implements a message that is sent when a device is no longer claimed.
 *
 * @see FTargetDeviceClaimDenied, FTargetDeviceClaimRequest
 */
USTRUCT()
struct FTargetDeviceUnclaimed
{
	GENERATED_USTRUCT_BODY()

	/** Holds the identifier of the device that is no longer claimed. */
	UPROPERTY()
	FString DeviceName;

	/** Holds the name of the host computer that had claimed the device. */
	UPROPERTY()
	FString HostName;

	/** Holds the name of the user that had claimed the device. */
	UPROPERTY()
	FString HostUser;


	/** Default constructor. */
	FTargetDeviceUnclaimed( ) { }

	/** Creates and initializes a new instance. */
	FTargetDeviceUnclaimed(const FString& InDeviceName, const FString& InHostName, const FString& InHostUser)
		: DeviceName(InDeviceName)
		, HostName(InHostName)
		, HostUser(InHostUser)
	{ }
};


/* Device discovery messages
 *****************************************************************************/

/**
 * Implements a message for discovering target device services on the network.
 */
USTRUCT()
struct FTargetDeviceServicePing
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user who generated the ping. */
	UPROPERTY()
	FString HostUser;


	/** Default constructor. */
	FTargetDeviceServicePing( ) { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServicePing( const FString& InHostUser )
		: HostUser(InHostUser)
	{ }
};


/**
* Struct for a flavor's information
*/
USTRUCT()
struct FTargetDeviceVariant
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString DeviceID;

	UPROPERTY()
	FName VariantName;

	UPROPERTY()
	FString TargetPlatformName;

	UPROPERTY()
	FName TargetPlatformId;

	UPROPERTY()
	FName VanillaPlatformId;

	UPROPERTY()
	FString PlatformDisplayName;
};


/**
 * Implements a message that is sent in response to target device service discovery messages.
 */
USTRUCT()
struct FTargetDeviceServicePong
{
	GENERATED_USTRUCT_BODY()

	/** Holds a flag indicating whether the device is currently connected. */
	UPROPERTY()
	bool Connected;

	/** Holds the name of the host computer that the device is attached to. */
	UPROPERTY()
	FString HostName;

	/** Holds the name of the user under which the host computer is running. */
	UPROPERTY()
	FString HostUser;

	/** Holds the make of the device, i.e. Microsoft or Sony. */
	UPROPERTY()
	FString Make;

	/** Holds the model of the device. */
	UPROPERTY()
	FString Model;

	/** Holds the human readable name of the device, i.e "Bob's XBox'. */
	UPROPERTY()
	FString Name;

	/** Holds the name of the user that we log in to remote device as, i.e "root". */
	UPROPERTY()
	FString DeviceUser;

	/** Holds the password of the user that we log in to remote device as, i.e "12345". */
	UPROPERTY()
	FString DeviceUserPassword;

	/** Holds a flag indicating whether this device is shared with other users on the network. */
	UPROPERTY()
	bool Shared;

	/** Holds a flag indicating whether the device supports running multiple application instances in parallel. */
	UPROPERTY()
	bool SupportsMultiLaunch;

	/** Holds a flag indicating whether the device can be powered off. */
	UPROPERTY()
	bool SupportsPowerOff;

	/** Holds a flag indicating whether the device can be powered on. */
	UPROPERTY()
	bool SupportsPowerOn;

	/** Holds a flag indicating whether the device can be rebooted. */
	UPROPERTY()
	bool SupportsReboot;

	/** Holds a flag indicating whether the device's target platform supports variants. */
	UPROPERTY()
	bool SupportsVariants;

	/** Holds the device type. */
	UPROPERTY()
	FString Type;

	/** Holds the variant name of the default device. */
	UPROPERTY()
	FName DefaultVariant;

	/** List of the Flavors this service supports */
	UPROPERTY()
	TArray<FTargetDeviceVariant> Variants;
};


/* Miscellaneous messages
 *****************************************************************************/

/**
 * Implements a message for powering on a target device.
 */
USTRUCT()
struct FTargetDeviceServicePowerOff
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds a flag indicating whether the power-off should be enforced.
	 *
	 * If powering off is not enforced, if may fail if some running application prevents it.
	 */
	UPROPERTY()
	bool Force;

	/** Holds the name of the user that wishes to power off the device. */
	UPROPERTY()
	FString Operator;


	/** Default constructor. */
	FTargetDeviceServicePowerOff( ) { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServicePowerOff( const FString& InOperator, bool InForce )
		: Force(InForce)
		, Operator(InOperator)
	{ }
};


/**
 * Implements a message for powering on a target device.
 */
USTRUCT()
struct FTargetDeviceServicePowerOn
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user that wishes to power on the device. */
	UPROPERTY()
	FString Operator;


	/** Default constructor. */
	FTargetDeviceServicePowerOn( ) { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServicePowerOn( const FString& InOperator )
		: Operator(InOperator)
	{ }
};


/**
 * Implements a message for rebooting a target device.
 */
USTRUCT()
struct FTargetDeviceServiceReboot
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user that wishes to reboot the device. */
	UPROPERTY()
	FString Operator;


	/** Default constructor. */
	FTargetDeviceServiceReboot( ) { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceReboot( const FString& InOperator )
		: Operator(InOperator)
	{ }
};


/**
 * Implements a message for running an executable on a target device.
 *
 * To launch a previously deployed application on a device use the FTargetDeviceServiceLaunchApp message instead.
 *
 * @see FTargetDeviceServiceLaunchApp
 */
USTRUCT()
struct FTargetDeviceServiceRunExecutable
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use for execution. */
	UPROPERTY()
	FName Variant;

	/** Holds the path to the executable on the device. */
	UPROPERTY()
	FString ExecutablePath;

	/** Holds optional command line parameters for the executable. */
	UPROPERTY()
	FString Params;


	/** Default constructor. */
	FTargetDeviceServiceRunExecutable() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceRunExecutable(FName InVariant, const FString& InExecutablePath, const FString& InParams)
		: Variant(InVariant)
		, ExecutablePath(InExecutablePath)
		, Params(InParams)
	{ }
};


/**
 * Implements a message for notifying a target device proxy that running an executable has finished.
 *
 * @see FTargetDeviceServiceRunExecutable
 */
USTRUCT()
struct FTargetDeviceServiceRunFinished
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use. */
	UPROPERTY()
	FName Variant;

	/** Holds the path to the executable that was run. */
	UPROPERTY()
	FString ExecutablePath;

	/** Holds the process identifier of the running executable. */
	UPROPERTY()
	int32 ProcessId;

	/** Holds a flag indicating whether the executable started successfully. */
	UPROPERTY()
	bool Succeeded;


	/** Default constructor. */
	FTargetDeviceServiceRunFinished() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceRunFinished(FName InVariant, const FString& InExecutablePath, int32 InProcessId, bool InSucceeded)
		: Variant(InVariant)
		, ExecutablePath(InExecutablePath)
		, ProcessId(InProcessId)
		, Succeeded(InSucceeded)
	{ }
};
