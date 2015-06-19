/************************************************************************************
Filename    :   OVR_Android_HIDDevice.h
Content     :   Android HID device implementation.
Created     :   July 10, 2013
Authors     :   Brant Lewis
 
Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_Android_HIDDevice.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include "OVR_HIDDeviceImpl.h"
#include <dirent.h>
#include <string.h>
#include <linux/types.h>
#include <sys/stat.h>

#include <jni.h>

// Definitions from linux/hidraw.h

#define HID_MAX_DESCRIPTOR_SIZE		4096

struct hidraw_report_descriptor {
    __u32 size;
    __u8 value[HID_MAX_DESCRIPTOR_SIZE];
};

struct hidraw_devinfo {
    __u32 bustype;
    __s16 vendor;
    __s16 product;
};

// ioctl interface
#define HIDIOCGRDESCSIZE	_IOR('H', 0x01, int)
#define HIDIOCGRDESC		_IOR('H', 0x02, struct hidraw_report_descriptor)
#define HIDIOCGRAWINFO		_IOR('H', 0x03, struct hidraw_devinfo)
#define HIDIOCGRAWNAME(len)     _IOC(_IOC_READ, 'H', 0x04, len)
#define HIDIOCGRAWPHYS(len)     _IOC(_IOC_READ, 'H', 0x05, len)
// The first byte of SFEATURE and GFEATURE is the report number
#define HIDIOCSFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x07, len)

#define HIDRAW_FIRST_MINOR 0
#define HIDRAW_MAX_DEVICES 64
// number of reports to buffer
#define HIDRAW_BUFFER_SIZE 64

#define OVR_DEVICE_NAMES	"ovr"

namespace OVR { namespace Android {
    
static const char * deviceModeNames[DEVICE_MODE_MAX] =
{
	"UNKNOWN", "READ", "READ_WRITE", "WRITE"
};


//-------------------------------------------------------------------------------------
// **** Android::DeviceManager
//-----------------------------------------------------------------------------
HIDDeviceManager::HIDDeviceManager(DeviceManager* manager) : DevManager(manager)
{
	TimeToPollForDevicesSeconds = 0.0;
}

//-----------------------------------------------------------------------------
HIDDeviceManager::~HIDDeviceManager()
{
}

//-----------------------------------------------------------------------------
bool HIDDeviceManager::initializeManager()
{
    return true;
}

//-----------------------------------------------------------------------------
bool HIDDeviceManager::Initialize()
{
	DevManager->pThread->AddTicksNotifier(this);
	scanForDevices(true);
	return true;
}

//-----------------------------------------------------------------------------
void HIDDeviceManager::Shutdown()
{
    LogText("OVR::Android::HIDDeviceManager - shutting down.\n");
}

//-------------------------------------------------------------------------------
bool HIDDeviceManager::AddNotificationDevice(HIDDevice* device)
{
    NotificationDevices.PushBack(device);
    return true;
}

//-------------------------------------------------------------------------------
bool HIDDeviceManager::RemoveNotificationDevice(HIDDevice* device)
{
    for (UPInt i = 0; i < NotificationDevices.GetSize(); i++)
    {
        if (NotificationDevices[i] == device)
        {
            NotificationDevices.RemoveAt(i);
            return true;
        }
    }
    return false;
}

//-------------------------------------------------------------------------------
bool HIDDeviceManager::initVendorProduct(int deviceHandle, HIDDeviceDesc* desc) const
{
    // Get Raw Info
    hidraw_devinfo info;
    memset(&info, 0, sizeof(info));

    int r = ioctl(deviceHandle, HIDIOCGRAWINFO, &info);

    if (r < 0)
    {
        return false;
    }

    desc->VendorId = info.vendor;
    desc->ProductId = info.product;

    return true;
}

//-----------------------------------------------------------------------------
bool HIDDeviceManager::getStringProperty(const String& devNodePath, const char* propertyName, OVR::String* pResult) const
{
	// LDC - In order to get device data on Android we walk the sysfs folder hierarchy. Normally this would be handled by
	// libudev but that doesn't seem available through the ndk. For the device '/dev/ovr0' we find the physical
	// path of the symbolic link '/sys/class/ovr/ovr0'. Then we walk up the folder hierarchy checking to see if
	// the property 'file' is present in that folder e.g. '/sys/devices/platform/xhci-hcd/usb1/1-1/manufacturer'.
	// When we find it we read and return the reported string. We have to walk the path backwards to avoid returning
	// properties of any usb hubs that are in the usb graph.

	// Create the command string using the last character of the device path.
	char cmdStr[1024];
	sprintf(cmdStr, "cd -P /sys/class/ovr/ovr%c; pwd", devNodePath[devNodePath.GetLength()-1]);

	// Execute the command and get stdout.
	FILE* cmdFile = popen(cmdStr, "r");

	if (cmdFile)
	{
		char stdoutBuf[1024];
		char *pSysPath = fgets(stdoutBuf, sizeof(stdoutBuf), cmdFile);
		pclose(cmdFile);

		if (pSysPath == NULL)
		{
			return false;
		}

		// Now walk the path back until we find a folder with a file that has the same name as the propertyName.
		while(true)
		{
			DIR* dir = opendir(pSysPath);
			if (dir)
			{
				dirent* entry = readdir(dir);
				while (entry)
				{
			    	if (strcmp(entry->d_name, propertyName) == 0)
			    	{
			    		// We've found the file.
				    	closedir(dir);

				    	char propertyPath[1024];
				    	sprintf(propertyPath, "%s/%s", pSysPath, propertyName);

				    	FILE* propertyFile = fopen(propertyPath, "r");

						char propertyContents[2048];
				    	char* result = fgets(propertyContents, sizeof(propertyContents), propertyFile);
						fclose(propertyFile);

						if (result == NULL)
						{
							return false;
						}

						// Remove line feed character.
						propertyContents[strlen(propertyContents)-1] = '\0';

						*pResult = propertyContents;

				    	return true;
			    	}

					entry = readdir(dir);
				}

				closedir(dir);
			}

			// Determine the parent folder path.
			while (pSysPath[strlen(pSysPath)-1] != '/')
			{
				pSysPath[strlen(pSysPath)-1] = '\0';

				if (strlen(pSysPath) == 0)
				{
					return false;
				}
			}

			// Remove the '/' from the end.
			pSysPath[strlen(pSysPath)-1] = '\0';
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
bool HIDDeviceManager::Enumerate(HIDEnumerateVisitor* enumVisitor)
{

    // Scan the /dev directory looking for devices
    DIR* dir = opendir("/dev");
    if (dir)
    {
        dirent* entry = readdir(dir);
        while (entry)
        {
            if (strstr(entry->d_name, OVR_DEVICE_NAMES))
            {   // Open the device to check if this is an Oculus device.

            	char devicePath[32];
                sprintf(devicePath, "/dev/%s", entry->d_name);


                // Try to open for both read and write initially if we can.
                int device = open(devicePath, O_RDWR);
                if (device < 0)
                {
                	device = open(devicePath, O_RDONLY);
                }

                if (device >= 0)
                {
                    HIDDeviceDesc devDesc;

                    if (initVendorProduct(device, &devDesc) &&
                        enumVisitor->MatchVendorProduct(devDesc.VendorId, devDesc.ProductId))
                    {
                    	getFullDesc(device, String(devicePath), &devDesc);

                        // Look for the device to check if it is already opened.
                        Ptr<DeviceCreateDesc> existingDevice = DevManager->FindHIDDevice(devDesc);
                        // if device exists and it is opened then most likely the device open()
                        // will fail; therefore, we just set Enumerated to 'true' and continue.
                        if (existingDevice && existingDevice->pDevice)
                        {
                            existingDevice->Enumerated = true;
                        }
                        else
                        {
                        	// Construct minimal device that the visitor callback can get feature reports from.
                            Android::HIDDevice hidDevice(this, device);
                            enumVisitor->Visit(hidDevice, devDesc);
                        }
                    }

                    close(device);
                }
                else
                {
                    LogText("Failed to open device %s with error %d\n", devicePath, errno);
                }
            }
            entry = readdir(dir);
        }

        closedir(dir);
    }

    return true;
}

//-----------------------------------------------------------------------------
bool HIDDeviceManager::getPath(int deviceHandle, const String& devNodePath, String* pPath) const
{

	HIDDeviceDesc desc;

    if (!initVendorProduct(deviceHandle, &desc))
    {
        return false;
    }

    getStringProperty(devNodePath, "serial", &(desc.SerialNumber));

    // Compose the path.
	StringBuffer buffer;
	buffer.AppendFormat(	"vid=%04hx:pid=%04hx:ser=%s",
							desc.VendorId,
							desc.ProductId,
							desc.SerialNumber.ToCStr());

	*pPath = String(buffer);

    return true;
}

//-----------------------------------------------------------------------------
bool HIDDeviceManager::getFullDesc(int deviceHandle, const String& devNodePath, HIDDeviceDesc* desc) const
{

    if (!initVendorProduct(deviceHandle, desc))
    {
        return false;
    }

    String versionStr;
    getStringProperty(devNodePath, "bcdDevice", &versionStr);
    SInt32 versionNum;
    sscanf(versionStr.ToCStr(), "%x", &versionNum);
    desc->VersionNumber = versionNum;

    getStringProperty(devNodePath, "manufacturer", &(desc->Manufacturer));
    getStringProperty(devNodePath, "product", &(desc->Product));
    getStringProperty(devNodePath, "serial", &(desc->SerialNumber));

    // Compose the path.
    getPath(deviceHandle, devNodePath, &(desc->Path));

    return true;
}

//-----------------------------------------------------------------------------
bool HIDDeviceManager::GetHIDDeviceDesc(const String& devNodePath, HIDDeviceDesc* pdevDesc) const
{

    int deviceHandle = open(devNodePath.ToCStr(), O_RDONLY);

    if (deviceHandle < 0)
    {
        return false;
    }

    bool result = getFullDesc(deviceHandle, devNodePath, pdevDesc);

    close(deviceHandle);
    return result;
}

//-----------------------------------------------------------------------------
OVR::HIDDevice* HIDDeviceManager::Open(const String& path)
{

    Ptr<Android::HIDDevice> device = *new Android::HIDDevice(this);

    if (!device->HIDInitialize(path))
    {
    	return NULL;
    }

    device->AddRef();

    return device;
}

//=============================================================================
//                           Android::HIDDevice
//=============================================================================

HIDDevice::HIDDevice(HIDDeviceManager* manager) :
	InMinimalMode(false),
	HIDManager(manager),
	Device( -1 ),
	DeviceMode( DEVICE_MODE_UNKNOWN )
{
}

//-----------------------------------------------------------------------------
// This is a minimal constructor used during enumeration for us to pass
// a HIDDevice to the visit function (so that it can query feature reports).
HIDDevice::HIDDevice(HIDDeviceManager* manager, int deviceHandle) :
	InMinimalMode(true),
	HIDManager(manager),
	Device(deviceHandle),
	DeviceMode( DEVICE_MODE_UNKNOWN )
{
}

//-----------------------------------------------------------------------------
HIDDevice::~HIDDevice()
{
    if (!InMinimalMode)
    {
        HIDShutdown();
    }
}

//-----------------------------------------------------------------------------
bool HIDDevice::HIDInitialize(const String& path)
{

	DevDesc.Path = path;

	if (!openDevice())
    {
        LogText("OVR::Android::HIDDevice - Failed to open HIDDevice: %s", path.ToCStr());
        return false;
    }

    if (!initDeviceInfo())
    {
        LogText("OVR::Android::HIDDevice - Failed to get device info for HIDDevice: %s", path.ToCStr());
        closeDevice();
        return false;
    }


    HIDManager->DevManager->pThread->AddTicksNotifier(this);
    HIDManager->AddNotificationDevice(this);

    LogText("OVR::Android::HIDDevice - Opened:'%s'  Manufacturer:'%s'  Product:'%s'  Serial#:'%s'  Version:'%04x'\n",
    		DevDesc.Path.ToCStr(),
            DevDesc.Manufacturer.ToCStr(),
            DevDesc.Product.ToCStr(),
            DevDesc.SerialNumber.ToCStr(),
            DevDesc.VersionNumber);

    return true;
}

//-----------------------------------------------------------------------------
bool HIDDevice::initDeviceInfo()
{
    // Device must have been successfully opened.
	OVR_ASSERT(Device >= 0);

#if 0
    int desc_size = 0;
    hidraw_report_descriptor rpt_desc;
    memset(&rpt_desc, 0, sizeof(rpt_desc));

    // get report descriptor size
    int r = ioctl(Device, HIDIOCGRDESCSIZE, &desc_size);
    if (r < 0)
    {
        OVR_ASSERT_LOG(false, ("Failed to get report descriptor size."));
        return false;
    }

    // Get the report descriptor
    rpt_desc.size = desc_size;
    r = ioctl(Device, HIDIOCGRDESC, &rpt_desc);
    if (r < 0)
    {
        OVR_ASSERT_LOG(false, ("Failed to get report descriptor."));
        return false;
    }

    // TODO: Parse the report descriptor and read out the report lengths etc.
#endif

    // Hard code report lengths for now.
    InputReportBufferLength = 62;
    OutputReportBufferLength = 0;
    FeatureReportBufferLength = 69;
    
    if (ReadBufferSize < InputReportBufferLength)
    {
        OVR_ASSERT_LOG(false, ("Input report buffer length is bigger than read buffer."));
        return false;
    }

    
	// Get device desc.
    if (!HIDManager->getFullDesc(Device, DevNodePath, &DevDesc))
    {
        OVR_ASSERT_LOG(false, ("Failed to get device desc while initializing device."));
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool HIDDevice::openDevice()
{

	OVR_ASSERT(Device == -1);

	OVR_DEBUG_LOG(("HIDDevice::openDevice %s", DevDesc.Path.ToCStr()));

	// Have to iterate through devices to find the one with this path.
	// Scan the /dev directory looking for devices
	DIR* dir = opendir("/dev");
	if (dir)
	{
		dirent* entry = readdir(dir);
		while (entry)
		{
			if (strstr(entry->d_name, OVR_DEVICE_NAMES))
			{
				// Open the device to check if this is an Oculus device.
				char devicePath[32];
				sprintf(devicePath, "/dev/%s", entry->d_name);

				// Try to open for both read and write if we can.
				int device = open(devicePath, O_RDWR);
				DeviceMode = DEVICE_MODE_READ_WRITE;
				if (device < 0)
				{
					device = open(devicePath, O_RDONLY);
					DeviceMode = DEVICE_MODE_READ;
				}

				if (device >= 0)
				{
					String path;
					if (!HIDManager->getPath(device, devicePath, &path) ||
						path != DevDesc.Path)
					{
						close(device);
						continue;
					}

					Device = device;
					DevNodePath = String(devicePath);
					LogText("OVR::Android::HIDDevice - device mode %s", deviceModeNames[DeviceMode] );
					break;
				}
			}
			entry = readdir(dir);
		}
		closedir(dir);
	}

    if (Device < 0)
    {
        OVR_DEBUG_LOG(("Failed to open device %s, error = 0x%X.", DevDesc.Path.ToCStr(), errno));
        Device = -1;
        return false;
    }

    // Add the device to the polling list.
    if (!HIDManager->DevManager->pThread->AddSelectFd(this, Device))
    {
        OVR_ASSERT_LOG(false, ("Failed to initialize polling for HIDDevice."));

        close(Device);
        Device = -1;
        return false;
    }
    
    return true;
}
    
//-----------------------------------------------------------------------------
void HIDDevice::HIDShutdown()
{

    HIDManager->DevManager->pThread->RemoveTicksNotifier(this);
    HIDManager->RemoveNotificationDevice(this);
    
    if (Device >= 0) // Device may already have been closed if unplugged.
    {
        closeDevice();
    }
    
    LogText("OVR::Android::HIDDevice - HIDShutdown '%s'\n", DevDesc.Path.ToCStr());
}

//-----------------------------------------------------------------------------
void HIDDevice::closeDevice()
{
    OVR_ASSERT(Device >= 0);
    
    HIDManager->DevManager->pThread->RemoveSelectFd(this, Device);

    close(Device);  // close the file handle
    Device = -1;
    DeviceMode = DEVICE_MODE_UNKNOWN;
    LogText("OVR::Android::HIDDevice - HID Device Closed '%s'\n", DevDesc.Path.ToCStr());
}

//-----------------------------------------------------------------------------
void HIDDevice::closeDeviceOnIOError()
{
    LogText("OVR::Android::HIDDevice - Lost connection to '%s'\n", DevDesc.Path.ToCStr());
    closeDevice();
}

//-----------------------------------------------------------------------------
bool HIDDevice::SetFeatureReport(UByte* data, UInt32 length)
{
    if (Device < 0)
        return false;
    
    UByte reportID = data[0];

    if (reportID == 0)
    {
        // Not using reports so remove from data packet.
        data++;
        length--;
    }

    int r = ioctl(Device, HIDIOCSFEATURE(length), data);
	return (r >= 0);
}

//-----------------------------------------------------------------------------
bool HIDDevice::GetFeatureReport(UByte* data, UInt32 length)
{
    if (Device < 0)
        return false;

	int r = ioctl(Device, HIDIOCGFEATURE(length), data);
    return (r >= 0);
}

//-----------------------------------------------------------------------------
double HIDDevice::OnTicks(double tickSeconds)
{
    if (Handler)
    {
        return Handler->OnTicks(tickSeconds);
    }
    
    return DeviceManagerThread::Notifier::OnTicks(tickSeconds);
}

//-----------------------------------------------------------------------------
void HIDDevice::OnEvent(int i, int fd)
{
    // We have data to read from the device
    int bytes = read(fd, ReadBuffer, ReadBufferSize);

	if (bytes < 0)
	{
		LogText( "OVR::Android::HIDDevice - ReadError: fd %d, ReadBufferSize %d, BytesRead %d, errno %d\n", 
			fd, ReadBufferSize, bytes, errno );

		if ( errno == EAGAIN )
		{
			LogText( "OVR::Android::HIDDevice - EAGAIN, device is %s.", deviceModeNames[DeviceMode] );
		}

		// Close the device on read error.
		closeDeviceOnIOError();

		// Generate a device removed event.
		bool error;
		OnDeviceNotification(Message_DeviceRemoved, &DevDesc, &error);

		HIDManager->removeDevicePath( this );

		return;
	}

	
	// TODO: I need to handle partial messages and package reconstruction
	if (Handler)
	{
		Handler->OnInputReport(ReadBuffer, bytes);
	}
}

//-----------------------------------------------------------------------------
bool HIDDevice::OnDeviceAddedNotification(	const String& devNodePath,
                                     	 	HIDDeviceDesc* devDesc,
                                     	 	bool* error)
{

	String devicePath = devDesc->Path;

    // Is this the correct device?
    if (DevDesc.Path.CompareNoCase(devicePath) != 0)
    {
        return false;
    }

    if (Device == -1)
    {
    	// A closed device has been re-added. Try to reopen.

    	DevNodePath = devNodePath;

        if (!openDevice())
        {
            LogError("OVR::Android::HIDDevice - Failed to reopen a device '%s' that was re-added.\n", devicePath.ToCStr());
			*error = true;
            return true;
        }

        LogText("OVR::Android::HIDDevice - Reopened device '%s'\n", devicePath.ToCStr());
    }

    *error = false;
    return true;
}

//-----------------------------------------------------------------------------
bool HIDDevice::OnDeviceNotification(MessageType messageType,
                                     HIDDeviceDesc* devDesc,
                                     bool* error)
{

	String devicePath = devDesc->Path;

    // Is this the correct device?
    if (DevDesc.Path.CompareNoCase(devicePath) != 0)
    {
        return false;
    }

    if (messageType == Message_DeviceRemoved)
    {
        if (Device >= 0)
        {
            closeDevice();
        }

        if (Handler)
        {
            Handler->OnDeviceMessage(HIDHandler::HIDDeviceMessage_DeviceRemoved);
        }
    }
    else
    {
        OVR_ASSERT(0);
    }

    *error = false;
    return true;
}

//-----------------------------------------------------------------------------
HIDDeviceManager* HIDDeviceManager::CreateInternal(Android::DeviceManager* devManager)
{
        
    if (!System::IsInitialized())
    {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
                            LogMessage(Log_Debug, "HIDDeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Android::HIDDeviceManager> manager = *new Android::HIDDeviceManager(devManager);

    if (manager)
    {
        if (manager->Initialize())
        {
            manager->AddRef();
        }
        else
        {
            manager.Clear();
        }
    }

    return manager.GetPtr();
}

//-----------------------------------------------------------------------------
void HIDDeviceManager::getCurrentDevices(Array<String>* deviceList)
{
	deviceList->Clear();

	DIR* dir = opendir("/dev");
	if (dir)
	{
		dirent* entry = readdir(dir);
		while (entry)
		{
			if (strstr(entry->d_name, OVR_DEVICE_NAMES))
			{
				char dev_path[32];
				sprintf(dev_path, "/dev/%s", entry->d_name);

				deviceList->PushBack(String(dev_path));
			}

			entry = readdir(dir);
		}
	}

	closedir(dir);
}

//-----------------------------------------------------------------------------
void HIDDeviceManager::scanForDevices(bool firstScan)
{

	// Create current device list.
	Array<String> currentDeviceList;
	getCurrentDevices(&currentDeviceList);

	if (firstScan)
	{
		ScannedDevicePaths = currentDeviceList;
		return;
	}

	// Check for new devices.
    for (Array<String>::Iterator itCurrent = currentDeviceList.Begin();
    			itCurrent != currentDeviceList.End(); ++itCurrent)
	{
    	String devNodePath = *itCurrent;
    	bool found = false;

    	// Was it in the previous scan?
    	for (Array<String>::Iterator itScanned = ScannedDevicePaths.Begin();
        		itScanned != ScannedDevicePaths.End(); ++itScanned)
    	{
        	if (devNodePath.CompareNoCase(*itScanned) == 0)
        	{
        		found = true;
        		break;
        	}
    	}

    	if (found)
    	{
    		continue;
    	}

    	// Get device desc.
        HIDDeviceDesc devDesc;
		if (!GetHIDDeviceDesc(devNodePath, &devDesc))
        {
        	continue;
        }


    	bool error = false;
        bool deviceFound = false;
    	for (UPInt i = 0; i < NotificationDevices.GetSize(); i++)
        {
    		if (NotificationDevices[i] &&
    			NotificationDevices[i]->OnDeviceAddedNotification(devNodePath, &devDesc, &error))
    		{
    			// The device handled the notification so we're done.
                deviceFound = true;
    			break;
    		}
        }

		if (deviceFound)
		{
			continue;
		}


		// A new device was connected. Go through all device factories and
		// try to detect the device using HIDDeviceDesc.
		Lock::Locker deviceLock(DevManager->GetLock());
		DeviceFactory* factory = DevManager->Factories.GetFirst();
		while(!DevManager->Factories.IsNull(factory))
		{
			if (factory->DetectHIDDevice(DevManager, devDesc))
			{
				break;
			}
			factory = factory->pNext;
		}
	}

    ScannedDevicePaths = currentDeviceList;
}

//-----------------------------------------------------------------------------
void HIDDeviceManager::removeDevicePath(HIDDevice* device)
{
	for ( int i = 0; i < ScannedDevicePaths.GetSizeI(); i++ )
	{
		if ( ScannedDevicePaths[i] == device->DevNodePath )
		{
			ScannedDevicePaths.RemoveAt( i );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
double HIDDeviceManager::OnTicks(double tickSeconds)
{
	if (tickSeconds >= TimeToPollForDevicesSeconds)
	{
		TimeToPollForDevicesSeconds = tickSeconds + 1.0;
		scanForDevices();
	}

    return TimeToPollForDevicesSeconds - tickSeconds;
}

} // namespace Android

//-------------------------------------------------------------------------------------
// ***** Creation

// Creates a new HIDDeviceManager and initializes OVR.
HIDDeviceManager* HIDDeviceManager::Create()
{
    OVR_ASSERT_LOG(false, ("Standalone mode not implemented yet."));
    
    if (!System::IsInitialized())
    {
        // Use custom message, since Log is not yet installed.
        OVR_DEBUG_STATEMENT(Log::GetDefaultLog()->
            LogMessage(Log_Debug, "HIDDeviceManager::Create failed - OVR::System not initialized"); );
        return 0;
    }

    Ptr<Android::HIDDeviceManager> manager = *new Android::HIDDeviceManager(NULL);

    if (manager)
    {
        if (manager->Initialize())
        {
            manager->AddRef();
        }
        else
        {
            manager.Clear();
        }
    }

    return manager.GetPtr();
}

} // namespace OVR

