// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LocalLinuxTargetDevice.h: Declares the LocalLinuxTargetDevice class.
=============================================================================*/

#pragma once


/**
 * Type definition for shared pointers to instances of FLinuxTargetDevice.
 */
typedef TSharedPtr<class FLinuxTargetDevice, ESPMode::ThreadSafe> FLinuxTargetDevicePtr;

/**
 * Type definition for shared references to instances of FLinuxTargetDevice.
 */
typedef TSharedRef<class FLinuxTargetDevice, ESPMode::ThreadSafe> FLinuxTargetDeviceRef;


/**
 * Implements a Linux target device.
 */
class FLinuxTargetDevice
	: public ITargetDevice
{
public:

	/**
	 * Creates and initializes a new device for the specified target platform.
	 *
	 * @param InTargetPlatform - The target platform.
	 */
	FLinuxTargetDevice( const ITargetPlatform& InTargetPlatform, const FTargetDeviceId& InDeviceId, const FString& InDeviceName )
		: TargetPlatform(InTargetPlatform)
		, DeviceName(InDeviceName)
		, TargetDeviceId(InDeviceId)
	{ }


public:

	virtual bool Connect( ) override
	{
		return true;
	}

	virtual bool Deploy( const FString& SourceFolder, FString& OutAppId ) override;

	virtual void Disconnect( ) override
	{ }

	virtual ETargetDeviceTypes GetDeviceType( ) const override
	{
		return ETargetDeviceTypes::Desktop;
	}

	virtual FTargetDeviceId GetId( ) const override
	{
		return TargetDeviceId;
	}

	virtual FString GetName( ) const override
	{
		return DeviceName;
	}

	virtual FString GetOperatingSystemName( ) override
	{
		return TEXT("GNU/Linux");
	}

	virtual int32 GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) override;

	virtual const class ITargetPlatform& GetTargetPlatform( ) const override
	{
		return TargetPlatform;
	}

	virtual bool IsConnected( ) override
	{
		return true;
	}

	virtual bool IsDefault( ) const override
	{
		return true;
	}

	virtual bool PowerOff( bool Force ) override
	{
		return false;
	}

	virtual bool PowerOn( ) override
	{
		return false;
	}

	virtual bool Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId ) override;
	
	virtual bool Reboot( bool bReconnect = false ) override;

	virtual bool Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId ) override;

	virtual bool SupportsFeature( ETargetDeviceFeatures Feature ) const override;

	virtual bool SupportsSdkVersion( const FString& VersionString ) const override;

	virtual void SetUserCredentials( const FString& UserName, const FString& UserPassword ) override;

	virtual bool GetUserCredentials( FString& OutUserName, FString& OutUserPassword ) override;

	virtual bool TerminateProcess( const int32 ProcessId ) override;


private:

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;

	/** Device display name */
	FString DeviceName;

	/** Device id */
	FTargetDeviceId TargetDeviceId;

	/** User name on the remote machine */
	FString UserName;

	/** User password on the remote machine */
	FString UserPassword;
};
