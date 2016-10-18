// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved

#include "IOSTargetPlatformPrivatePCH.h"

#define EABLE_IOS_DEVICE_DETECT 0

struct FDeviceNotificationCallbackInformation
{
	FString UDID;
	FString	DeviceName;
	uint32 msgType;
};

class FIOSDevice
{
public:
    FIOSDevice(FString InID, FString InName)
		: UDID(InID)
		, Name(InName)
    {
    }
    
	FString SerialNumber() const
	{
		return UDID;
	}

private:
    FString UDID;
	FString Name;
};

/**
 * Delegate type for devices being connected or disconnected from the machine
 *
 * The first parameter is newly added or removed device
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDeviceNotification, void*)

class FDeviceQueryTask
	: public FRunnable
{
public:
	FDeviceQueryTask()
		: Stopping(false)
		, bCheckDevices(true)
	{}

	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		while (!Stopping)
		{
			if (bCheckDevices)
			{
				QueryDevices();
			}

			FPlatformProcess::Sleep(5.0f);
		}

		return 0;
	}

	virtual void Stop() override
	{
		Stopping = true;
	}

	virtual void Exit() override
	{}

	FDeviceNotification& OnDeviceNotification()
	{
		return DeviceNotification;
	}

	void Enable(bool OnOff)
	{
		bCheckDevices = OnOff;
	}

private:
	bool ExecuteIPPCommand(const FString& CommandLine, FString* OutStdOut, FString* OutStdErr) const
	{
		FString ExecutablePath = FString::Printf(TEXT("%s\\Binaries\\DotNET\\IOS"), *FPaths::EngineDir());
		FString Filename = TEXT("IPhonePackager.exe");

		// execute the command
		int32 ReturnCode;
		FString DefaultError;

		// make sure there's a place for error output to go if the caller specified nullptr
		if (!OutStdErr)
		{
			OutStdErr = &DefaultError;
		}

		void* WritePipe;
		void* ReadPipe;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*(ExecutablePath / Filename), *CommandLine, false, true, true, NULL, 0, *ExecutablePath, WritePipe);
		while (FPlatformProcess::IsProcRunning(ProcessHandle))
		{
			FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
			if (NewLine.Len() > 0)
			{
				// process the string to break it up in to lines
				*OutStdOut += NewLine;
			}

			FPlatformProcess::Sleep(0.25);
		}
		FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
		if (NewLine.Len() > 0)
		{
			// process the string to break it up in to lines
			*OutStdOut += NewLine;
		}

		FPlatformProcess::Sleep(0.25);
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

		if (!FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode))
		{
			return false;
		}

		if (ReturnCode != 0)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("The IPhonePackager command '%s' failed to run. Return code: %d, Error: %s\n"), *CommandLine, ReturnCode, **OutStdErr);

			return false;
		}

		return true;
	}

	void QueryDevices()
	{
		FString StdOut;
		// get the list of devices
		if (!ExecuteIPPCommand(TEXT("devices"), &StdOut, nullptr))
		{
			return;
		}

		// separate out each line
		TArray<FString> DeviceStrings;
		StdOut = StdOut.Replace(TEXT("\r"), TEXT("\n"));
		StdOut.ParseIntoArray(DeviceStrings, TEXT("\n"), true);

		TArray<FString> CurrentDeviceIds;
		bool FoundList = false;
		for (int32 StringIndex = 0; StringIndex < DeviceStrings.Num(); ++StringIndex)
		{
			const FString& DeviceString = DeviceStrings[StringIndex];

			// skip over non-device lines
			if (!FoundList && !DeviceString.StartsWith("List "))
			{
				continue;
			}
			else if (!FoundList && DeviceString.StartsWith("List "))
			{
				FoundList = true;
				continue;
			}

			// grab the device serial number
			int32 TabIndex;
			if (!DeviceString.FindChar(TCHAR(' '), TabIndex))
			{
				continue;
			}

			FString SerialNumber = DeviceString.Left(TabIndex);
			CurrentDeviceIds.Add(SerialNumber);

			// move on to next device if this one is already a known device
			if (ConnectedDeviceIds.Find(SerialNumber) != INDEX_NONE)
			{
				ConnectedDeviceIds.Remove(SerialNumber);
				continue;
			}

			// parse device name
			FString DeviceName;
			FParse::Value(*DeviceString, TEXT("device:"), DeviceName);

			// create an FIOSDevice
			FDeviceNotificationCallbackInformation CallbackInfo;
			CallbackInfo.DeviceName = DeviceName;
			CallbackInfo.UDID = SerialNumber;
			CallbackInfo.msgType = 1;
			DeviceNotification.Broadcast(&CallbackInfo);
		}

		// remove all devices no longer found
		for (int32 DeviceIndex = 0; DeviceIndex < ConnectedDeviceIds.Num(); ++DeviceIndex)
		{
			FDeviceNotificationCallbackInformation CallbackInfo;
			CallbackInfo.UDID = ConnectedDeviceIds[DeviceIndex];
			CallbackInfo.msgType = 2;
			DeviceNotification.Broadcast(&CallbackInfo);
		}
		ConnectedDeviceIds = CurrentDeviceIds;
	}

	bool Stopping;
	bool bCheckDevices;
	TArray<FString> ConnectedDeviceIds;
	FDeviceNotification DeviceNotification;
};

/* FIOSDeviceHelper structors
 *****************************************************************************/
