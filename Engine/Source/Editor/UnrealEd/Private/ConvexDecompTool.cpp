// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ConvexDecompTool.cpp: Utility for turning graphics mesh into convex hulls.
=============================================================================*/

// Only enabling on windows until other platforms can test!
#define USE_VHACD (PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC)

#include "UnrealEd.h"
#include "PhysicsEngine/BodySetup.h"

#if USE_VHACD

#include "ThirdParty/VHACD/public/VHACD.h"

#if PLATFORM_MAC
#include <OpenCL/cl.h>
#endif

#else

#ifdef __clang__
// HACD headers use pragmas that Clang doesn't recognize (inline depth)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"	// warning : unknown pragma ignored [-Wunknown-pragmas]
#endif

#include "ThirdParty/HACD/HACD_1.0/public/HACD.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif // USE_VHACD



#include "ConvexDecompTool.h"
#include "PhysicsEngine/BodySetup.h"


DEFINE_LOG_CATEGORY_STATIC(LogConvexDecompTool, Log, All);

#if USE_VHACD
using namespace VHACD;

class FVHACDProgressCallback : public IVHACD::IUserCallback
{
public:
	FVHACDProgressCallback(void) {}
	~FVHACDProgressCallback() {};

	void Update(const double overallProgress, const double stageProgress, const double operationProgress, const char * const stage,	const char * const    operation)
	{
		FString StatusString = FString::Printf(TEXT("Processing [%s]..."), ANSI_TO_TCHAR(stage));

		GWarn->StatusUpdate(stageProgress*10.f, 1000, FText::FromString(StatusString));
		//GWarn->StatusUpdate(overallProgress*10.f, 1000, FText::FromString(StatusString));
	};
};
#else
using namespace HACD_STANDALONE;

class FHACDProgressCallback : public hacd::ICallback
{
public:
	virtual void ReportProgress(const char * Status, hacd::HaF32 Progress)
	{
		GWarn->StatusUpdate( Progress*1000.f, 1000, FText::FromString( FString( Status ) ) );
	}

	virtual bool Cancelled()
	{
		return (GWarn->ReceivedUserCancel() != false);
	}
};
#endif

