/************************************************************************************

Filename    :   OVR_SensorDeviceImpl.cpp
Content     :   Oculus Sensor device implementation.
Created     :   March 7, 2013
Authors     :   Lee Cooper, Dov Katz

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_SensorDeviceImpl.h"

// HMDDeviceDesc can be created/updated through Sensor carrying DisplayInfo.

#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Alg.h"

namespace OVR {
    
//-------------------------------------------------------------------------------------
// ***** Oculus Sensor-specific packet data structures

enum {    
    Sensor_BootLoader   = 0x1001,

    Sensor_DefaultReportRate = 500, // Hz
    Sensor_MaxReportRate     = 1000 // Hz
};

// Reported data is little-endian now
static UInt16 DecodeUInt16(const UByte* buffer)
{
    return (UInt16(buffer[1]) << 8) | UInt16(buffer[0]);
}

static SInt16 DecodeSInt16(const UByte* buffer)
{
    return (SInt16(buffer[1]) << 8) | SInt16(buffer[0]);
}

static UInt32 DecodeUInt32(const UByte* buffer)
{    
    return (buffer[0]) | UInt32(buffer[1] << 8) | UInt32(buffer[2] << 16) | UInt32(buffer[3] << 24);    
}

static void EncodeUInt16(UByte* buffer, UInt16 val)
{
    *(UInt16*)buffer = Alg::ByteUtil::SystemToLE ( val );
}

static float DecodeFloat(const UByte* buffer)
{
    union {
        UInt32 U;
        float  F;
    };

    U = DecodeUInt32(buffer);
    return F;
}


static void UnpackSensor(const UByte* buffer, SInt32* x, SInt32* y, SInt32* z)
{
    // Sign extending trick
    // from http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
    struct {SInt32 x:21;} s;

    *x = s.x = (buffer[0] << 13) | (buffer[1] << 5) | ((buffer[2] & 0xF8) >> 3);
    *y = s.x = ((buffer[2] & 0x07) << 18) | (buffer[3] << 10) | (buffer[4] << 2) |
               ((buffer[5] & 0xC0) >> 6);
    *z = s.x = ((buffer[5] & 0x3F) << 15) | (buffer[6] << 7) | (buffer[7] >> 1);
}

void PackSensor(UByte* buffer, SInt32 x, SInt32 y, SInt32 z)
{
    // Pack 3 32 bit integers into 8 bytes
    buffer[0] = UByte(x >> 13);
    buffer[1] = UByte(x >> 5);
    buffer[2] = UByte((x << 3) | ((y >> 18) & 0x07));
    buffer[3] = UByte(y >> 10);
    buffer[4] = UByte(y >> 2);
    buffer[5] = UByte((y << 6) | ((z >> 15) & 0x3F));
    buffer[6] = UByte(z >> 7);
    buffer[7] = UByte(z << 1);
}


// Messages we care for
enum TrackerMessageType
{
    TrackerMessage_None              = 0,
    TrackerMessage_Sensors           = 1,
    TrackerMessage_Unknown           = 0x100,
    TrackerMessage_SizeError         = 0x101,
};

struct TrackerSample
{
    SInt32 AccelX, AccelY, AccelZ;
    SInt32 GyroX, GyroY, GyroZ;
};


struct TrackerSensors
{
    UByte	SampleCount;
    UInt16	Timestamp;
    UInt16	LastCommandID;
    SInt16	Temperature;

    TrackerSample Samples[3];

    SInt16	MagX, MagY, MagZ;

    TrackerMessageType Decode(const UByte* buffer, int size)
    {
        if (size < 62)
            return TrackerMessage_SizeError;

        SampleCount		= buffer[1];
        Timestamp		= DecodeUInt16(buffer + 2);
        LastCommandID	= DecodeUInt16(buffer + 4);
        Temperature		= DecodeSInt16(buffer + 6);
        
        //if (SampleCount > 2)        
        //    OVR_DEBUG_LOG_TEXT(("TackerSensor::Decode SampleCount=%d\n", SampleCount));        

        // Only unpack as many samples as there actually are
        UByte iterationCount = (SampleCount > 2) ? 3 : SampleCount;

        for (UByte i = 0; i < iterationCount; i++)
        {
            UnpackSensor(buffer + 8 + 16 * i,  &Samples[i].AccelX, &Samples[i].AccelY, &Samples[i].AccelZ);
            UnpackSensor(buffer + 16 + 16 * i, &Samples[i].GyroX,  &Samples[i].GyroY,  &Samples[i].GyroZ);
        }

        MagX = DecodeSInt16(buffer + 56);
        MagY = DecodeSInt16(buffer + 58);
        MagZ = DecodeSInt16(buffer + 60);

        return TrackerMessage_Sensors;
    }
};

struct TrackerMessage
{
    TrackerMessageType Type;
    TrackerSensors     Sensors;
};

bool DecodeTrackerMessage(TrackerMessage* message, UByte* buffer, int size)
{
    memset(message, 0, sizeof(TrackerMessage));

    if (size < 4)
    {
        message->Type = TrackerMessage_SizeError;
        return false;
    }

    switch (buffer[0])
    {
    case TrackerMessage_Sensors:
        message->Type = message->Sensors.Decode(buffer, size);
        break;

    default:
        message->Type = TrackerMessage_Unknown;
        break;
    }

    return (message->Type < TrackerMessage_Unknown) && (message->Type != TrackerMessage_None);
}


// ***** SensorRangeImpl Implementation

// Sensor HW only accepts specific maximum range values, used to maximize
// the 16-bit sensor outputs. Use these ramps to specify and report appropriate values.
static const UInt16 AccelRangeRamp[] = { 2, 4, 8, 16 };
static const UInt16 GyroRangeRamp[]  = { 250, 500, 1000, 2000 };
static const UInt16 MagRangeRamp[]   = { 880, 1300, 1900, 2500 };

static UInt16 SelectSensorRampValue(const UInt16* ramp, unsigned count,
                                    float val, float factor, const char* label)
{    
    UInt16 threshold = (UInt16)(val * factor);

    for (unsigned i = 0; i<count; i++)
    {
        if (ramp[i] >= threshold)
            return ramp[i];
    }
    OVR_DEBUG_LOG(("SensorDevice::SetRange - %s clamped to %0.4f",
                   label, float(ramp[count-1]) / factor));
    OVR_UNUSED2(factor, label);
    return ramp[count-1];
}

// SensorScaleImpl provides buffer packing logic for the Sensor Range
// record that can be applied to DK1 sensor through Get/SetFeature. We expose this
// through SensorRange class, which has different units.
struct SensorRangeImpl
{
    enum  { PacketSize = 8 };
    UByte   Buffer[PacketSize];
    
    UInt16  CommandId;
    UInt16  AccelScale;
    UInt16  GyroScale;
    UInt16  MagScale;

    SensorRangeImpl(const SensorRange& r, UInt16 commandId = 0)
    {
        SetSensorRange(r, commandId);
    }

    void SetSensorRange(const SensorRange& r, UInt16 commandId = 0)
    {
        CommandId  = commandId;
        AccelScale = SelectSensorRampValue(AccelRangeRamp, sizeof(AccelRangeRamp)/sizeof(AccelRangeRamp[0]),
                                           r.MaxAcceleration, (1.0f / 9.81f), "MaxAcceleration");
        GyroScale  = SelectSensorRampValue(GyroRangeRamp, sizeof(GyroRangeRamp)/sizeof(GyroRangeRamp[0]),
                                           r.MaxRotationRate, Math<float>::RadToDegreeFactor, "MaxRotationRate");
        MagScale   = SelectSensorRampValue(MagRangeRamp, sizeof(MagRangeRamp)/sizeof(MagRangeRamp[0]),
                                           r.MaxMagneticField, 1000.0f, "MaxMagneticField");
        Pack();
    }

    void GetSensorRange(SensorRange* r)
    {
        r->MaxAcceleration = AccelScale * 9.81f;
        r->MaxRotationRate = DegreeToRad((float)GyroScale);
        r->MaxMagneticField= MagScale * 0.001f;
    }

    static SensorRange GetMaxSensorRange()
    {
        return SensorRange(AccelRangeRamp[sizeof(AccelRangeRamp)/sizeof(AccelRangeRamp[0]) - 1] * 9.81f,
                           GyroRangeRamp[sizeof(GyroRangeRamp)/sizeof(GyroRangeRamp[0]) - 1] *
                                Math<float>::DegreeToRadFactor,
                           MagRangeRamp[sizeof(MagRangeRamp)/sizeof(MagRangeRamp[0]) - 1] * 0.001f);
    }

    void  Pack()
    {
        Buffer[0] = 4;
        Buffer[1] = UByte(CommandId & 0xFF);
        Buffer[2] = UByte(CommandId >> 8);
        Buffer[3] = UByte(AccelScale);
        Buffer[4] = UByte(GyroScale & 0xFF);
        Buffer[5] = UByte(GyroScale >> 8);
        Buffer[6] = UByte(MagScale & 0xFF);
        Buffer[7] = UByte(MagScale >> 8);
    }

    void Unpack()
    {
        CommandId = Buffer[1] | (UInt16(Buffer[2]) << 8);
        AccelScale= Buffer[3];
        GyroScale = Buffer[4] | (UInt16(Buffer[5]) << 8);
        MagScale  = Buffer[6] | (UInt16(Buffer[7]) << 8);
    }
};


// Sensor configuration command, ReportId == 2.

struct SensorConfigImpl
{
    enum  { PacketSize = 7 };
    UByte   Buffer[PacketSize];

    // Flag values for Flags.
    enum {
        Flag_RawMode            = 0x01,
        Flag_CalibrationTest    = 0x02, // Internal test mode
        Flag_UseCalibration     = 0x04,
        Flag_AutoCalibration    = 0x08,
        Flag_MotionKeepAlive    = 0x10,
        Flag_CommandKeepAlive   = 0x20,
        Flag_SensorCoordinates  = 0x40
    };

    UInt16  CommandId;
    UByte   Flags;
    UInt16  PacketInterval;
    UInt16  KeepAliveIntervalMs;

    SensorConfigImpl() : CommandId(0), Flags(0), PacketInterval(0), KeepAliveIntervalMs(0)
    {
        memset(Buffer, 0, PacketSize);
        Buffer[0] = 2;
    }

    void    SetSensorCoordinates(bool sensorCoordinates)
    { Flags = (Flags & ~Flag_SensorCoordinates) | (sensorCoordinates ? Flag_SensorCoordinates : 0); }
    bool    IsUsingSensorCoordinates() const
    { return (Flags & Flag_SensorCoordinates) != 0; }

    void Pack()
    {
        Buffer[0] = 2;
        Buffer[1] = UByte(CommandId & 0xFF);
        Buffer[2] = UByte(CommandId >> 8);
        Buffer[3] = Flags;
        Buffer[4] = UByte(PacketInterval);
        Buffer[5] = UByte(KeepAliveIntervalMs & 0xFF);
        Buffer[6] = UByte(KeepAliveIntervalMs >> 8);
    }

    void Unpack()
    {
        CommandId          = Buffer[1] | (UInt16(Buffer[2]) << 8);
        Flags              = Buffer[3];
        PacketInterval     = Buffer[4];
        KeepAliveIntervalMs= Buffer[5] | (UInt16(Buffer[6]) << 8);
    }
    
};

// SensorFactoryCalibration - feature report used to retrieve factory calibration data.
struct SensorFactoryCalibrationImpl
{
    enum  { PacketSize = 69 };
    UByte   Buffer[PacketSize];
    
    Vector3f AccelOffset;
    Vector3f GyroOffset;
    Matrix4f AccelMatrix;
    Matrix4f GyroMatrix;
    float    Temperature;

    SensorFactoryCalibrationImpl() 
		: AccelOffset(), GyroOffset(), AccelMatrix(), GyroMatrix(), Temperature(0)
	{
		memset(Buffer, 0, PacketSize);
		Buffer[0] = 3;
	}

	void Pack()
	{
		static const float sensorMax = (1 << 20) - 1;
		SInt32 x, y, z;

		Buffer[0] = 3;

		x = SInt32(AccelOffset.x * 1e4f);
		y = SInt32(AccelOffset.y * 1e4f);
		z = SInt32(AccelOffset.z * 1e4f);
		PackSensor(Buffer + 3, x, y, z);

		x = SInt32(GyroOffset.x * 1e4f);
		y = SInt32(GyroOffset.y * 1e4f);
		z = SInt32(GyroOffset.z * 1e4f);
		PackSensor(Buffer + 11, x, y, z);

		for (int i = 0; i < 3; i++)
		{
			SInt32 row[3];
			for (int j = 0; j < 3; j++)
			{
				float val = AccelMatrix.M[i][j];
				if (i == j)
					val -= 1.0f;
				row[j] = SInt32(val * sensorMax);
			}
			PackSensor(Buffer + 19 + 8 * i, row[0], row[1], row[2]);
		}

		for (int i = 0; i < 3; i++)
		{
			SInt32 row[3];
			for (int j = 0; j < 3; j++)
			{
				float val = GyroMatrix.M[i][j];
				if (i == j)
					val -= 1.0f;
				row[j] = SInt32(val * sensorMax);
			}
			PackSensor(Buffer + 43 + 8 * i, row[0], row[1], row[2]);
		}

		Alg::EncodeSInt16(Buffer + 67, SInt16(Temperature * 100.0f));
	}

	void Unpack()
	{
		static const float sensorMax = (1 << 20) - 1;
		SInt32 x, y, z;

		UnpackSensor(Buffer + 3, &x, &y, &z);
		AccelOffset.y = (float) y * 1e-4f;
		AccelOffset.z = (float) z * 1e-4f;
		AccelOffset.x = (float) x * 1e-4f;

		UnpackSensor(Buffer + 11, &x, &y, &z);
		GyroOffset.x = (float) x * 1e-4f;
		GyroOffset.y = (float) y * 1e-4f;
		GyroOffset.z = (float) z * 1e-4f;

		AccelMatrix.SetIdentity();
		for (int i = 0; i < 3; i++)
		{
			UnpackSensor(Buffer + 19 + 8 * i, &x, &y, &z);
			AccelMatrix.M[i][0] = (float) x / sensorMax;
			AccelMatrix.M[i][1] = (float) y / sensorMax;
			AccelMatrix.M[i][2] = (float) z / sensorMax;
			AccelMatrix.M[i][i] += 1.0f;
		}

		GyroMatrix.SetIdentity();
		for (int i = 0; i < 3; i++)
		{
			UnpackSensor(Buffer + 43 + 8 * i, &x, &y, &z);
			GyroMatrix.M[i][0] = (float) x / sensorMax;
			GyroMatrix.M[i][1] = (float) y / sensorMax;
			GyroMatrix.M[i][2] = (float) z / sensorMax;
			GyroMatrix.M[i][i] += 1.0f;
		}

		Temperature = (float) DecodeSInt16(Buffer + 67) / 100.0f;
	}	
};

// SensorKeepAlive - feature report that needs to be sent at regular intervals for sensor
// to receive commands.
struct SensorKeepAliveImpl
{
    enum  { PacketSize = 5 };
    UByte   Buffer[PacketSize];

    UInt16  CommandId;
    UInt16  KeepAliveIntervalMs;

    SensorKeepAliveImpl(UInt16 interval = 0, UInt16 commandId = 0)
        : CommandId(commandId), KeepAliveIntervalMs(interval)
    {
        Pack();
    }

    void  Pack()
    {
        Buffer[0] = 8;
        Buffer[1] = UByte(CommandId & 0xFF);
        Buffer[2] = UByte(CommandId >> 8);
        Buffer[3] = UByte(KeepAliveIntervalMs & 0xFF);
        Buffer[4] = UByte(KeepAliveIntervalMs >> 8);
    }

    void Unpack()
    {
        CommandId          = Buffer[1] | (UInt16(Buffer[2]) << 8);
        KeepAliveIntervalMs= Buffer[3] | (UInt16(Buffer[4]) << 8);
    }
};

// Serial feature report.
struct SerialImpl
{
    enum  { PacketSize = 15 };
    UByte   Buffer[PacketSize];

    SerialReport Settings;

	SerialImpl()
	{
	memset(Buffer, 0, sizeof(Buffer));
	Buffer[0] = 10;
	}

	SerialImpl(const SerialReport& settings)
			:Settings(settings)
	{
		Pack();
	}

	void  Pack()
	{
    Buffer[0] = 10;
    Alg::EncodeUInt16(Buffer+1, Settings.CommandId);
	for (int i = 0; i < Settings.SERIAL_NUMBER_SIZE; ++i)
		Buffer[3 + i] = Settings.SerialNumberValue[i];
	}

	void Unpack()
	{
		Settings.CommandId = Alg::DecodeUInt16(Buffer+1);
		for (int i = 0; i < Settings.SERIAL_NUMBER_SIZE; ++i)
			Settings.SerialNumberValue[i] = Buffer[3 + i];
	}
};

// UUID feature report.
struct UUIDImpl
{
    enum  { PacketSize = 23 };
    UByte   Buffer[PacketSize];

    UUIDReport Settings;

	UUIDImpl()
	{
		memset(Buffer, 0, sizeof(Buffer));
		Buffer[0] = 19;
	}

    UUIDImpl(const UUIDReport& settings)
        :   Settings(settings)
    {
        Pack();
    }

    void  Pack()
    {
        Buffer[0] = 19;
        EncodeUInt16(Buffer+1, Settings.CommandId);
		for (int i = 0; i < 20; ++i)
			Buffer[3 + i] = Settings.UUIDValue[i];
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer+1);
		for (int i = 0; i < 20; ++i)
			Settings.UUIDValue[i] = Buffer[3 + i];
    }
};

// Temperature feature report.
struct TemperatureImpl
{
    enum  { PacketSize = 24 };
    UByte   Buffer[PacketSize];
    
    TemperatureReport Settings;

	TemperatureImpl()
	{
		memset(Buffer, 0, sizeof(Buffer));
		Buffer[0] = 20;
	}

    TemperatureImpl(const TemperatureReport& settings)
		:	Settings(settings)
    {
		Pack();
	}

    void  Pack()
    {

        Buffer[0] = 20;
        Alg::EncodeUInt16(Buffer + 1, Settings.CommandId);
        Buffer[3] = Settings.Version;

        Buffer[4] = Settings.NumBins;
        Buffer[5] = Settings.Bin;
        Buffer[6] = Settings.NumSamples;
        Buffer[7] = Settings.Sample;

        Alg::EncodeSInt16(Buffer + 8 , SInt16(Settings.TargetTemperature * 1e2));
        Alg::EncodeSInt16(Buffer + 10, SInt16(Settings.ActualTemperature * 1e2));

        Alg::EncodeUInt32(Buffer + 12, Settings.Time);

        Vector3d offset = Settings.Offset * 1e4;
        PackSensor(Buffer + 16, (SInt32) offset.x, (SInt32) offset.y, (SInt32) offset.z);
    }

    void Unpack()
    {
        Settings.CommandId = DecodeUInt16(Buffer + 1);
        Settings.Version = Buffer[3];

        Settings.NumBins    = Buffer[4];
        Settings.Bin        = Buffer[5];
        Settings.NumSamples = Buffer[6];
        Settings.Sample     = Buffer[7];

        Settings.TargetTemperature = DecodeSInt16(Buffer + 8) * 1e-2;
        Settings.ActualTemperature = DecodeSInt16(Buffer + 10) * 1e-2;

        Settings.Time = DecodeUInt32(Buffer + 12);

        SInt32 x, y, z;
        UnpackSensor(Buffer + 16, &x, &y, &z);
        Settings.Offset = Vector3d(x, y, z) * 1e-4;
    }
};

// GyroOffset feature report.
struct GyroOffsetImpl
{
    enum  { PacketSize = 18 };
    UByte   Buffer[PacketSize];

    GyroOffsetReport Settings;

    GyroOffsetImpl()
    {
        memset(Buffer, 0, sizeof(Buffer));
        Buffer[0] = 21;
    }

   GyroOffsetImpl(const GyroOffsetReport& settings)
		:	Settings(settings)
    {
		Pack();
	}

    void  Pack()
    {

        Buffer[0] = 21;
        Buffer[1] = UByte(Settings.CommandId & 0xFF);
        Buffer[2] = UByte(Settings.CommandId >> 8);
        Buffer[3] = UByte(Settings.Version);

		Vector3d offset = Settings.Offset * 1e4;
		PackSensor(Buffer + 4, (SInt32) offset.x, (SInt32) offset.y, (SInt32) offset.z);

        Alg::EncodeSInt16(Buffer + 16, SInt16(Settings.Temperature * 1e2));
    }

    void Unpack()
    {
        Settings.CommandId   = DecodeUInt16(Buffer + 1);
        Settings.Version     = GyroOffsetReport::VersionEnum(Buffer[3]);

        SInt32 x, y, z;
        UnpackSensor(Buffer + 4, &x, &y, &z);
        Settings.Offset      = Vector3d(x, y, z) * 1e-4f;

        Settings.Temperature = DecodeSInt16(Buffer + 16) * 1e-2;
    }
};

//-------------------------------------------------------------------------------------
// ***** SensorDisplayInfoImpl
SensorDisplayInfoImpl::SensorDisplayInfoImpl()
 :  CommandId(0), DistortionType(Base_None)
{
    memset(Buffer, 0, PacketSize);
    Buffer[0] = 9;
}

void SensorDisplayInfoImpl::Unpack()
{
    CommandId               = Buffer[1] | (UInt16(Buffer[2]) << 8);
    DistortionType          = Buffer[3];
    Resolution.H            = DecodeUInt16(Buffer+4);
    Resolution.V            = DecodeUInt16(Buffer+6);
    ScreenSize.H            = DecodeUInt32(Buffer+8) *  (1/1000000.f);
    ScreenSize.V            = DecodeUInt32(Buffer+12) * (1/1000000.f);
    VCenter                 = DecodeUInt32(Buffer+16) * (1/1000000.f);
    LensSeparation          = DecodeUInt32(Buffer+20) * (1/1000000.f);
    // No longer used - user-configured instead. Preserved for for documentation.
    //EyeToScreenDistance[0]  = DecodeUInt32(Buffer+24) * (1/1000000.f);
    //EyeToScreenDistance[1]  = DecodeUInt32(Buffer+28) * (1/1000000.f);
    // TODO: add DistortionEqn
    // TODO: currently these values are all zeros and the distortion is hard-coded in HMDDeviceCreateDesc::GetDeviceInfo()
    DistortionK[0]          = DecodeFloat(Buffer+32);
    DistortionK[1]          = DecodeFloat(Buffer+36);
    DistortionK[2]          = DecodeFloat(Buffer+40);
    DistortionK[3]          = DecodeFloat(Buffer+44);
    DistortionK[4]          = DecodeFloat(Buffer+48);
    DistortionK[5]          = DecodeFloat(Buffer+52);
    // TODO: add DistortionEqn
}


//-------------------------------------------------------------------------------------
// ***** SensorDeviceFactory

SensorDeviceFactory &SensorDeviceFactory::GetInstance()
{
	static SensorDeviceFactory instance;
	return instance;
}

void SensorDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{

    class SensorEnumerator : public HIDEnumerateVisitor
    {
        // Assign not supported; suppress MSVC warning.
        void operator = (const SensorEnumerator&) { }

        DeviceFactory*     pFactory;
        EnumerateVisitor&  ExternalVisitor;   
    public:
        SensorEnumerator(DeviceFactory* factory, EnumerateVisitor& externalVisitor)
            : pFactory(factory), ExternalVisitor(externalVisitor) { }

        virtual bool MatchVendorProduct(UInt16 vendorId, UInt16 productId)
        {
            return pFactory->MatchVendorProduct(vendorId, productId);
        }

        virtual void Visit(HIDDevice& device, const HIDDeviceDesc& desc)
        {
            
            if (desc.ProductId == Sensor_BootLoader)
            {   // If we find a sensor in boot loader mode then notify the app
                // about the existence of the device, but don't allow the app
                // to create or access the device
                BootLoaderDeviceCreateDesc createDesc(pFactory, desc);
                ExternalVisitor.Visit(createDesc);
                return;
            }
            
            SensorDeviceCreateDesc createDesc(pFactory, desc);
            ExternalVisitor.Visit(createDesc);

            // Check if the sensor returns DisplayInfo. If so, try to use it to override potentially
            // mismatching monitor information (in case wrong EDID is reported by splitter),
            // or to create a new "virtualized" HMD Device.
            
            SensorDisplayInfoImpl displayInfo;

            if (device.GetFeatureReport(displayInfo.Buffer, SensorDisplayInfoImpl::PacketSize))
            {
                displayInfo.Unpack();

                // If we got display info, try to match / create HMDDevice as well
                // so that sensor settings give preference.
                if (displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt)
                {
                    SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo(displayInfo, ExternalVisitor);
                }
            }
        }
    };

    //double start = Timer::GetProfileSeconds();

    SensorEnumerator sensorEnumerator(this, visitor);
    GetManagerImpl()->GetHIDDeviceManager()->Enumerate(&sensorEnumerator);

    //double totalSeconds = Timer::GetProfileSeconds() - start; 
}

bool SensorDeviceFactory::MatchVendorProduct(UInt16 vendorId, UInt16 productId) const
{
    return 	((vendorId == Oculus_VendorId) && (productId == Device_Tracker_ProductId)) ||
    		((vendorId == Oculus_VendorId) && (productId == Device_KTracker_ProductId)) ||
			((vendorId == Samsung_VendorId) && (productId == Device_KTracker_Samsung_ProductId_0)) ||
			((vendorId == Samsung_VendorId) && (productId == Device_KTracker_Samsung_ProductId_1)) ||
			((vendorId == Samsung_VendorId) && (productId == Device_KTracker_Samsung_ProductId_2)) ||
			((vendorId == Samsung_VendorId) && (productId == Device_KTracker_Samsung_ProductId_3)) ||
			((vendorId == Samsung_VendorId) && (productId == Device_KTracker_Samsung_ProductId_4)) ||
			((vendorId == Samsung_VendorId) && (productId == Device_KTracker_Samsung_ProductId_5));
}

bool SensorDeviceFactory::DetectHIDDevice(DeviceManager* pdevMgr, const HIDDeviceDesc& desc)
{
    if (MatchVendorProduct(desc.VendorId, desc.ProductId))
    {
        if (desc.ProductId == Sensor_BootLoader)
        {   // If we find a sensor in boot loader mode then notify the app
            // about the existence of the device, but don't allow them
            // to create or access the device
            BootLoaderDeviceCreateDesc createDesc(this, desc);
            pdevMgr->AddDevice_NeedsLock(createDesc);
            return false;  // return false to allow upstream boot loader factories to catch the device
        }
        else
        {
            SensorDeviceCreateDesc createDesc(this, desc);
            return pdevMgr->AddDevice_NeedsLock(createDesc).GetPtr() != NULL;
        }
    }
    return false;
}

//-------------------------------------------------------------------------------------
// ***** SensorDeviceCreateDesc

DeviceBase* SensorDeviceCreateDesc::NewDeviceInstance()
{
    return new SensorDeviceImpl(this);
}

bool SensorDeviceCreateDesc::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_Sensor) &&
        (info->InfoClassType != Device_None))
        return false;

    OVR_strcpy(info->ProductName,  DeviceInfo::MaxNameLength, HIDDesc.Product.ToCStr());
    OVR_strcpy(info->Manufacturer, DeviceInfo::MaxNameLength, HIDDesc.Manufacturer.ToCStr());
    info->Type    = Device_Sensor;

    if (info->InfoClassType == Device_Sensor)
    {
        SensorInfo* sinfo = (SensorInfo*)info;
        sinfo->VendorId  = HIDDesc.VendorId;
        sinfo->ProductId = HIDDesc.ProductId;
        sinfo->Version   = HIDDesc.VersionNumber;
        sinfo->MaxRanges = SensorRangeImpl::GetMaxSensorRange();
        OVR_strcpy(sinfo->SerialNumber, sizeof(sinfo->SerialNumber),HIDDesc.SerialNumber.ToCStr());
    }
    return true;
}

//-------------------------------------------------------------------------------------
// ***** SensorDevice

SensorDeviceImpl::SensorDeviceImpl(SensorDeviceCreateDesc* createDesc)
    : OVR::HIDDeviceImpl<OVR::SensorDevice>(createDesc, 0),
      Coordinates(SensorDevice::Coord_Sensor),
      HWCoordinates(SensorDevice::Coord_HMD), // HW reports HMD coordinates by default.
      NextKeepAliveTickSeconds(0),
      FullTimestamp(0),
      RealTimeDelta(0.0),
      MaxValidRange(SensorRangeImpl::GetMaxSensorRange()),
	  pCalibration(NULL)
{
    SequenceValid  = false;
    LastSampleCount= 0;
    LastTimestamp   = 0;

    OldCommandId = 0;

    pPhoneSensors = PhoneSensors::Create();
	
	SensorInfo si;
	createDesc->GetDeviceInfo(&si);

	bool deviceSupportsPhoneTemperatureTable =	false;	// Support means that we can read the device serial number via our feature report 'Serial'
														// to allow us to associate the temperature table with a device.

	if (si.VendorId == Oculus_VendorId &&
		si.ProductId == Device_KTracker_ProductId)
	{
		// When using the old Oculus id's we used the version to indicate support.
		UInt32 versionValue = si.Version & 0x00ff;	// LDC - We're using the top two bytes as flags and the bottom two as version number.
		if (versionValue == 2 || versionValue >= 4)
		{
			deviceSupportsPhoneTemperatureTable = true;
		}
	}
	else if (	si.VendorId == Samsung_VendorId &&
				(si.ProductId == Device_KTracker_Samsung_ProductId_0 ||
				 si.ProductId == Device_KTracker_Samsung_ProductId_1 ||
				 si.ProductId == Device_KTracker_Samsung_ProductId_2 ||
				 si.ProductId == Device_KTracker_Samsung_ProductId_3 ||
				 si.ProductId == Device_KTracker_Samsung_ProductId_4 ||
				 si.ProductId == Device_KTracker_Samsung_ProductId_5))
	{
		// Newer firmware supports temp table.
		deviceSupportsPhoneTemperatureTable = true;
	}

	if (deviceSupportsPhoneTemperatureTable)
	{
		pCalibration = 	new SensorCalibration(this);
	}
}

SensorDeviceImpl::~SensorDeviceImpl()
{
    // Check that Shutdown() was called.
    OVR_ASSERT(!pCreateDesc->pDevice);

	delete pCalibration;
}

// Internal creation APIs.
bool SensorDeviceImpl::Initialize(DeviceBase* parent)
{
    if (HIDDeviceImpl<OVR::SensorDevice>::Initialize(parent))
    {
        openDevice();

        LogText("OVR::SensorDevice initialized.\n");

        return true;
    }

    return false;
}

void SensorDeviceImpl::openDevice()
{

    // Read the currently configured range from sensor.
    SensorRangeImpl sr(SensorRange(), 0);

    if (GetInternalDevice()->GetFeatureReport(sr.Buffer, SensorRangeImpl::PacketSize))
    {
        sr.Unpack();
        sr.GetSensorRange(&CurrentRange);
        // Increase the magnetometer range, since the default value is not enough in practice
        CurrentRange.MaxMagneticField = 2.5f;
        setRange(CurrentRange);
    }

	// Read the currently configured calibration from sensor.
	if (pCalibration != NULL)
	{
		SensorFactoryCalibrationImpl sc;
		if (GetInternalDevice()->GetFeatureReport(sc.Buffer, SensorFactoryCalibrationImpl::PacketSize))
		{
			sc.Unpack();
			AccelCalibrationOffset = sc.AccelOffset;
			GyroCalibrationOffset  = sc.GyroOffset;
			AccelCalibrationMatrix = sc.AccelMatrix;
			GyroCalibrationMatrix  = sc.GyroMatrix;
			CalibrationTemperature = sc.Temperature;
		}
	}

    // If the sensor has "DisplayInfo" data, use HMD coordinate frame by default.
    SensorDisplayInfoImpl displayInfo;
    if (GetInternalDevice()->GetFeatureReport(displayInfo.Buffer, SensorDisplayInfoImpl::PacketSize))
    {
        displayInfo.Unpack();
        Coordinates = (displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt) ?
                      Coord_HMD : Coord_Sensor;
    }

    // Read/Apply sensor config.
    setCoordinateFrame(Coordinates);
    setReportRate(Sensor_DefaultReportRate);
	if (pCalibration != NULL)
	{
		setOnboardCalibrationEnabled(false);
	}
	
	
    // Set Keep-alive at 10 seconds.
    SensorKeepAliveImpl skeepAlive(10 * 1000);
    GetInternalDevice()->SetFeatureReport(skeepAlive.Buffer, SensorKeepAliveImpl::PacketSize);

	if (pCalibration != NULL)
	{
		// Read the temperature data from the device.
		StringBuffer str;

#if 0
		// Get device code from uuid.
		UUIDReport uuid;
		if (!getUUIDReport(&uuid))
		{	
			LogText("OVR::SensorDeviceImpl::openDevice - failed to get device uuid.\n");
		}

		// Convert to string.
		for (int i=0; i<UUIDReport::UUID_SIZE; i++)
		{
			str.AppendFormat("%02X", uuid.UUIDValue[i]);		
		}
#else
		// Get device code from serial number.
		SerialReport serial;
		if (!getSerialReport(&serial))
		{	
			LogText("OVR::SensorDeviceImpl::openDevice - failed to get device uuid.\n");
		}

		// Convert to string.
		for (int i=0; i<SerialReport::SERIAL_NUMBER_SIZE; i++)
		{
			str.AppendFormat("%02X", serial.SerialNumberValue[i]);		
		}
#endif
		
		LogText("OVR::SensorDeviceImpl::openDevice - with serial code '%s'.\n", str.ToCStr());
		
		pCalibration->Initialize(String(str));
	}
}

void SensorDeviceImpl::closeDeviceOnError()
{
    LogText("OVR::SensorDevice - Lost connection to '%s'\n", getHIDDesc()->Path.ToCStr());
    NextKeepAliveTickSeconds = 0;
}

void SensorDeviceImpl::Shutdown()
{   
    HIDDeviceImpl<OVR::SensorDevice>::Shutdown();

    LogText("OVR::SensorDevice - Closed '%s'\n", getHIDDesc()->Path.ToCStr());
}


void SensorDeviceImpl::OnInputReport(UByte* pData, UInt32 length)
{

    bool processed = false;
    if (!processed)
    {

        TrackerMessage message;
        if (DecodeTrackerMessage(&message, pData, length))
        {
            processed = true;
            onTrackerMessage(&message);
        }
    }
}

double SensorDeviceImpl::OnTicks(double tickSeconds)
{
    if (tickSeconds >= NextKeepAliveTickSeconds)
    {
        // Use 3-seconds keep alive by default.
        double keepAliveDelta = 3.0;

        // Set Keep-alive at 10 seconds.
        SensorKeepAliveImpl skeepAlive(10 * 1000);
        // OnTicks is called from background thread so we don't need to add this to the command queue.
        GetInternalDevice()->SetFeatureReport(skeepAlive.Buffer, SensorKeepAliveImpl::PacketSize);

		// Emit keep-alive every few seconds.
        NextKeepAliveTickSeconds = tickSeconds + keepAliveDelta;
    }
    return NextKeepAliveTickSeconds - tickSeconds;
}

bool SensorDeviceImpl::SetRange(const SensorRange& range, bool waitFlag)
{
    bool                 result = 0;
    ThreadCommandQueue * threadQueue = GetManagerImpl()->GetThreadQueue();

    if (!waitFlag)
    {
        return threadQueue->PushCall(this, &SensorDeviceImpl::setRange, range);
    }
    
    if (!threadQueue->PushCallAndWaitResult(this, 
                                            &SensorDeviceImpl::setRange,
                                            &result, 
                                            range))
    {
        return false;
    }

    return result;
}

void SensorDeviceImpl::GetRange(SensorRange* range) const
{
    Lock::Locker lockScope(GetLock());
    *range = CurrentRange;
}

bool SensorDeviceImpl::setRange(const SensorRange& range)
{
    SensorRangeImpl sr(range);
    
    if (GetInternalDevice()->SetFeatureReport(sr.Buffer, SensorRangeImpl::PacketSize))
    {
        Lock::Locker lockScope(GetLock());
        sr.GetSensorRange(&CurrentRange);
        return true;
    }
    
    return false;
}

void SensorDeviceImpl::SetCoordinateFrame(CoordinateFrame coordframe)
{ 
    // Push call with wait.
    GetManagerImpl()->GetThreadQueue()->
        PushCall(this, &SensorDeviceImpl::setCoordinateFrame, coordframe, true);
}

SensorDevice::CoordinateFrame SensorDeviceImpl::GetCoordinateFrame() const
{
    return Coordinates;
}

Void SensorDeviceImpl::setCoordinateFrame(CoordinateFrame coordframe)
{

    Coordinates = coordframe;

    // Read the original coordinate frame, then try to change it.
    SensorConfigImpl scfg;
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
    }

    scfg.SetSensorCoordinates(coordframe == Coord_Sensor);
    scfg.Pack();

    GetInternalDevice()->SetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize);
    
    // Re-read the state, in case of older firmware that doesn't support Sensor coordinates.
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
        HWCoordinates = scfg.IsUsingSensorCoordinates() ? Coord_Sensor : Coord_HMD;
    }
    else
    {
        HWCoordinates = Coord_HMD;
    }
    return 0;
}

void SensorDeviceImpl::SetReportRate(unsigned rateHz)
{ 
    // Push call with wait.
    GetManagerImpl()->GetThreadQueue()->
        PushCall(this, &SensorDeviceImpl::setReportRate, rateHz, true);
}

unsigned SensorDeviceImpl::GetReportRate() const
{
    // Read the original configuration
    SensorConfigImpl scfg;
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
        return Sensor_MaxReportRate / (scfg.PacketInterval + 1);
    }
    return 0; // error
}

Void SensorDeviceImpl::setReportRate(unsigned rateHz)
{
    // Read the original configuration
    SensorConfigImpl scfg;
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
    }

    if (rateHz > Sensor_MaxReportRate)
        rateHz = Sensor_MaxReportRate;
    else if (rateHz == 0)
        rateHz = Sensor_DefaultReportRate;

    scfg.PacketInterval = UInt16((Sensor_MaxReportRate / rateHz) - 1);

    scfg.Pack();

    GetInternalDevice()->SetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize);
    return 0;
}

void SensorDeviceImpl::GetFactoryCalibration(Vector3f* AccelOffset, Vector3f* GyroOffset,
                                             Matrix4f* AccelMatrix, Matrix4f* GyroMatrix, 
                                             float* Temperature)
{
    *AccelOffset = AccelCalibrationOffset;
    *GyroOffset  = GyroCalibrationOffset;
    *AccelMatrix = AccelCalibrationMatrix;
    *GyroMatrix  = GyroCalibrationMatrix;
    *Temperature = CalibrationTemperature;
}

bool SensorDeviceImpl::SetOnboardCalibrationEnabled(bool enabled)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &SensorDeviceImpl::setOnboardCalibrationEnabled, &result, enabled))
	{
		return false;
	}

	return result;
}

bool SensorDeviceImpl::setOnboardCalibrationEnabled(bool enabled)
{
    // Read the original configuration
    SensorConfigImpl scfg;
    if (GetInternalDevice()->GetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize))
    {
        scfg.Unpack();
    }

    if (enabled)
        scfg.Flags |= (SensorConfigImpl::Flag_AutoCalibration | SensorConfigImpl::Flag_UseCalibration);
    else
        scfg.Flags &= ~(SensorConfigImpl::Flag_AutoCalibration | SensorConfigImpl::Flag_UseCalibration);

	scfg.Pack();

    return GetInternalDevice()->SetFeatureReport(scfg.Buffer, SensorConfigImpl::PacketSize);
}

void SensorDeviceImpl::SetMessageHandler(MessageHandler* handler)
{
    if (handler)
    {
        SequenceValid = false;
        DeviceBase::SetMessageHandler(handler);
    }
    else
    {       
        DeviceBase::SetMessageHandler(handler);
    }    
}

// Sensor reports data in the following coordinate system:
// Accelerometer: 10^-4 m/s^2; X forward, Y right, Z Down.
// Gyro:          10^-4 rad/s; X positive roll right, Y positive pitch up; Z positive yaw right.


// We need to convert it to the following RHS coordinate system:
// X right, Y Up, Z Back (out of screen)
//
Vector3f AccelFromBodyFrameUpdate(const TrackerSensors& update, UByte sampleNumber,
                                  bool convertHMDToSensor = false)
{
    const TrackerSample& sample = update.Samples[sampleNumber];
    float                ax = (float)sample.AccelX;
    float                ay = (float)sample.AccelY;
    float                az = (float)sample.AccelZ;

    Vector3f val = convertHMDToSensor ? Vector3f(ax, az, -ay) :  Vector3f(ax, ay, az);
    return val * 0.0001f;
}


Vector3f MagFromBodyFrameUpdate(const TrackerSensors& update,
                                bool convertHMDToSensor = false)
{   
    // Note: Y and Z are swapped in comparison to the Accel.  
    // This accounts for DK1 sensor firmware axis swap, which should be undone in future releases.
    if (!convertHMDToSensor)
    {
        return Vector3f( (float)update.MagX,
                         (float)update.MagZ,
                         (float)update.MagY) * 0.0001f;
    }    

    return Vector3f( (float)update.MagX,
                     (float)update.MagY,
                    -(float)update.MagZ) * 0.0001f;
}

Vector3f EulerFromBodyFrameUpdate(const TrackerSensors& update, UByte sampleNumber,
                                  bool convertHMDToSensor = false)
{
    const TrackerSample& sample = update.Samples[sampleNumber];
    float                gx = (float)sample.GyroX;
    float                gy = (float)sample.GyroY;
    float                gz = (float)sample.GyroZ;

    Vector3f val = convertHMDToSensor ? Vector3f(gx, gz, -gy) :  Vector3f(gx, gy, gz);
    return val * 0.0001f;
}


bool SensorDeviceImpl::SetSerialReport(const SerialReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &SensorDeviceImpl::setSerialReport, &result, data))
	{
		return false;
	}

	return result;
}

bool SensorDeviceImpl::setSerialReport(const SerialReport& data)
{
    SerialImpl di(data);
    return GetInternalDevice()->SetFeatureReport(di.Buffer, SerialImpl::PacketSize);
}

bool SensorDeviceImpl::GetSerialReport(SerialReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &SensorDeviceImpl::getSerialReport, &result, data))
	{
		return false;
	}

	return result;
}

bool SensorDeviceImpl::getSerialReport(SerialReport* data)
{
    SerialImpl di;
    if (GetInternalDevice()->GetFeatureReport(di.Buffer, SerialImpl::PacketSize))
    {
        di.Unpack();
        *data = di.Settings;
        return true;
    }

    return false;
}

bool SensorDeviceImpl::SetUUIDReport(const UUIDReport& data)
{ 
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &SensorDeviceImpl::setUUIDReport, &result, data))
	{
		return false;
	}

	return result;
}

bool SensorDeviceImpl::setUUIDReport(const UUIDReport& data)
{
    UUIDImpl ui(data);
    return GetInternalDevice()->SetFeatureReport(ui.Buffer, UUIDImpl::PacketSize);
}

bool SensorDeviceImpl::GetUUIDReport(UUIDReport* data)
{
	bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &SensorDeviceImpl::getUUIDReport, &result, data))
	{
		return false;
	}

	return result;
}

bool SensorDeviceImpl::getUUIDReport(UUIDReport* data)
{
    UUIDImpl ui;
    if (GetInternalDevice()->GetFeatureReport(ui.Buffer, UUIDImpl::PacketSize))
    {
        ui.Unpack();
        *data = ui.Settings;
        return true;
    }

    return false;
}

bool SensorDeviceImpl::SetTemperatureReport(const TemperatureReport& data)
{
    // direct call if we are already on the device manager thread
    if (GetCurrentThreadId() == GetManagerImpl()->GetThreadId())
    {
        return setTemperatureReport(data);
    }

    bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &SensorDeviceImpl::setTemperatureReport, &result, data))
	{
		return false;
	}

	return result;
}

bool SensorDeviceImpl::setTemperatureReport(const TemperatureReport& data)
{
    TemperatureImpl ti(data);
    return GetInternalDevice()->SetFeatureReport(ti.Buffer, TemperatureImpl::PacketSize);
}

bool SensorDeviceImpl::getTemperatureReport(TemperatureReport* data)
{
    TemperatureImpl ti;
    if (GetInternalDevice()->GetFeatureReport(ti.Buffer, TemperatureImpl::PacketSize))
    {
        ti.Unpack();
        *data = ti.Settings;
        return true;
    }

    return false;
}

bool SensorDeviceImpl::GetAllTemperatureReports(Array<Array<TemperatureReport> >* data)
{
    // direct call if we are already on the device manager thread
    if (GetCurrentThreadId() == GetManagerImpl()->GetThreadId())
    {
        return getAllTemperatureReports(data);
    }

    bool result;
    if (!GetManagerImpl()->GetThreadQueue()->
        PushCallAndWaitResult(this, &SensorDeviceImpl::getAllTemperatureReports, &result, data))
    {
        return false;
    }

    return result;
}

bool SensorDeviceImpl::getAllTemperatureReports(Array<Array<TemperatureReport> >* data)
{
    TemperatureReport t;
    bool result = getTemperatureReport(&t);
    if (!result)
        return false;

    int bins = t.NumBins, samples = t.NumSamples;
    data->Clear();
    data->Resize(bins);
    for (int i = 0; i < bins; i++)
        (*data)[i].Resize(samples);

    for (int i = 0; i < bins; i++)
        for (int j = 0; j < samples; j++)
        {
            result = getTemperatureReport(&t);
            if (!result)
                return false;
            OVR_ASSERT(t.NumBins == bins && t.NumSamples == samples);

            (*data)[t.Bin][t.Sample] = t;
        }
    return true;
}

bool SensorDeviceImpl::GetGyroOffsetReport(GyroOffsetReport* data)
{
    // direct call if we are already on the device manager thread
    if (GetCurrentThreadId() == GetManagerImpl()->GetThreadId())
    {
        return getGyroOffsetReport(data);
    }

    bool result;
	if (!GetManagerImpl()->GetThreadQueue()->
            PushCallAndWaitResult(this, &SensorDeviceImpl::getGyroOffsetReport, &result, data))
	{
		return false;
	}

	return result;
}

bool SensorDeviceImpl::getGyroOffsetReport(GyroOffsetReport* data)
{
    GyroOffsetImpl goi;
    if (GetInternalDevice()->GetFeatureReport(goi.Buffer, GyroOffsetImpl::PacketSize))
    {
        goi.Unpack();
        *data = goi.Settings;
        return true;
    }

    return false;
}

// MA: #define to enable John's old absoluteTime code instead of SensorTimeFilter;
// to be eliminated soon.
//#define OVR_OLD_TIMING_LOGIC


void SensorDeviceImpl::onTrackerMessage(TrackerMessage* message)
{
    if (message->Type != TrackerMessage_Sensors)
        return;
    
    const double    timeUnit        = (1.0 / 1000.0);
    double          scaledTimeUnit  = timeUnit;
    TrackerSensors& s               = message->Sensors;
    // DK1 timestamps the first sample, so the actual device time will be later
    // by the time we get the message if there are multiple samples.
    int             timestampAdjust = (s.SampleCount > 0) ? s.SampleCount-1 : 0;

    // Call OnMessage() within a lock to avoid conflicts with handlers.
    Lock::Locker scopeLock(HandlerRef.GetLock());

    const double now                 = OVR::Timer::GetSeconds();
    double absoluteTimeSeconds       = 0.0;
    

    if (SequenceValid)
    {
        unsigned timestampDelta;

        if (s.Timestamp < LastTimestamp)
        {
        	// The timestamp rolled around the 16 bit counter, so FullTimeStamp
        	// needs a high word increment.
        	FullTimestamp += 0x10000;
            timestampDelta = ((((int)s.Timestamp) + 0x10000) - (int)LastTimestamp);
        }
        else
        {
            timestampDelta = (s.Timestamp - LastTimestamp);
        }
        // Update the low word of FullTimeStamp
        FullTimestamp = ( FullTimestamp & ~0xffff ) | s.Timestamp;

       
#ifdef OVR_OLD_TIMING_LOGIC
        // If this timestamp, adjusted by our best known delta, would
        // have the message arriving in the future, we need to adjust
        // the delta down.
        if ( FullTimestamp * timeUnit + RealTimeDelta > now )
        {
        	RealTimeDelta = now - ( FullTimestamp * timeUnit );
        }
        else
        {
        	// Creep the delta by 100 microseconds so we are always pushing
        	// it slightly towards the high clamping case, instead of having to
        	// worry about clock drift in both directions.
        	RealTimeDelta += 0.0001;
        }
        // This will be considered the absolute time of the last sample in
        // the message.  If we are double or tripple stepping the samples,
        // their absolute times will be adjusted backwards.
        absoluteTimeSeconds = FullTimestamp * timeUnit + RealTimeDelta;

#else

        double deviceTime   = (FullTimestamp + timestampAdjust) * timeUnit;
        absoluteTimeSeconds = TimeFilter.SampleToSystemTime(deviceTime, now);
        scaledTimeUnit      = TimeFilter.ScaleTimeUnit(timeUnit);

#endif
        
        // If we missed a small number of samples, replicate the last sample.
        if ((timestampDelta > LastSampleCount) && (timestampDelta <= 254))
        {
            if (HandlerRef.GetHandler())
            {
                MessageBodyFrame sensors(this);

                sensors.AbsoluteTimeSeconds = absoluteTimeSeconds - LastSampleCount * scaledTimeUnit;
                sensors.TimeDelta           = (float)((timestampDelta - LastSampleCount) * scaledTimeUnit);
                sensors.Acceleration        = LastAcceleration;
                sensors.RotationRate        = LastRotationRate;
                sensors.MagneticField       = LastMagneticField;
                sensors.MagneticBias        = LastMagneticBias;
                sensors.Temperature         = LastTemperature;

				if (pCalibration != NULL)
				{
					pCalibration->Apply(sensors);
				}

                HandlerRef.GetHandler()->OnMessage(sensors);
            }
        }
    }
    else
    {
        LastAcceleration = Vector3f(0);
        LastRotationRate = Vector3f(0);
        LastMagneticField = Vector3f(0);
        LastMagneticBias = Vector3f(0);
        LastTemperature  = 0;
        SequenceValid    = true;

        // This is our baseline sensor to host time delta,
        // it will be adjusted with each new message.
        FullTimestamp = s.Timestamp;

#ifdef OVR_OLD_TIMING_LOGIC
    	RealTimeDelta       = now - ( FullTimestamp * timeUnit );
        absoluteTimeSeconds = FullTimestamp * timeUnit + RealTimeDelta;
#else

        double deviceTime   = (FullTimestamp + timestampAdjust) * timeUnit;
        absoluteTimeSeconds = TimeFilter.SampleToSystemTime(deviceTime, now);
        scaledTimeUnit      = TimeFilter.ScaleTimeUnit(timeUnit);
#endif
    }

    LastSampleCount = s.SampleCount;
    LastTimestamp   = s.Timestamp;

    bool convertHMDToSensor = (Coordinates == Coord_Sensor) && (HWCoordinates == Coord_HMD);

    // LDC - Normally we get the coordinate system from the tracker.
    // Since KTracker doesn't store it we'll always assume HMD coordinate system.
    convertHMDToSensor = false;

    if (HandlerRef.GetHandler())
    {
        MessageBodyFrame sensors(this);                
        UByte            iterations = s.SampleCount;

        if (s.SampleCount > 3)
        {
            iterations        = 3;
            sensors.TimeDelta = (float)(s.SampleCount - 2) * scaledTimeUnit;
        }
        else
        {
            sensors.TimeDelta = (float)scaledTimeUnit;
        }

        for (UByte i = 0; i < iterations; i++)
        {     
            sensors.AbsoluteTimeSeconds = absoluteTimeSeconds - ( iterations - 1 - i ) * scaledTimeUnit;
            sensors.Acceleration  = AccelFromBodyFrameUpdate(s, i, convertHMDToSensor);
            sensors.RotationRate  = EulerFromBodyFrameUpdate(s, i, convertHMDToSensor);
            sensors.MagneticField = MagFromBodyFrameUpdate(s, convertHMDToSensor);
            replaceWithPhoneMag(&(sensors.MagneticField), &(sensors.MagneticBias));
            sensors.Temperature   = s.Temperature * 0.01f;

			if (pCalibration != NULL)
			{
				pCalibration->Apply(sensors);
			}

            HandlerRef.GetHandler()->OnMessage(sensors);

            // TimeDelta for the last two sample is always fixed.
            sensors.TimeDelta = (float)scaledTimeUnit;
        }

        LastAcceleration = sensors.Acceleration;
        LastRotationRate = sensors.RotationRate;
        LastMagneticField = sensors.MagneticField;
        LastMagneticBias = sensors.MagneticBias;
        LastTemperature = sensors.Temperature;
    }
    else
    {
        UByte i = (s.SampleCount > 3) ? 2 : (s.SampleCount - 1);
        LastAcceleration  = AccelFromBodyFrameUpdate(s, i, convertHMDToSensor);
        LastRotationRate  = EulerFromBodyFrameUpdate(s, i, convertHMDToSensor);
        LastMagneticField = MagFromBodyFrameUpdate(s, convertHMDToSensor);
        replaceWithPhoneMag(&LastMagneticField, &LastMagneticBias);
        LastTemperature   = s.Temperature * 0.01f;
    }
}

void SensorDeviceImpl::replaceWithPhoneMag(Vector3f* mag, Vector3f* bias)
{

	Vector3f magPhone;
	Vector3f biasPhone;
	pPhoneSensors->GetLatestUncalibratedMagAndBiasValue(&magPhone, &biasPhone);


	// Phone values are in micro-Tesla. Convert it to Gauss and flip axes.
	magPhone *= 10000.0f/1000000.0f;

	Vector3f resMag;
	resMag.x = -magPhone.y;
	resMag.y = magPhone.x;
	resMag.z = magPhone.z;

	*mag = resMag;


	biasPhone *= 10000.0f/1000000.0f;

	Vector3f resBias;
	resBias.x = -biasPhone.y;
	resBias.y = biasPhone.x;
	resBias.z = biasPhone.z;

	*bias = resBias;
}

} // namespace OVR


