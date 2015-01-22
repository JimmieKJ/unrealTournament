// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//
// Experimental Portable Interface to AMD's GPUPerfAPI.
// This is Epic code in the 3rd party directory
// because it is used to interface with 3rd party API
// in a portable way (Linux/Windows)
// without actually including 3rd party headers or library.
//

#ifndef GPA_WINDOWS
	#define GPA_WINDOWS 0
#endif
#ifndef GPA_LINUX
	#define GPA_LINUX 0
#endif
#ifndef GPA_DEBUG
	#define GPA_DEBUG 0
#endif

#ifdef GPA_WINDOWS
	#define GpaSnprintfs _snprintf_s
#else
	#define GpaSnprintfs snprintfs
#endif

// Manual version of GPUPerfAPI.h and GPUPerfAPIFunctionTypes.h
#define GPA_STATUS_OK 0
typedef uint32 (*GPA_InitializeF)(void);
typedef uint32 (*GPA_OpenContextF)(void*);
typedef uint32 (*GPA_GetCounterIndexF)(const char*, uint32*);
typedef uint32 (*GPA_EnableCounterF)(uint32);
typedef uint32 (*GPA_BeginSessionF)(uint32*);
typedef uint32 (*GPA_GetPassCountF)(uint32*);
typedef uint32 (*GPA_BeginPassF)(void);
typedef uint32 (*GPA_BeginSampleF)(uint32);
typedef uint32 (*GPA_EndSampleF)(void);
typedef uint32 (*GPA_EndPassF)(void);
typedef uint32 (*GPA_EndSessionF)(void);
typedef uint32 (*GPA_DisableAllCountersF)(void);
typedef uint32 (*GPA_IsSampleReadyF)(bool*, uint32, uint32);
typedef uint32 (*GPA_GetSampleFloat64F)(uint32, uint32, uint32, double*);
typedef uint32 (*GPA_GetNumCountersF)(uint32*);
typedef uint32 (*GPA_GetCounterDescriptionF)(uint32, uint32*);
typedef uint32 (*GPA_GetCounterDataTypeF)(uint32, uint32*);
typedef uint32 (*GPA_GetCounterNameF)(uint32, const char**);

// Unable to add "VSVerticesIn", possible bug in GPUPerfAPI?
enum 
{
	GPA_PS_GPUTime,
	GPA_PS_VSBusy,
	GPA_PS_PSBusy,
	GPA_PS_TexUnitBusy,
	GPA_PS_PSExportStalls,
	GPA_PS_CBMemRead,
	GPA_PS_CBMemWritten,
	GPA_PS_PreZSamplesPassing,
	GPA_PS_PostZSamplesPassing,
	GPA_PS_PostZSamplesFailingS,
	GPA_PS_PostZSamplesFailingZ,
	GPA_PS_TOTAL 
};

static const char* GpaPsTab[] =
{
	"GPUTime",
	"VSBusy",
	"PSBusy",
	"TexUnitBusy",
	"PSExportStalls",
	"CBMemRead",
	"CBMemWritten",
	"PreZSamplesPassing",
	"PostZSamplesPassing",
	"PostZSamplesFailingS",
	"PostZSamplesFailingZ"
};

// Unable to use "CSFetchSize" and "CSWriteSize", possible bug in GPUPerfAPI.
enum
{
	GPA_CS_GPUTime,
	GPA_CS_CSBusy,
	GPA_CS_CSThreads,
	GPA_CS_CSVALUInsts,
	GPA_CS_CSVALUUtilization, 
	GPA_CS_CSVALUBusy,
	GPA_CS_CSSALUBusy,
	GPA_CS_CSMemUnitBusy,
	GPA_CS_CSMemUnitStalled,
	GPA_CS_CSWriteUnitStalled,
	GPA_CS_CSALUStalledByLDS,
	GPA_CS_CSLDSBankConflict,
	GPA_CS_CSCacheHit,
	GPA_CS_CSSALUInsts,
	GPA_CS_CSVFetchInsts,
	GPA_CS_CSSFetchInsts,
	GPA_CS_CSVWriteInsts,
	GPA_CS_CSGDSInsts,
	GPA_CS_CSLDSInsts,
	GPA_CS_TOTAL
};

