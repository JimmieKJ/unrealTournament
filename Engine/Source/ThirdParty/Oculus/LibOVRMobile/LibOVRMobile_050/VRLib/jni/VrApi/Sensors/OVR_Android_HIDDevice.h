/************************************************************************************
Filename    :   OVR_Android_HIDDevice.h
Content     :   Android HID device implementation.
Created     :   July 10, 2013
Authors     :   Brant Lewis

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_Android_HIDDevice_h
#define OVR_Android_HIDDevice_h

#include "OVR_HIDDevice.h"
#include "OVR_Android_DeviceManager.h"

#include <jni.h>

namespace OVR { namespace Android {

class HIDDeviceManager;

enum eDeviceMode
{
	DEVICE_MODE_UNKNOWN,
	DEVICE_MODE_READ,
	DEVICE_MODE_READ_WRITE,
	DEVICE_MODE_WRITE,
	DEVICE_MODE_MAX
};

//-------------------------------------------------------------------------------------
// ***** Android HIDDevice

class HIDDevice : public OVR::HIDDevice, public DeviceManagerThread::Notifier
{
private:
    friend class HIDDeviceManager;

public:
    HIDDevice(HIDDeviceManager* manager);

    // This is a minimal constructor used during enumeration for us to pass
    // a HIDDevice to the visit function (so that it can query feature reports).
    HIDDevice(HIDDeviceManager* manager, int deviceHandle);
    
    virtual ~HIDDevice();

    bool HIDInitialize(const String& path);
    void HIDShutdown();
    
    virtual bool SetFeatureReport(UByte* data, UInt32 length);
	virtual bool GetFeatureReport(UByte* data, UInt32 length);

    // DeviceManagerThread::Notifier
    void OnEvent(int i, int fd);
    double OnTicks(double tickSeconds);

    bool OnDeviceNotification(	MessageType messageType,
                            	HIDDeviceDesc* devDesc,
                            	bool* error);

    bool OnDeviceAddedNotification(	const String& devNodePath,
                         	 	 	HIDDeviceDesc* devDesc,
                         	 	 	bool* error);

private:
    bool initDeviceInfo();
    bool openDevice();
    void closeDevice();
    void closeDeviceOnIOError();
    bool setupDevicePluggedInNotification();

    bool                    InMinimalMode;
    HIDDeviceManager*       HIDManager;

    int                     Device;     // File handle to the device.
    String                  DevNodePath;
    eDeviceMode             DeviceMode;

    HIDDeviceDesc           DevDesc;
    
    enum { ReadBufferSize = 96 };
    UByte                   ReadBuffer[ReadBufferSize];

    UInt16                  InputReportBufferLength;
    UInt16                  OutputReportBufferLength;
    UInt16                  FeatureReportBufferLength;
};


//-------------------------------------------------------------------------------------
// ***** Android HIDDeviceManager

class HIDDeviceManager : public OVR::HIDDeviceManager, public DeviceManagerThread::Notifier
{
	friend class HIDDevice;

public:
    HIDDeviceManager(Android::DeviceManager* manager);
    virtual ~HIDDeviceManager();

    virtual bool Initialize();
    virtual void Shutdown();

    virtual bool Enumerate(HIDEnumerateVisitor* enumVisitor);
    virtual OVR::HIDDevice* Open(const String& path);

    // Fills HIDDeviceDesc by using the path.
    // Returns 'true' if successful, 'false' otherwise.
    bool GetHIDDeviceDesc(const String& path, HIDDeviceDesc* pdevDesc) const;

    static HIDDeviceManager* CreateInternal(DeviceManager* manager);

    // DeviceManagerThread::Notifier - OnTicks is used to initiate poll for new devices.
    void OnEvent(int /*i*/, int /*fd*/) {};
    double OnTicks(double tickSeconds);

private:
    bool initializeManager();

    bool initVendorProduct(int deviceHandle, HIDDeviceDesc* desc) const;
    bool getFullDesc(int deviceHandle, const String& devNodePath, HIDDeviceDesc* desc) const;

    bool getStringProperty(const String& devNodePath, const char* propertyName, OVR::String* pResult) const;
    bool getPath(int deviceHandle, const String& devNodePath, String* pPath) const;
    
    bool AddNotificationDevice(HIDDevice* device);
    bool RemoveNotificationDevice(HIDDevice* device);


    void scanForDevices(bool firstScan = false);
    void getCurrentDevices(Array<String>* deviceList);
    void removeDevicePath(HIDDevice* device);

    DeviceManager*        	DevManager;

    Array<HIDDevice*>     	NotificationDevices;
    Array<String>			ScannedDevicePaths;
    double 					TimeToPollForDevicesSeconds;
};

}} // namespace OVR::Android

#endif // OVR_Android_HIDDevice_h
