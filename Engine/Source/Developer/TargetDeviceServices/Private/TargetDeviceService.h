// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Implements remote services for a specific target device.
 */
class FTargetDeviceService
	: public ITargetDeviceService
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InDeviceName The name of the device to expose.
	 * @param InMessageBus The message bus to listen on for clients.
	 */
	FTargetDeviceService(const FString& InDeviceName, const IMessageBusRef& InMessageBus);

	/** Destructor. */
	~FTargetDeviceService();

public:

	// ITargetDeviceService interface

	virtual void AddTargetDevice(ITargetDevicePtr InDevice) override;
	virtual bool CanStart(FName InFlavor = NAME_None) const override;

	virtual const FString& GetClaimHost() override
	{
		return ClaimHost;
	}

	virtual const FString& GetClaimUser() override
	{
		return ClaimUser;
	}

	virtual ITargetDevicePtr GetDevice(FName InVariant = NAME_None) const override;

	virtual FString GetDeviceName() const override
	{
		return DeviceName;
	}

	virtual FName GetDevicePlatformName() const override
	{
		return DevicePlatformName;
	}

	virtual FString GetDevicePlatformDisplayName() const override
	{
		return DevicePlatformDisplayName;
	}

	virtual bool IsRunning() const override
	{
		return Running;
	}

	virtual bool IsShared( ) const override
	{
		return (Running && Shared);
	}

	virtual int32 NumTargetDevices() override
	{
		return TargetDevicePtrs.Num();
	}


	virtual void RemoveTargetDevice(ITargetDevicePtr InDevice) override;

	virtual void SetShared(bool InShared) override
	{
		Shared = InShared;
	}

	virtual bool Start() override;
	virtual void Stop() override;

protected:

	/**
	 * Stores the specified file to deploy.
	 *
	 * @param FileReader The archive reader providing access to the file data.
	 * @param TargetFileName The desired name of the file on the target device.
	 */
	bool StoreDeployedFile(FArchive* FileReader, const FString& TargetFileName) const;

private:

	/** Callback for FTargetDeviceClaimDenied messages. */
	void HandleClaimDeniedMessage(const FTargetDeviceClaimDenied& Message, const IMessageContextRef& Context);

	/** Callback for FTargetDeviceClaimDenied messages. */
	void HandleClaimedMessage( const FTargetDeviceClaimed& Message, const IMessageContextRef& Context );

	/** Callback for FTargetDeviceClaimDropped messages. */
	void HandleUnclaimedMessage(const FTargetDeviceUnclaimed& Message, const IMessageContextRef& Context);

	/** Callback for FTargetDeviceServiceDeployFile messages. */
	void HandleDeployFileMessage(const FTargetDeviceServiceDeployFile& Message, const IMessageContextRef& Context);

	/** Callback for FTargetDeviceServiceDeployCommit messages. */
	void HandleDeployCommitMessage(const FTargetDeviceServiceDeployCommit& Message, const IMessageContextRef& Context);

	/** Callback for FTargetDeviceServiceLaunchApp messages. */
	void HandleLaunchAppMessage(const FTargetDeviceServiceLaunchApp& Message, const IMessageContextRef& Context);

	/** Callback for FTargetDeviceServicePing messages. */
	void HandlePingMessage(const FTargetDeviceServicePing& InMessage, const IMessageContextRef& Context);

	 /** Callback for FTargetDeviceServicePowerOff messages. */
	void HandlePowerOffMessage(const FTargetDeviceServicePowerOff& Message, const IMessageContextRef& Context);

	/** Callback for FTargetDeviceServicePowerOn messages. */
	void HandlePowerOnMessage(const FTargetDeviceServicePowerOn& Message, const IMessageContextRef& Context);

	/** Callback for FTargetDeviceServiceReboot messages. */
	void HandleRebootMessage(const FTargetDeviceServiceReboot& Message, const IMessageContextRef& Context);

	/** Callback for FTargetDeviceServiceRunExecutable messages. */
	void HandleRunExecutableMessage(const FTargetDeviceServiceRunExecutable& Message, const IMessageContextRef& Context);

private:

	/** Default target device used when no flavor is specified. */
	ITargetDeviceWeakPtr DefaultDevicePtr;

	/** Caches the name of the device name that this services exposes. */
	FString DeviceName;

	/** Caches the platform name of the device name that this services exposes. */
	FName DevicePlatformName;

	/** Caches the platform name of the device name that this services exposes. */
	FString DevicePlatformDisplayName;

	/** Holds the name of the host that has a claim on the device. */
	FString ClaimHost;

	/** Holds the message address of the target device service that has a claim on the device. */
	FMessageAddress ClaimAddress;

	/** Holds the name of the user that has a claim on the device. */
	FString ClaimUser;

	/** Holds the message endpoint. */
	FMessageEndpointPtr MessageEndpoint;

	/** Holds a flag indicating whether this service is currently running. */
	bool Running;

	/** Holds a flag indicating whether the device is shared with other users. */
	bool Shared;
	
	/** Map of all the Flavors for this Service */
	TMap<FName, ITargetDeviceWeakPtr> TargetDevicePtrs;
};
