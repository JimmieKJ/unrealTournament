// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RawInputSettings.h"
#include "RawInputFunctionLibrary.h"

#if PLATFORM_WINDOWS
	#include "Windows/RawInputWindows.h"
#endif

FRawInputDeviceConfiguration::FRawInputDeviceConfiguration()
{
	ButtonProperties.AddDefaulted(12);
	AxisProperties.AddDefaulted(8);

	ButtonProperties[0].Key = FRawInputKeys::GenericUSBController_Button1;
	ButtonProperties[1].Key = FRawInputKeys::GenericUSBController_Button2;
	ButtonProperties[2].Key = FRawInputKeys::GenericUSBController_Button3;
	ButtonProperties[3].Key = FRawInputKeys::GenericUSBController_Button4;
	ButtonProperties[4].Key = FRawInputKeys::GenericUSBController_Button5;
	ButtonProperties[5].Key = FRawInputKeys::GenericUSBController_Button6;
	ButtonProperties[6].Key = FRawInputKeys::GenericUSBController_Button7;
	ButtonProperties[7].Key = FRawInputKeys::GenericUSBController_Button8;
	ButtonProperties[8].Key = FRawInputKeys::GenericUSBController_Button9;
	ButtonProperties[9].Key = FRawInputKeys::GenericUSBController_Button10;
	ButtonProperties[10].Key = FRawInputKeys::GenericUSBController_Button11;
	ButtonProperties[11].Key = FRawInputKeys::GenericUSBController_Button12;

	AxisProperties[0].Key = FRawInputKeys::GenericUSBController_Axis1;
	AxisProperties[1].Key = FRawInputKeys::GenericUSBController_Axis2;
	AxisProperties[2].Key = FRawInputKeys::GenericUSBController_Axis3;
	AxisProperties[3].Key = FRawInputKeys::GenericUSBController_Axis4;
	AxisProperties[4].Key = FRawInputKeys::GenericUSBController_Axis5;
	AxisProperties[5].Key = FRawInputKeys::GenericUSBController_Axis6;
	AxisProperties[6].Key = FRawInputKeys::GenericUSBController_Axis7;
	AxisProperties[7].Key = FRawInputKeys::GenericUSBController_Axis8;
}

FName URawInputSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText URawInputSettings::GetSectionText() const
{
	return NSLOCTEXT("RawInputPlugin", "RawInputSettingsSection", "Raw Input");
}

void URawInputSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	
#if PLATFORM_WINDOWS
	FRawInputWindows* RawInput = static_cast<FRawInputWindows*>(static_cast<FRawInputPlugin*>(&FRawInputPlugin::Get())->GetRawInputDevice().Get());

	for (const TPair<int32, FRawWindowsDeviceEntry>& RegisteredDevice : RawInput->RegisteredDeviceList)
	{
		const FRawWindowsDeviceEntry& DeviceEntry = RegisteredDevice.Value;
		if (DeviceEntry.bIsConnected)
		{
			RawInput->SetupBindings(RegisteredDevice.Key, false);
		}
	}

#endif
}
#endif