void DecomposeMeshToHulls(UBodySetup* InBodySetup, const TArray<FVector>& InVertices, const TArray<uint32>& InIndices, float InAccuracy, int32 InMaxHullVerts)
{
	check(InBodySetup != NULL);

	bool bSuccess = false;

#if USE_VHACD
	FVHACDProgressCallback VHACD_Callback;

	IVHACD::Parameters VHACD_Params;
	VHACD_Params.m_resolution = 1000000;
	VHACD_Params.m_maxNumVerticesPerCH = InMaxHullVerts;
	VHACD_Params.m_concavity = 0.3f * (1.f - FMath::Clamp(InAccuracy, 0.f, 1.f));
	VHACD_Params.m_callback = &VHACD_Callback;
	VHACD_Params.m_oclAcceleration = false;

	GWarn->BeginSlowTask(NSLOCTEXT("ConvexDecompTool", "BeginCreatingCollisionTask", "Creating Collision"), true, false);

	IVHACD* InterfaceVHACD = CreateVHACD();
	
#if CL_VERSION_1_1
	cl_device_id max_gflops_device = NULL;
	int max_gflops = 0;
	
	cl_uint NumPlatforms = 0;
	cl_int Result = clGetPlatformIDs(0, NULL, &NumPlatforms);
	if(Result == CL_SUCCESS && NumPlatforms > 0)
	{
		cl_platform_id* Platforms = new cl_platform_id[NumPlatforms];
		Result = clGetPlatformIDs(NumPlatforms, Platforms, NULL);
		ANSICHAR PlatformVersion[1024];
		for(cl_uint i = 0; Result == CL_SUCCESS && i < NumPlatforms; i++)
		{
			clGetPlatformInfo(Platforms[i], CL_PLATFORM_VERSION, sizeof(PlatformVersion), PlatformVersion, nullptr);
			
			int32 Major = 0;
			int32 Minor = 0;
			sscanf(PlatformVersion, "OpenCL %d.%d", &Major, &Minor);
			
			if(Major > 1 || (Major == 1 && Minor >= 1))
			{
				cl_uint NumGPUs = 0;
				Result = clGetDeviceIDs(Platforms[i], CL_DEVICE_TYPE_GPU, 0, NULL, &NumGPUs);
				if ( Result == CL_SUCCESS )
				{
					cl_device_id* Devices = new cl_device_id[NumGPUs];
					Result = clGetDeviceIDs(Platforms[i], CL_DEVICE_TYPE_GPU, NumGPUs, Devices, NULL);
					for(cl_uint j = 0; j < NumGPUs; j++)
					{
						clGetDeviceInfo(Devices[j], CL_DEVICE_VERSION, sizeof(PlatformVersion), PlatformVersion, nullptr);
						Major = 0;
						Minor = 0;
						sscanf(PlatformVersion, "OpenCL %d.%d", &Major, &Minor);
						if(Major > 1 || (Major == 1 && Minor >= 1))
						{
							cl_uint Freq = 0;
							clGetDeviceInfo(Devices[j], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(Freq), &Freq, NULL);
							size_t Units = 0;
							clGetDeviceInfo(Devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(Units), &Units, NULL);
							int gflops = Units * Freq;
							if (gflops > max_gflops)
							{
								max_gflops = gflops;
								max_gflops_device = Devices[j];
							}
						}
					}
					delete [] Devices;
				}
			}
		}
		delete [] Platforms;
	}
	if ( max_gflops_device )
	{
		VHACD_Params.m_oclAcceleration = true;
		InterfaceVHACD->OCLInit(&max_gflops_device);
	}
#endif

	const float* const Verts = (float*)InVertices.GetData();
	const unsigned int NumVerts = InVertices.Num();
	const int* const Tris = (int*)InIndices.GetData();
	const unsigned int NumTris = InIndices.Num() / 3;

	bSuccess = InterfaceVHACD->Compute(Verts, 3, NumVerts, Tris, 3, NumTris, VHACD_Params);
#else
	// Fill in data for decomposition tool.
	HACD_API::Desc Desc;
	Desc.mVertexCount = InVertices.Num();
	Desc.mVertices = (float*)InVertices.GetData();

	Desc.mTriangleCount = InIndices.Num() / 3;
	Desc.mIndices = (uint32*)InIndices.GetData();

	Desc.mMaxMergeHullCount = FMath::Max(1, FMath::RoundToInt(InAccuracy*24.f));
	//Desc.mConcavity = 0.3f * (1.f - FMath::Clamp(InAccuracy, 0.f, 1.f));
	Desc.mMaxHullVertices = InMaxHullVerts;

	Desc.mUseFastVersion = false;
	Desc.mBackFaceDistanceFactor = 0.00000000001f;
	Desc.mDecompositionDepth = 5;

	FHACDProgressCallback Callback;
	Desc.mCallback = &Callback;

	// Perform the work
	HACD_API* HAPI = createHACD_API();

	GWarn->BeginSlowTask(NSLOCTEXT("ConvexDecompTool", "BeginCreatingCollisionTask", "Creating Collision"), true, true);

	uint32 Result = HAPI->performHACD(Desc);
	bSuccess = true;
#endif // USE_VHACD


	GWarn->EndSlowTask();

	if(bSuccess)
	{
		// Clean out old hulls
		InBodySetup->RemoveSimpleCollision();

		// Iterate over each result hull
#if USE_VHACD
		int32 NumHulls = InterfaceVHACD->GetNConvexHulls();
#else
		int32 NumHulls = HAPI->getHullCount();
#endif // USE_VHACD

		for(int32 HullIdx=0; HullIdx<NumHulls; HullIdx++)
		{
			// Create a new hull in the aggregate geometry
			FKConvexElem ConvexElem;

#if USE_VHACD
			IVHACD::ConvexHull Hull;
			InterfaceVHACD->GetConvexHull(HullIdx, Hull);
			for (uint32 VertIdx = 0; VertIdx < Hull.m_nPoints; VertIdx++)
			{
				FVector V;
				V.X = (float)(Hull.m_points[(VertIdx * 3) + 0]);
				V.Y = (float)(Hull.m_points[(VertIdx * 3) + 1]);
				V.Z = (float)(Hull.m_points[(VertIdx * 3) + 2]);

				ConvexElem.VertexData.Add(V);
			}			
#else
			// Read out each hull vertex
			const HACD_API::Hull* Hull = HAPI->getHull(HullIdx);
			for(uint32 VertIdx=0; VertIdx<Hull->mVertexCount; VertIdx++)
			{
				FVector* V = (FVector*)(Hull->mVertices + (VertIdx*3));

				ConvexElem.VertexData.Add(*V);
			}
#endif // USE_VHACD

			ConvexElem.UpdateElemBox();

			InBodySetup->AggGeom.ConvexElems.Add(ConvexElem);
		}

		InBodySetup->InvalidatePhysicsData(); // update GUID
	}
	
#if USE_VHACD
#ifdef CL_VERSION_1_1
	if (VHACD_Params.m_oclAcceleration)
	{
		bool bOK = InterfaceVHACD->OCLRelease();
		check(bOK);
	}
#endif //CL_VERSION_1_1
	
	InterfaceVHACD->Clean();
	InterfaceVHACD->Release();
#endif
}