static const char* GpaCsTab[] =
{
	"GPUTime",
	"CSBusy",
	"CSThreads",
	"CSVALUInsts",
	"CSVALUUtilization", 
	"CSVALUBusy",
	"CSSALUBusy",
	"CSMemUnitBusy",
	"CSMemUnitStalled",
	"CSWriteUnitStalled",
	"CSALUStalledByLDS",
	"CSLDSBankConflict",
	"CSCacheHit",
	"CSSALUInsts",
	"CSVFetchInsts",
	"CSSFetchInsts",
	"CSVWriteInsts",
	"CSGDSInsts",
	"CSLDSInsts"
};

typedef struct {
	// Library entry points grouped by usage.
	GPA_InitializeF GPA_Initialize;
	GPA_OpenContextF GPA_OpenContext;
	GPA_GetCounterIndexF GPA_GetCounterIndex;
	GPA_EnableCounterF GPA_EnableCounter;
	GPA_BeginSessionF GPA_BeginSession;
	GPA_GetPassCountF GPA_GetPassCount;
	GPA_BeginPassF GPA_BeginPass;
	GPA_BeginSampleF GPA_BeginSample;
	GPA_EndSampleF GPA_EndSample;
	GPA_EndPassF GPA_EndPass;
	GPA_EndSessionF GPA_EndSession;
	GPA_DisableAllCountersF GPA_DisableAllCounters;
	GPA_IsSampleReadyF GPA_IsSampleReady;
	GPA_GetSampleFloat64F GPA_GetSampleFloat64;
	#if GPA_DEBUG
		GPA_GetNumCountersF GPA_GetNumCounters;
		GPA_GetCounterDescriptionF GPA_GetCounterDescription;
		GPA_GetCounterDataTypeF GPA_GetCounterDataType;
		GPA_GetCounterNameF GPA_GetCounterName;
	#endif
	#if GPA_WINDOWS
		HMODULE Lib;
	#endif
	#if GPA_LINUX
		// TODO
	#endif
	#define GPA_OK__TRY 0
	#define GPA_OK__WORKING 1
	#define GPA_OK__NOT_SUPPORTED 2
	uint32 Ok;
	uint32 Hash;
	uint32 Pass;
	uint32 Passes;
	uint32 InSession;
	uint32 Session;
	uint32 CounterPS[GPA_PS_TOTAL];
	uint32 CounterCS[GPA_CS_TOTAL];
} GpaT;

static GpaT Gpa;

// Returns pointer to function in loaded library.
// Sets ok=0 on fail.
static void* GpaGet(GpaT* RESTRICT T, const char* Name)
{
	void* Ptr;
	#if GPA_WINDOWS
		Ptr = GetProcAddress(T->Lib, Name);
	#endif
	#if GPA_LINUX
		Ptr = 0; // TODO
	#endif
	if(Ptr == 0)
	{
		T->Ok = GPA_OK__NOT_SUPPORTED;
	}
	return Ptr;
}
	
