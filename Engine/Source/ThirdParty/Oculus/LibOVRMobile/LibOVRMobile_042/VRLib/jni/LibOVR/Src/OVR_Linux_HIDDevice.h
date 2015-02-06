/************************************************************************************
Filename    :   OVR_Linux_HIDDevice.h
Content     :   Linux HID device implementation.
Created     :   June 13, 2013
Authors     :   Brant Lewis

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_LINUX_HIDDevice_h
#define OVR_LINUX_HIDDevice_h

#include "OVR_HIDDevice.h"
#include "OVR_Linux_DeviceManager.h"
#include <libudev.h>

namespace OVR { namespace Linux {

class HIDDeviceManager;

//-------------------------------------------------------------------------------------
// ***** Linux HIDDevice

class HIDDevice : public OVR::HIDDevice, public DeviceManagerThread::Notifier
{
private:
    friend class HIDDeviceManager;

public:
    HIDDevice(HIDDeviceManager* manager);

    // This is a minimal constructor used during enumeration for us to pass
    // a HIDDevice to the visit function (so that it can query feature reports).
    HIDDevice(HIDDeviceManager* manager, int device_handle);
    
    virtual ~HIDDevice();

    bool HIDInitialize(const String& path);
    void HIDShutdown();
    
    virtual bool SetFeatureReport(UByte* data, UInt32 length);
	virtual bool GetFeatureReport(UByte* data, UInt32 length);

    // DeviceManagerThread::Notifier
    void OnEvent(int i, int fd);
    double OnTicks(double tickSeconds);

    bool OnDeviceNotification(MessageType messageType,
                              HIDDeviceDesc* device_info,
                              bool* error);

private:
    bool initInfo();
    bool openDevice(const char* dev_path);
    void closeDevice(bool wasUnplugged);
    void closeDeviceOnIOError();
    bool setupDevicePluggedInNotification();

    bool                    InMinimalMode;
    HIDDeviceManager*       HIDManager;
    int                     DeviceHandle;     // file handle to the device
    HIDDeviceDesc           DevDesc;
    
    enum { ReadBufferSize = 96 };
    UByte                   ReadBuffer[ReadBufferSize];

    UInt16                  InputReportBufferLength;
    UInt16                  OutputReportBufferLength;
    UInt16                  FeatureReportBufferLength;
};


//-------------------------------------------------------------------------------------
// ***** Linux HIDDeviceManager

class HIDDeviceManager : public OVR::HIDDeviceManager, public DeviceManagerThread::Notifier
{
	friend class HIDDevice;

public:
    HIDDeviceManager(Linux::DeviceManager* Manager);
    virtual ~HIDDeviceManager();

    virtual bool Initialize();
    virtual void Shutdown();

    virtual bool Enumerate(HIDEnumerateVisitor* enumVisitor);
    virtual OVR::HIDDevice* Open(const String& path);

    static HIDDeviceManager* CreateInternal(DeviceManager* manager);

    void OnEvent(int i, int fd);
    
private:
    bool initializeManager();
    bool initVendorProductVersion(udev_device* device, HIDDeviceDesc* pDevDesc);
    bool getPath(udev_device* device, String* pPath);
    bool getIntProperty(udev_device* device, const char* key, int32_t* pResult);
    bool getStringProperty(udev_device* device,
                           const char* propertyName,
                           OVR::String* pResult);
    bool getFullDesc(udev_device* device, HIDDeviceDesc* desc);
    bool GetDescriptorFromPath(const char* dev_path, HIDDeviceDesc* desc);
    
    bool AddNotificationDevice(HIDDevice* device);
    bool RemoveNotificationDevice(HIDDevice* device);
    
    DeviceManager*           DevManager;

    udev*                    UdevInstance;     // a handle to the udev library instance
    udev_monitor*            HIDMonitor;
    int                      HIDMonHandle;     // the udev_monitor file handle

    Array<HIDDevice*>        NotificationDevices;
};

}} // namespace OVR::Linux

#endif // OVR_Linux_HIDDevice_h
