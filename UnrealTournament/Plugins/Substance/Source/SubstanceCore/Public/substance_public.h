//! @file substance_public.h
//! @brief Substance Unreal Engine 4 API File
//! @author Josh Coyne - Allegorithmic
//! @copyright 2014 Allegorithmic Inc. All rights reserved.
#pragma once

#define WITH_SUBSTANCE				1
#define SUBSTANCE_PLATFORM_BLEND	1

#ifdef __cplusplus
#	define SUBSTANCE_EXTERNC extern "C"
#else
#	define SUBSTANCE_EXTERNC
#endif

#ifdef _WIN32
#	define DLL_EXPORT __declspec(dllexport)
#	define DLL_IMPORT __declspec(dllimport)
#elif __APPLE__ || __linux__
#	define DLL_EXPORT
#	define DLL_IMPORT
#endif

#if defined (SUBSTANCE_LIB_DYNAMIC)
#	if defined (SUBSTANCE_LIB)
#		define SUBSTANCELIB_API SUBSTANCE_EXTERNC DLL_EXPORT
#	else
#		define SUBSTANCELIB_API SUBSTANCE_EXTERNC DLL_IMPORT
#	endif
#else
#	define SUBSTANCELIB_API
#endif

#if defined (SUBSTANCE_LIB)

#include "substance_types.h"
#include "substance_private.h"

#else

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#	define SUBSTANCE_CALLBACK __stdcall
#else
#	define SUBSTANCE_CALLBACK
#endif


#if __PS4__ || _DURANGO
#define PTRINT unsigned int*
#endif

typedef PTRINT SubstanceContext;
typedef PTRINT SubstanceDevice;
typedef PTRINT SubstanceHandle;
typedef PTRINT SubstanceLinkerContext;
typedef PTRINT SubstanceLinkerHandle;
typedef PTRINT XMLElementHandle;

#define SUBSTANCE_GPU_COUNT_MAX 4
#define SUBSTANCE_CPU_COUNT_MAX 32
#define SUBSTANCE_SPU_COUNT_MAX 32
#define SUBSTANCE_MEMORYBUDGET_FORCEFLUSH 1

typedef enum
{
	Substance_Resource_Default = 0,      /**< Default value on this platform */
	Substance_Resource_DoNotUse,         /**< The resource must not be used */
	Substance_Resource_Partial1,         /**< Resource partial use: 1/8*/
	Substance_Resource_Partial2,         /**< Partial use 2/8 */
	Substance_Resource_Partial3,         /**< Partial use 3/8 */
	Substance_Resource_Partial4,         /**< Partial use 4/8 */
	Substance_Resource_Partial5,         /**< Partial use 5/8 */
	Substance_Resource_Partial6,         /**< Partial use 6/8 */
	Substance_Resource_Partial7,         /**< Resource partial use: 7/8*/
	Substance_Resource_FullUse,          /**< The resource can be used 100% */

} SubstanceHardSwitch;

typedef enum
{
	Substance_ChanOrder_NC = 0,
	Substance_ChanOrder_RGBA = 0xE4,
	Substance_ChanOrder_BGRA = 0xC6,
	Substance_ChanOrder_ABGR = 0x1B,
	Substance_ChanOrder_ARGB = 0x39
} SubstanceChannelsOrder;

typedef enum
{
	Substance_IType_Float = 0x0,
	Substance_IType_Float2 = 0x1,
	Substance_IType_Float3 = 0x2,
	Substance_IType_Float4 = 0x3,
	Substance_IType_Integer = 0x4,
	Substance_IType_Integer2 = 0x8,
	Substance_IType_Integer3 = 0x9,
	Substance_IType_Integer4 = 0xA,
	Substance_IType_Image = 0x5,
	Substance_IType_Mesh = 0x6,
	Substance_IType_String = 0x7
} SubstanceInputType;

typedef enum
{
	Substance_Linker_UIDCollision_Output = 0x0,
	Substance_Linker_UIDCollision_Input = 0x1   
} SubstanceLinkerUIDCollisionType;