static TMap<FIOSDevice*, FIOSLaunchDaemonPong> ConnectedDevices;
static FDeviceQueryTask* QueryTask = NULL;
static FRunnableThread* QueryThread = NULL;
static TArray<FDeviceNotificationCallbackInformation> NotificationMessages;
static FTickerDelegate TickDelegate;

bool FIOSDeviceHelper::MessageTickDelegate(float DeltaTime)
{
	for (int Index = 0; Index < NotificationMessages.Num(); ++Index)
	{
		FDeviceNotificationCallbackInformation cbi = NotificationMessages[Index];
		FIOSDeviceHelper::DeviceCallback(&cbi);
	}
	NotificationMessages.Empty();

	return true;
}

void FIOSDeviceHelper::Initialize(bool bIsTVOS)
{
	// Create a dummy device to hand over
	const FString DummyDeviceName(FString::Printf(bIsTVOS ? TEXT("All_tvOS_On_%s") : TEXT("All_iOS_On_%s"), FPlatformProcess::ComputerName()));
	
	FIOSLaunchDaemonPong Event;
	Event.DeviceID = FString::Printf(bIsTVOS ? TEXT("TVOS@%s") : TEXT("IOS@%s"), *DummyDeviceName);
	Event.bCanReboot = false;
	Event.bCanPowerOn = false;
	Event.bCanPowerOff = false;
	Event.DeviceName = DummyDeviceName;
	Event.DeviceType = bIsTVOS ? TEXT("AppleTV") : TEXT("");
	FIOSDeviceHelper::OnDeviceConnected().Broadcast(Event);

#if ENABLE_IOS_DEVICE_DETECT
	// add the message pump
	TickDelegate = FTickerDelegate::CreateStatic(MessageTickDelegate);
	FTicker::GetCoreTicker().AddTicker(TickDelegate, 5.0f);

	// kick off a thread to query for connected devices
	QueryTask = new FDeviceQueryTask();
	QueryTask->OnDeviceNotification().AddStatic(FIOSDeviceHelper::DeviceCallback);
	QueryThread = FRunnableThread::Create(QueryTask, TEXT("FIOSDeviceHelper.QueryTask"), 128 * 1024, TPri_Normal);
#endif
}

void FIOSDeviceHelper::DeviceCallback(void* CallbackInfo)
{
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;

	if (!IsInGameThread())
	{
		NotificationMessages.Add(*cbi);
	}
	else
	{
		switch(cbi->msgType)
		{
		case 1:
			FIOSDeviceHelper::DoDeviceConnect(CallbackInfo);
			break;

		case 2:
			FIOSDeviceHelper::DoDeviceDisconnect(CallbackInfo);
			break;
		}
	}
}

void FIOSDeviceHelper::DoDeviceConnect(void* CallbackInfo)
{
	// connect to the device
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;
	FIOSDevice* Device = new FIOSDevice(cbi->UDID, cbi->DeviceName);

	// fire the event
	FIOSLaunchDaemonPong Event;
	Event.DeviceID = FString::Printf(TEXT("IOS@%s"), *(cbi->UDID));
	Event.DeviceName = cbi->DeviceName;
	Event.bCanReboot = false;
	Event.bCanPowerOn = false;
	Event.bCanPowerOff = false;
	FIOSDeviceHelper::OnDeviceConnected().Broadcast(Event);

	// add to the device list
	ConnectedDevices.Add(Device, Event);
}

void FIOSDeviceHelper::DoDeviceDisconnect(void* CallbackInfo)
{
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;
	FIOSDevice* device = NULL;
	for (auto DeviceIterator = ConnectedDevices.CreateIterator(); DeviceIterator; ++DeviceIterator)
	{
		if (DeviceIterator.Key()->SerialNumber() == cbi->UDID)
		{
			device = DeviceIterator.Key();
			break;
		}
	}
	if (device != NULL)
	{
		// extract the device id from the connected list
		FIOSLaunchDaemonPong Event = ConnectedDevices.FindAndRemoveChecked(device);

		// fire the event
		FIOSDeviceHelper::OnDeviceDisconnected().Broadcast(Event);

		// delete the device
		delete device;
	}
}

bool FIOSDeviceHelper::InstallIPAOnDevice(const FTargetDeviceId& DeviceId, const FString& IPAPath)
{
    return false;
}

void FIOSDeviceHelper::EnableDeviceCheck(bool OnOff)
{
#if ENABLE_IOS_DEVICE_DETECT
		QueryTask->Enable(OnOff);
#endif
}