// Opens the interface.
// This does not require linking to the AMD library.
// Instead this function attempts to load the library manually.
static void GpaOpen(GpaT* RESTRICT T, void* Context)
{
	T->Ok = GPA_OK__WORKING;
	// Load the library.
	#if GPA_WINDOWS
		// Not implementing anything which FreeLibrary()'s here.
		// Reasoning is that multiple LoadLibrary()'s simply increments ref count.
		// Library automatically unloaded at EXE termination anyway.
		if(sizeof(void*) == 8)
		{
			T->Lib = LoadLibraryA("GPUPerfAPIDX11-x64.dll");
		}
		else
		{
			T->Lib = LoadLibraryA("GPUPerfAPIDX11.dll");
		}
		if(T->Lib == 0)
		{
			T->Ok = GPA_OK__NOT_SUPPORTED;
			return;
		}
	#endif
	#if GPA_LINUX
		// TODO
		T->Ok = GPA_OK__NOT_SUPPORTED;
		return;
	#endif
	// Fetch all the entry points.
	T->GPA_Initialize = (GPA_InitializeF) GpaGet(T, "GPA_Initialize");
	T->GPA_OpenContext = (GPA_OpenContextF) GpaGet(T, "GPA_OpenContext");
	T->GPA_GetCounterIndex = (GPA_GetCounterIndexF) GpaGet(T, "GPA_GetCounterIndex");
	T->GPA_EnableCounter = (GPA_EnableCounterF) GpaGet(T, "GPA_EnableCounter");	
	T->GPA_BeginSession = (GPA_BeginSessionF) GpaGet(T, "GPA_BeginSession");
	T->GPA_GetPassCount = (GPA_GetPassCountF) GpaGet(T, "GPA_GetPassCount");
	T->GPA_BeginPass = (GPA_BeginPassF) GpaGet(T, "GPA_BeginPass");
	T->GPA_BeginSample = (GPA_BeginSampleF) GpaGet(T, "GPA_BeginSample");
	T->GPA_EndSample = (GPA_EndSampleF) GpaGet(T, "GPA_EndSample");
	T->GPA_EndPass = (GPA_EndPassF) GpaGet(T, "GPA_EndPass");
	T->GPA_EndSession = (GPA_EndSessionF) GpaGet(T, "GPA_EndSession");
	T->GPA_DisableAllCounters = (GPA_DisableAllCountersF) GpaGet(T, "GPA_DisableAllCounters");
	T->GPA_IsSampleReady = (GPA_IsSampleReadyF) GpaGet(T, "GPA_IsSampleReady");
	T->GPA_GetSampleFloat64 = (GPA_GetSampleFloat64F) GpaGet(T, "GPA_GetSampleFloat64");
	#if GPA_DEBUG
		T->GPA_GetNumCounters = (GPA_GetNumCountersF) GpaGet(T, "GPA_GetNumCounters");
		T->GPA_GetCounterDescription = (GPA_GetCounterDescriptionF) GpaGet(T, "GPA_GetCounterDescription");
		T->GPA_GetCounterDataType = (GPA_GetCounterDataTypeF) GpaGet(T, "GPA_GetCounterDataType");
		T->GPA_GetCounterName = (GPA_GetCounterNameF) GpaGet(T, "GPA_GetCounterName");
	#endif
	// Check for entry point fail.
	if(T->Ok == GPA_OK__NOT_SUPPORTED)
	{
		return;
	}
	// Open the library.
	uint32 Status;
	Status = T->GPA_Initialize();
	if(Status != GPA_STATUS_OK)
	{
		T->Ok = GPA_OK__NOT_SUPPORTED;
		return;
	}
	Status = T->GPA_OpenContext(Context);
	if(Status != GPA_STATUS_OK)
	{
		T->Ok = GPA_OK__NOT_SUPPORTED;
		return;
	}
	// Debug print out all counters.
	#if GPA_DEBUG && GPA_WINDOWS
		uint32 I;
		uint32 Count;
		if(T->GPA_GetNumCounters(&Count) != GPA_STATUS_OK)
		{
			return;
		}
		for(I=0;I<Count;I++)
		{
			char* Name = 0;
			T->GPA_GetCounterName(I,(const char**)&Name);
			OutputDebugStringA(Name);
			OutputDebugStringA(" - ");
			uint32 Val;
			char Data[4] = ".";
			T->GPA_GetCounterDataType(I,(GPA_Type*)&Val);
			Data[0]='0'+Val;
			OutputDebugStringA(Data);
			OutputDebugStringA(" - ");
			T->GPA_GetCounterDescription(I,(const char**)&Name);
			OutputDebugStringA(Name);
			OutputDebugStringA("\n");
		}
	#endif
}

