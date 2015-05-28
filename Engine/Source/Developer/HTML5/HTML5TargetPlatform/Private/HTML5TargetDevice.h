// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5TargetDevice.h: Declares the HTML5TargetDevice class.
=============================================================================*/

#pragma once


/**
 * Type definition for shared pointers to instances of FHTML5TargetDevice.
 */
typedef TSharedPtr<class FHTML5TargetDevice, ESPMode::ThreadSafe> FHTML5TargetDevicePtr;

/**
 * Type definition for shared references to instances of FHTML5TargetDevice.
 */
typedef TSharedRef<class FHTML5TargetDevice, ESPMode::ThreadSafe> FHTML5TargetDeviceRef;


/**
 * Implements a HTML5 target device.
 */
class FHTML5TargetDevice
	: public ITargetDevice
{
public:

	/**
	 * Creates and initializes a new HTML5 target device.
	 *
	 * @param InTarget - The TMAPI target interface.
	 * @param InTargetManager - The TMAPI target manager.
	 * @param InTargetPlatform - The target platform.
	 * @param InName - The device name.
	 */
	FHTML5TargetDevice( const ITargetPlatform& InTargetPlatform, const FString& InName )
		: TargetPlatform(InTargetPlatform), 
		  Name(InName)
	{ }

	/**
	 * Destructor.
	 */
	~FHTML5TargetDevice( )
	{
		Disconnect();
	}


public:

	// Begin ITargetDevice interface

	virtual bool Connect( ) override;

	virtual bool Deploy( const FString& SourceFolder, FString& OutAppId ) override;

	virtual void Disconnect( ) override;

	virtual ETargetDeviceTypes GetDeviceType( ) const override
	{
		return ETargetDeviceTypes::Browser;
	}

	virtual FTargetDeviceId GetId( ) const override;

	virtual FString GetName( ) const override;

	virtual FString GetOperatingSystemName( ) override;

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

	virtual bool Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId ) override;

	virtual bool PowerOff( bool Force ) override;

	virtual bool PowerOn( ) override;

	virtual bool Reboot( bool bReconnect = false ) override;

	virtual bool Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId ) override;

	virtual bool SupportsFeature( ETargetDeviceFeatures Feature ) const override;

	virtual bool SupportsSdkVersion( const FString& VersionString ) const override;

	virtual bool TerminateProcess( const int32 ProcessId ) override;

	virtual void SetUserCredentials( const FString& UserName, const FString& UserPassword ) override;

	virtual bool GetUserCredentials( FString& OutUserName, FString& OutUserPassword ) override;

	// End ITargetDevice interface

private:

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;

	FString Name; 
};