typedef enum
{
	/* Format (2 bits) */
	Substance_PF_RAW   = 0x0,                    /**< Non-compressed flag */
	Substance_PF_BC    = 0x1,                    /**< DXT compression flag */
	Substance_PF_PVRTC = 0x3,                    /**< PVRTC compression flag */
	Substance_PF_ETC   = 0x3,                    /**< ETC compression flag */
	Substance_PF_Misc  = 0x2,                    /**< Other compression flag */
	Substance_PF_MASK_Format = 0x3,
	/* etc. */
	
	/* RAW format */

	/* -> RAW format channels (2 bits) */
	Substance_PF_RGBA = Substance_PF_RAW | 0x0,  /**< RGBA 4x8bits format */
	Substance_PF_RGBx = Substance_PF_RAW | 0x4,  /**< RGBx (3+1:pad)x8bits format */
	Substance_PF_RGB  = Substance_PF_RAW | 0x8,  /**< RGB 3x8bits format */
	Substance_PF_L    = Substance_PF_RAW | 0xC,  /**< Luminance 8bits format */
	Substance_PF_MASK_RAWChannels = 0xC,
	
	/* -> RAW format precision flags (2bits: combined to channels) */
	Substance_PF_8I   = 0x00,                    /**< 8bits integer */
	Substance_PF_16I  = 0x10,                    /**< 16bits integer flag */
	Substance_PF_16F  = 0x50,                    /**< 16bits half-float flag */
	Substance_PF_32F  = 0x40,                    /**< 32bits float flag */
	Substance_PF_MASK_RAWPrecision = 0x50,

	Substance_PF_MASK_RAWFormat = 
		Substance_PF_MASK_RAWChannels|Substance_PF_MASK_RAWPrecision,
	
	/* Bc/DXT formats */
	Substance_PF_BC1 = Substance_PF_BC | 0x0,  /**< BC1/DXT1 format */
	Substance_PF_BC2 = Substance_PF_BC | 0x8,  /**< BC2/DXT2/DXT3 format */
	Substance_PF_BC3 = Substance_PF_BC | 0x10, /**< BC3/DXT4/DXT5 format */
	Substance_PF_BC4 = Substance_PF_BC | 0x18, /**< BC4/RGTC1/ATI1 format */
	Substance_PF_BC5 = Substance_PF_BC | 0x14, /**< BC5/RGTC2/ATI2 format */
	
	/* PVRTC/ETC formats (3bits) */
	Substance_PF_PVRTC2 = Substance_PF_PVRTC | 0x0,  /**< PVRTC 2bpp */
	Substance_PF_PVRTC4 = Substance_PF_PVRTC | 0x4,  /**< PVRTC 4bpp */
	Substance_PF_ETC1   = Substance_PF_ETC   | 0x8,  /**< ETC1 RGB8 */
	
	/* Miscellaneous compressed formats (3bits) */
	Substance_PF_JPEG = Substance_PF_Misc | 0x0, /**< JPEG format (input only) */
	
	/* Other flags: can be combined to any format */
	Substance_PF_sRGB = 0x20,                 /**< sRGB (gamma trans.) flag */

	/* Deprecated flags (syntax changed) */
	Substance_PF_DXT  = Substance_PF_BC,
	Substance_PF_16b  = Substance_PF_16I,
	Substance_PF_DXT1 = Substance_PF_BC1,
	Substance_PF_DXT2 = Substance_PF_DXT | 0x4,
	Substance_PF_DXT3 = Substance_PF_BC2,
	Substance_PF_DXT4 = Substance_PF_DXT | 0xC,
	Substance_PF_DXT5 = Substance_PF_BC3,
	Substance_PF_DXTn = Substance_PF_BC5,
} SubstancePixelFormat;

typedef struct SubstanceTexture_
{
	void *buffer;
	unsigned short level0Width;
	unsigned short level0Height;
	unsigned char pixelFormat;
	unsigned char channelsOrder;
	unsigned char mipmapCount;
} SubstanceTexture;

typedef struct SubstanceTextureInput_
{
	SubstanceTexture mTexture;
	unsigned int level0Width;
	unsigned int level0Height;
	unsigned char pixelFormat;
	unsigned char mipmapCount;
} SubstanceTextureInput;

typedef struct SubstanceHardResources_
{
	unsigned char gpusUse[SUBSTANCE_GPU_COUNT_MAX];
	unsigned char cpusUse[SUBSTANCE_CPU_COUNT_MAX];
	unsigned char spusUse[SUBSTANCE_SPU_COUNT_MAX];
	size_t systemMemoryBudget;
	size_t videoMemoryBudget[SUBSTANCE_GPU_COUNT_MAX];
	
} SubstanceHardResources;

typedef struct SubstanceDataDesc_
{
	unsigned int outputsCount;
	unsigned int inputsCount;
	unsigned short formatVersion;
	unsigned char platformId;

} SubstanceDataDesc;

typedef enum
{
	Substance_Sync_Synchronous  = 0x0,
	Substance_Sync_Asynchronous = 0x1,

} SubstanceSyncOption;

typedef struct SubstanceInputDesc_
{
	unsigned int inputId;
	SubstanceInputType inputType;
	union
	{
		const float *typeFloatX;
		const int *typeIntegerX;
		const char **typeString;

	} value;

} SubstanceInputDesc;