// Use only once/frame in the program.
// Place before a draw or set of draws to profile.
// Hash should be some kind of unique id for the set of draw(s) being profiled.
// The interface uses this to correctly manage the case of changing which draws get profiled.
// The "Context" is the D3DDevice or the GL context pointer.
// Returns if String256Bytes needs to be printed.
static bool GpaBegin(char* String256Bytes, uint32 Hash, bool bCompute, void* Context)
{
	bool bPrint = false;
	GpaT* RESTRICT T = (GpaT* RESTRICT)(&Gpa);
	// If interface is not there then exit.
	if(T->Ok == GPA_OK__NOT_SUPPORTED)
	{
		return false;
	}
	// Make sure interface is up and running.
	if(T->Ok == GPA_OK__TRY)
	{
		GpaOpen(T, Context);
		if(T->Ok != GPA_OK__WORKING)
		{
			return false;
		}
		// Setup the counter table.
		uint32 I;
		for(I=0;I<GPA_PS_TOTAL;I++)
		{
			T->GPA_GetCounterIndex(GpaPsTab[I], T->CounterPS+I);
		}
		for(I=0;I<GPA_CS_TOTAL;I++)
		{
			T->GPA_GetCounterIndex(GpaCsTab[I], T->CounterCS+I);
		}
		T->Pass = 0;
		T->InSession = 0;
	}
	// Check for a new session.
	if(T->Hash != Hash) 
	{
		T->Hash = Hash;
		T->Pass = 0;
		if(T->InSession)
		{
			T->GPA_EndSession();
			T->InSession = 0;
		}
	}
	// Check for results.
	if(T->Pass && (T->Pass == T->Passes))
	{
		bool bReady = false;
		T->GPA_IsSampleReady(&bReady, T->Session, 0);
		if(bReady)
		{
			T->Pass = 0;
			if(bCompute)
			{
				double Result[GPA_CS_TOTAL];
				uint32 I;
				for(I=0;I<GPA_CS_TOTAL;I++)
				{
					T->GPA_GetSampleFloat64(T->Session, 0, T->CounterCS[I], Result+I);
				}
				double Ms = Result[GPA_CS_GPUTime] * Result[GPA_CS_CSBusy] * (1.0/100.0);
				GpaSnprintfs(String256Bytes, 256, 255, "%6.3f ms [%7.4f ns/inv][%3d %%occ][%3dv %3dm %3ds %%util][%3d mem, %3d write, %3d lds, %3d bank %%stall][%3d%% hit][%5dv %2dvr %2dvw %2ds %2dsm %2dlds %2dgds op/inv]\n",
					(float)Ms,
					(float)((Ms*1000000.0)/Result[GPA_CS_CSThreads]),
					(int)Result[GPA_CS_CSVALUUtilization],
					(int)Result[GPA_CS_CSVALUBusy],
					(int)Result[GPA_CS_CSMemUnitBusy],
					(int)Result[GPA_CS_CSSALUBusy],
					(int)Result[GPA_CS_CSMemUnitStalled],
					(int)Result[GPA_CS_CSWriteUnitStalled],
					(int)Result[GPA_CS_CSALUStalledByLDS],
					(int)Result[GPA_CS_CSLDSBankConflict],
					(int)Result[GPA_CS_CSCacheHit],
					(int)Result[GPA_CS_CSVALUInsts],
					(int)Result[GPA_CS_CSVFetchInsts],
					(int)Result[GPA_CS_CSVWriteInsts],
					(int)Result[GPA_CS_CSSALUInsts],
					(int)Result[GPA_CS_CSSFetchInsts],
					(int)Result[GPA_CS_CSLDSInsts],
					(int)Result[GPA_CS_CSGDSInsts]);
				bPrint = true;
			}
			else
			{
				double Result[GPA_PS_TOTAL];
				uint32 I;
				for(I=0;I<GPA_PS_TOTAL;I++)
				{
					T->GPA_GetSampleFloat64(T->Session, 0, T->CounterPS[I], Result+I);
				}
				double Pix = Result[GPA_PS_PreZSamplesPassing]+Result[GPA_PS_PostZSamplesPassing]+Result[GPA_PS_PostZSamplesFailingS]+Result[GPA_PS_PostZSamplesFailingZ];
				GpaSnprintfs(String256Bytes, 256, 255, "%7.4f ms [%7.2f vs %7.2f ps %7.2f idle ps/pix] %7.3f %%tex [%5.1f %%wait, %2dr %2dw B/pix rop]\n",
					(float)Result[GPA_PS_GPUTime],
					(float)((Result[GPA_PS_GPUTime]*Result[GPA_PS_VSBusy]*1000000.0)/Pix),
					(float)((Result[GPA_PS_GPUTime]*Result[GPA_PS_PSBusy]*1000000.0)/Pix),
					(float)((Result[GPA_PS_GPUTime]*(100.0 - (Result[GPA_PS_VSBusy] + Result[GPA_PS_PSBusy]))*1000.0)/Pix),
					(float)Result[GPA_PS_TexUnitBusy],
					(float)Result[GPA_PS_PSExportStalls],
					(int)(Result[GPA_PS_CBMemRead]/Pix),
					(int)(Result[GPA_PS_CBMemWritten]/Pix));
				bPrint = true;
			}
		}
	}
	// Check if starting again.
	if(T->Pass == 0)
	{
		T->GPA_DisableAllCounters();
		uint32 I;
		if(bCompute)
		{
			for(I=0;I<GPA_CS_TOTAL;I++) 
			{
				T->GPA_EnableCounter(T->CounterCS[I]);
			}
		}
		else
		{
			for(I=0;I<GPA_PS_TOTAL;I++) 
			{
				T->GPA_EnableCounter(T->CounterPS[I]);
			}
		}
		T->GPA_BeginSession(&(T->Session));
		T->InSession = 1;
		T->GPA_GetPassCount(&(T->Passes));
	}
	// Start sampling.
	if(T->Pass < T->Passes)
	{
		T->GPA_BeginPass();
		T->GPA_BeginSample(0);
	}
	return bPrint;
}
	
// Use only once/frame in the program.
// Place after a draw or set of draws to profile.
static void GpaEnd(uint32 Hash, bool bCompute)
{
	GpaT* RESTRICT T = (GpaT* RESTRICT)(&Gpa);
	// If not open correctly then exit.
	if(T->Ok != GPA_OK__WORKING)
	{
		return;
	}
	// Make sure in the same session.
	if(T->Hash != Hash)
	{
		if(T->InSession)
		{
			T->GPA_EndSession();
			T->InSession = 0;
		}
		T->Hash = Hash;
		T->Pass = 0;
		return;
	}
	// End sampling.
	if(T->Pass < T->Passes)
	{
		// End sampling.
		T->GPA_EndSample();
		T->GPA_EndPass();
		T->Pass++;
		if(T->Pass == T->Passes)
		{
			T->GPA_EndSession();
		}
		return;
	}
}
	


#if 0

// Counter listing,

GPUTime - 1 - #Timing#Time this API call took to execute on the GPU in milliseconds. Does not include time that draw calls are processed in parallel.
GPUBusy - 1 - #Timing#The percentage of time GPU was busy.
TessellatorBusy - 1 - #Timing#The percentage of time the tessellation engine is busy.
VSBusy - 1 - #Timing#The percentage of time the ShaderUnit has vertex shader work to do.
HSBusy - 1 - #Timing#The percentage of time the ShaderUnit has hull shader work to do.
DSBusy - 1 - #Timing#The percentage of time the ShaderUnit has domain shader work to do.
GSBusy - 1 - #Timing#The percentage of time the ShaderUnit has geometry shader work to do.
PSBusy - 1 - #Timing#The percentage of time the ShaderUnit has pixel shader work to do.
CSBusy - 1 - #Timing#The percentage of time the ShaderUnit has compute shader work to do.
VSVerticesIn - 1 - #VertexShader#The number of vertices processed by the VS.
HSPatches - 1 - #HullShader#The number of patches processed by the HS.
DSVerticesIn - 1 - #DomainShader#The number of vertices processed by the DS.
GSPrimsIn - 1 - #GeometryShader#The number of primitives passed into the GS.
GSVerticesOut - 1 - #GeometryShader#The number of vertices output by the GS.
PrimitiveAssemblyBusy - 1 - #Timing#The percentage of GPUTime that primitive assembly (clipping and culling) is busy. High values may be caused by having many small primitives; mid to low values may indicate pixel shader or output buffer bottleneck.
PrimitivesIn - 1 - #PrimitiveAssembly#The number of primitives received by the hardware. This includes primitives generated by tessellation.
CulledPrims - 1 - #PrimitiveAssembly#The number of culled primitives. Typical reasons include scissor, the primitive having zero area, and back or front face culling.
ClippedPrims - 1 - #PrimitiveAssembly#The number of primitives that required one or more clipping operations due to intersecting the view volume or user clip planes.
PAStalledOnRasterizer - 1 - #PrimitiveAssembly#Percentage of GPUTime that primitive assembly waits for rasterization to be ready to accept data. This roughly indicates for what percentage of time the pipeline is bottlenecked by pixel operations.
PSPixelsOut - 1 - #PixelShader#Pixels exported from shader to colour buffers. Does not include killed or alpha tested pixels; if there are multiple rendertargets, each rendertarget receives one export, so this will be 2 for 1 pixel written to two RTs.
PSExportStalls - 1 - #PixelShader#Pixel shader output stalls. Percentage of GPUBusy. Should be zero for PS or further upstream limited cases; if not zero, indicates a bottleneck in late Z testing or in the colour buffer.
CSThreadGroups - 1 - #ComputeShader#Total number of thread groups.
CSWavefronts - 1 - #ComputeShader#The total number of wavefronts used for the CS.
CSThreads - 1 - #ComputeShader#The number of CS threads processed by the hardware.
CSVALUInsts - 1 - #ComputeShader#The average number of vector ALU instructions executed per work-item (affected by flow control).
CSVALUUtilization - 1 - #ComputeShader#The percentage of active vector ALU threads in a wave. A lower number can mean either more thread divergence in a wave or that the work-group size is not a multiple of 64. Value range: 0% (bad), 100% (ideal - no thread divergence).
CSSALUInsts - 1 - #ComputeShader#The average number of scalar ALU instructions executed per work-item (affected by flow control).
CSVFetchInsts - 1 - #ComputeShader#The average number of vector fetch instructions from the video memory executed per work-item (affected by flow control).
CSSFetchInsts - 1 - #ComputeShader#The average number of scalar fetch instructions from the video memory executed per work-item (affected by flow control).
CSVWriteInsts - 1 - #ComputeShader#The average number of vector write instructions to the video memory executed per work-item (affected by flow control).
CSVALUBusy - 1 - #ComputeShader#The percentage of GPUTime vector ALU instructions are processed. Value range: 0% (bad) to 100% (optimal).
CSSALUBusy - 1 - #ComputeShader#The percentage of GPUTime scalar ALU instructions are processed. Value range: 0% (bad) to 100% (optimal).
CSMemUnitBusy - 1 - #ComputeShader#The percentage of GPUTime the memory unit is active. The result includes the stall time (MemUnitStalled). This is measured with all extra fetches and writes and any cache or memory effects taken into account. Value range: 0% to 100% (fetch-bound).
CSMemUnitStalled - 1 - #ComputeShader#The percentage of GPUTime the memory unit is stalled. Try reduce the number or size of fetches and writes if possible. Value range: 0% (optimal) to 100% (bad).
CSFetchSize - 1 - #ComputeShader#The total kilobytes fetched from the video memory. This is measured with all extra fetches and any cache or memory effects taken into account.
CSWriteSize - 1 - #ComputeShader#The total kilobytes written to the video memory. This is measured with all extra fetches and any cache or memory effects taken into account.
CSCacheHit - 1 - #ComputeShader#The percentage of fetch, write, atomic, and other instructions that hit the data cache. Value range: 0% (no hit) to 100% (optimal).
CSWriteUnitStalled - 1 - #ComputeShader#The percentage of GPUTime the Write unit is stalled. Value range: 0% to 100% (bad).
CSGDSInsts - 1 - #ComputeShader#The average number of instructions to/from the GDS executed per work-item (affected by flow control). This counter is a subset of the VALUInsts counter.
CSLDSInsts - 1 - #ComputeShader#The average number of LDS read/write instructions executed per work-item (affected by flow control).
CSALUStalledByLDS - 1 - #ComputeShader#The percentage of GPUTime ALU units are stalled by the LDS input queue being full or the output queue being not ready. If there are LDS bank conflicts, reduce them. Otherwise, try reducing the number of LDS accesses if possible. Value range: 0% (optimal) to 100% (bad).
CSLDSBankConflict - 1 - #ComputeShader#The percentage of GPUTime LDS is stalled by bank conflicts. Value range: 0% (optimal) to 100% (bad).
TexUnitBusy - 1 - #Timing#The percentage of GPUTime the texture unit is active. This is measured with all extra fetches and any cache or memory effects taken into account.
TexTriFilteringPct - 1 - #TextureUnit#Percentage of pixels that received trilinear filtering. Note that not all pixels for which trilinear filtering is enabled will receive it (e.g. if the texture is magnified).
TexVolFilteringPct - 1 - #TextureUnit#Percentage of pixels that received volume filtering.
TexAveAnisotropy - 1 - #TextureUnit#The average degree of anisotropy applied. A number between 1 and 16. The anisotropic filtering algorithm only applies samples where they are required (e.g. there will be no extra anisotropic samples if the view vector is perpendicular to the surface) so this can be much lower than the requested anisotropy.
DepthStencilTestBusy - 1 - #Timing#Percentage of time GPU spent performing depth and stencil tests relative to GPUBusy.
HiZTilesAccepted - 1 - #DepthAndStencil#Percentage of tiles accepted by HiZ and will be rendered to the depth or color buffers.
PreZTilesDetailCulled - 1 - #DepthAndStencil#Percentage of tiles rejected because the associated prim had no contributing area.
HiZQuadsCulled - 1 - #DepthAndStencil#Percentage of quads that did not have to continue on in the pipeline after HiZ. They may be written directly to the depth buffer, or culled completely. Consistently low values here may suggest that the Z-range is not being fully utilized.
PreZQuadsCulled - 1 - #DepthAndStencil#Percentage of quads rejected based on the detailZ and earlyZ tests.
PostZQuads - 1 - #DepthAndStencil#Percentage of quads for which the pixel shader will run and may be postZ tested.
PreZSamplesPassing - 1 - #DepthAndStencil#Number of samples tested for Z before shading and passed.
PreZSamplesFailingS - 1 - #DepthAndStencil#Number of samples tested for Z before shading and failed stencil test.
PreZSamplesFailingZ - 1 - #DepthAndStencil#Number of samples tested for Z before shading and failed Z test.
PostZSamplesPassing - 1 - #DepthAndStencil#Number of samples tested for Z after shading and passed.
PostZSamplesFailingS - 1 - #DepthAndStencil#Number of samples tested for Z after shading and failed stencil test.
PostZSamplesFailingZ - 1 - #DepthAndStencil#Number of samples tested for Z after shading and failed Z test.
ZUnitStalled - 1 - #DepthAndStencil#The percentage of GPUTime the depth buffer spends waiting for the color buffer to be ready to accept data. High figures here indicate a bottleneck in color buffer operations.
CBMemRead - 1 - #ColorBuffer#Number of bytes read from the color buffer.
CBMemWritten - 1 - #ColorBuffer#Number of bytes written to the color buffer.
CBSlowPixelPct - 1 - #ColorBuffer#Percentage of pixels written to the color buffer using a half-rate or quarter-rate format.

#endif