typedef enum
{
	Substance_OState_NotComputed,         /**< Output not ready */
	Substance_OState_Ready,               /**< Output computed and ready */
	Substance_OState_Grabbed              /**< Output already grabbed by the user */

} SubstanceOutputState;

typedef struct SubstanceOutputDesc_
{
	unsigned int outputId;
	unsigned int level0Width;
	unsigned int level0Height;
	unsigned char pixelFormat;
	unsigned char mipmapCount;
	SubstanceOutputState state;
} SubstanceOutputDesc;

typedef enum
{
	Substance_State_Started = 0x1,
	Substance_State_Asynchronous = 0x2,
	Substance_State_EndingWaiting = 0x4,
	Substance_State_NoMoreRender = 0x8,
	Substance_State_NoMoreRsc = 0x10,
	Substance_State_PendingCommand = 0x20,

} SubstanceStateFlag;

typedef enum
{
	Substance_Linker_Select_Unselect = 0x0,
	Substance_Linker_Select_Select = 0x1,
	Substance_Linker_Select_UnselectAll = 0x2
} SubstanceLinkerSelect;

#endif

typedef enum
{
	Substance_Linker_Context_Init_Auto,
	Substance_Linker_Context_Init_Mobile,
	Substance_Linker_Context_Init_NonMobile,
} SubstanceLinkerContextInitEnum;

typedef void (*SubstanceInputOutputCallback)(void* userdata, unsigned int* inputIds, size_t inputCount, unsigned int* outputIds, size_t outputCount);

namespace Substance
{
	class IXMLNode;
	class ISubstanceAPI;

	extern ISubstanceAPI* gAPI;

	class IXMLFile
	{
	public:
		virtual ~IXMLFile() {}

		virtual const IXMLNode* GetRootNode() const = 0;
		virtual bool IsValid() const = 0;
	};

	class IXMLNode
	{
	public:
		virtual ~IXMLNode() {}

		virtual const IXMLNode* FindChildNode(const char* tag) const = 0;
		virtual const char* GetAttribute(const char* tag) const = 0;
		virtual const IXMLNode* GetFirstChildNode() const = 0;
		virtual const IXMLNode* GetNextNode() const = 0;
	};

	class ISubstanceAPI
	{
	public:
		virtual ~ISubstanceAPI() {}

		virtual unsigned int SubstanceContextInit(SubstanceContext** context, void* callbackOutputCompleted, void* callbackJobCompleted, void* callbackInputImageLock, void* callbackInputImageUnlock, void* callbackMalloc, void* callbackFree) = 0;
		virtual unsigned int SubstanceContextMemoryFree(SubstanceContext* context, void* data) = 0;
		virtual unsigned int SubstanceContextRelease(SubstanceContext* context) = 0;
		virtual unsigned int SubstanceHandleCompute(SubstanceHandle* handle) = 0;
		virtual unsigned int SubstanceHandleFlush(SubstanceHandle* handle) = 0;
		virtual unsigned int SubstanceHandleGetDesc(SubstanceHandle* handle, SubstanceDataDesc* dataDesc) = 0;
		virtual unsigned int SubstanceHandleGetInputDesc(SubstanceHandle* handle, unsigned int inputIdx, SubstanceInputDesc* substanceInputDesc) = 0;
		virtual unsigned int SubstanceHandleGetOutputDesc(SubstanceHandle* handle, unsigned int outputIdx, SubstanceOutputDesc* substanceOutputDesc) = 0;
		virtual unsigned int SubstanceHandleGetState(SubstanceHandle* handle, unsigned int* state) = 0;
		virtual unsigned int SubstanceHandleGetTexture(SubstanceHandle* handle, unsigned int outputIndex, SubstanceTexture* textureOut, SubstanceContext** contextOut) = 0;
		virtual unsigned int SubstanceHandleInit(SubstanceHandle** handle, SubstanceContext* context, unsigned char* data, size_t dataSize, SubstanceHardResources& hardrsc, size_t userData) = 0;
		virtual unsigned int SubstanceHandleInputOutputCallback(SubstanceHandle* handle, SubstanceInputOutputCallback callback, void* userdata) = 0;
		virtual unsigned int SubstanceHandlePushOutputs(SubstanceHandle* handle, const unsigned int* outputs, unsigned int outputCount, size_t userData) = 0;
		virtual unsigned int SubstanceHandlePushOutputsHint(SubstanceHandle* handle, const unsigned int* outputs, unsigned int outputCount) = 0;
		virtual unsigned int SubstanceHandlePushSetInput(SubstanceHandle* handle, unsigned int index, SubstanceInputType type, void* value, size_t userData) = 0;
		virtual unsigned int SubstanceHandlePushSetInputHint(SubstanceHandle* handle, unsigned int index, SubstanceInputType type) = 0;
		virtual unsigned int SubstanceHandleRelease(SubstanceHandle* handle) = 0;
		virtual unsigned int SubstanceHandleStart(SubstanceHandle* handle) = 0;
		virtual unsigned int SubstanceHandleStop(SubstanceHandle* handle) = 0;
		virtual unsigned int SubstanceHandleSwitchHard(SubstanceHandle* handle, SubstanceSyncOption syncMode, const SubstanceHardResources* substanceRscNew) = 0;
		virtual unsigned int SubstanceHandleTransferCache(SubstanceHandle* from, SubstanceHandle* to, const unsigned char* data) = 0;
		virtual unsigned int SubstanceLinkerConnectOutputToInput(SubstanceLinkerHandle* handle, unsigned int input, unsigned int output) = 0;
		virtual unsigned int SubstanceLinkerContextInit(SubstanceLinkerContext** context, SubstanceLinkerContextInitEnum initEnum) = 0;
		virtual unsigned int SubstanceLinkerContextSetXMLCallback(SubstanceLinkerContext* context, void* callbackPtr) = 0;
		virtual unsigned int SubstanceLinkerContextSetUIDCollisionCallback(SubstanceLinkerContext* context, void* callbackPtr) = 0;
		virtual unsigned int SubstanceLinkerEnableOutputs(SubstanceLinkerHandle* handle, unsigned int* enabledIds, size_t enabledCount) = 0;
		virtual unsigned int SubstanceLinkerGetCacheMapping(SubstanceLinkerHandle* handle, const unsigned char** data, const unsigned char* prevData) = 0;
		virtual unsigned int SubstanceLinkerGetUserData(SubstanceLinkerHandle* handle, size_t* userdata) = 0;
		virtual unsigned int SubstanceLinkerFuseInputs(SubstanceLinkerHandle* handle, unsigned int input0, unsigned int input1) = 0;
		virtual unsigned int SubstanceLinkerHandleInit(SubstanceLinkerHandle** handle, SubstanceLinkerContext* context, size_t userData) = 0;
		virtual unsigned int SubstanceLinkerLink(SubstanceLinkerHandle* handle, const unsigned char** data, size_t* dataSize) = 0;
		virtual unsigned int SubstanceLinkerPushAssembly(SubstanceLinkerHandle* handle, unsigned int uid, const char* data, size_t dataSize) = 0;
		virtual unsigned int SubstanceLinkerPushMemory(SubstanceLinkerHandle* handle, const char* data, size_t dataSize) = 0;
		virtual unsigned int SubstanceLinkerRelease(SubstanceLinkerHandle* handle, SubstanceLinkerContext* context) = 0;
		virtual unsigned int SubstanceLinkerSelectOutputs(SubstanceLinkerHandle* handle, SubstanceLinkerSelect flag, unsigned int idxOut) = 0;
		virtual unsigned int SubstanceLinkerSetOutputFormat(SubstanceLinkerHandle* handle, unsigned int uid, unsigned int format, int mips) = 0;
		virtual unsigned int SubstanceLinkerSetUserData(SubstanceLinkerHandle* handle, size_t userData) = 0;
		virtual void* SubstanceXMLFileCreate(const char* buffer) = 0;
		virtual void SubstanceXMLFileDestroy(void* pFile) = 0;
		
		virtual unsigned int SubstanceMaxTextureSize() const = 0;
		virtual const char* SubstanceAPIName() const = 0;
	};

	class AutoXMLFile
	{
	public:
		AutoXMLFile(const char* buffer)
			: mXMLFile(static_cast<IXMLFile*>(gAPI->SubstanceXMLFileCreate(buffer)))
		{
		}
		~AutoXMLFile()
		{
			gAPI->SubstanceXMLFileDestroy(mXMLFile);
		}

		inline operator IXMLFile* () const
		{
			return mXMLFile;
		}

		inline const IXMLNode* GetRootNode() const
		{
			return mXMLFile->GetRootNode();
		}

		inline bool IsValid() const
		{
			return mXMLFile->IsValid();
		}
	private:
		IXMLFile* mXMLFile;
	};
}

SUBSTANCELIB_API unsigned int GetSubstanceAPI(Substance::ISubstanceAPI** pAPI);

typedef unsigned int (*pfnGetSubstanceAPI)(Substance::ISubstanceAPI** pAPI);
