// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats.h"


CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSHA, Warning, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogStats, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogStreaming, Warning, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogInit, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogExit, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogExec, Warning, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogScript, Warning, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogLocalization, Error, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogLongPackageNames, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogProcess, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogLoad, Log, All);


/** Memory stats */
DECLARE_MEMORY_STAT_EXTERN(TEXT("Audio Memory Used"),STAT_AudioMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Animation Memory"),STAT_AnimationMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Precomputed Visibility Memory"),STAT_PrecomputedVisibilityMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Precomputed Shadow Depth Map Memory"),STAT_PrecomputedShadowDepthMapMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Precomputed Light Volume Memory"),STAT_PrecomputedLightVolumeMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("StaticMesh Total Memory"),STAT_StaticMeshTotalMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("SkeletalMesh Vertex Memory"),STAT_SkeletalMeshVertexMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("SkeletalMesh Index Memory"),STAT_SkeletalMeshIndexMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("SkeletalMesh M.BlurSkinning Memory"),STAT_SkeletalMeshMotionBlurSkinningMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("VertexShader Memory"),STAT_VertexShaderMemory,STATGROUP_Memory, FPlatformMemory::MCR_Physical, CORE_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("PixelShader Memory"),STAT_PixelShaderMemory,STATGROUP_Memory, FPlatformMemory::MCR_Physical, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Navigation Memory"),STAT_NavigationMemory,STATGROUP_Memory, CORE_API);
/** PhysX memory tracking needs PHYSX_MEMORY_STATS enabled */
DECLARE_MEMORY_STAT_EXTERN(TEXT("PhysX Memory Used"),STAT_MemoryPhysXTotalAllocationSize,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("ICU Memory Used"),STAT_MemoryICUTotalAllocationSize,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("ICU Data File Memory Used"),STAT_MemoryICUDataFileAllocationSize,STATGROUP_Memory, CORE_API);

DECLARE_MEMORY_STAT_EXTERN(TEXT("StaticMesh Total Memory"),STAT_StaticMeshTotalMemory2,STATGROUP_MemoryStaticMesh, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("StaticMesh Vertex Memory"),STAT_StaticMeshVertexMemory,STATGROUP_MemoryStaticMesh, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("StaticMesh VxColor Resource Mem"),STAT_ResourceVertexColorMemory,STATGROUP_MemoryStaticMesh, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("StaticMesh VxColor Inst Mem"),STAT_InstVertexColorMemory,STATGROUP_MemoryStaticMesh, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("StaticMesh Index Memory"),STAT_StaticMeshIndexMemory,STATGROUP_MemoryStaticMesh, CORE_API);

DECLARE_MEMORY_STAT_EXTERN(TEXT("Texture Memory Used"),STAT_TextureMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Reflection Capture Texture Memory"),STAT_ReflectionCaptureTextureMemory,STATGROUP_Memory, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Reflection Capture Memory"),STAT_ReflectionCaptureMemory,STATGROUP_Memory, CORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Total Render thread idle time"),STAT_RenderingIdleTime,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Wait for GPU Query"),STAT_RenderingIdleTime_WaitingForGPUQuery,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Wait for GPU Present"),STAT_RenderingIdleTime_WaitingForGPUPresent,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Other Render Thread Sleep Time"),STAT_RenderingIdleTime_RenderThreadSleepTime,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Rendering thread busy time"),STAT_RenderingBusyTime,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Game thread idle time"),STAT_GameIdleTime,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Game thread tick wait time"),STAT_GameTickWaitTime,STATGROUP_Threading, CORE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Game thread requested wait time"),STAT_GameTickWantedWaitTime,STATGROUP_Threading, CORE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Game thread additional wait time"),STAT_GameTickAdditionalWaitTime,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Game TaskGraph Tasks"),STAT_TaskGraph_GameTasks,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Game TaskGraph Stalls"),STAT_TaskGraph_GameStalls,STATGROUP_Threading, CORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Render Local Queue Stalls"),STAT_TaskGraph_RenderStalls,STATGROUP_Threading, CORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Other TaskGraph Tasks"),STAT_TaskGraph_OtherTasks,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Other TaskGraph Stalls"),STAT_TaskGraph_OtherStalls,STATGROUP_Threading, CORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Flush Threaded Logs"),STAT_FlushThreadedLogs,STATGROUP_Threading, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Pump Messages"),STAT_PumpMessages,STATGROUP_Threading, CORE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Percentage CPU utilization"),STAT_CPUTimePct,STATGROUP_Threading, CORE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Percentage CPU utilization (relative to one core)"),STAT_CPUTimePctRelative,STATGROUP_Threading, CORE_API);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Fulfilled read count"),STAT_AsyncIO_FulfilledReadCount,STATGROUP_AsyncIO, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Fulfilled read size"),STAT_AsyncIO_FulfilledReadSize,STATGROUP_AsyncIO, CORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Canceled read count"),STAT_AsyncIO_CanceledReadCount,STATGROUP_AsyncIO, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Canceled read size"),STAT_AsyncIO_CanceledReadSize,STATGROUP_AsyncIO, CORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Outstanding read count"),STAT_AsyncIO_OutstandingReadCount,STATGROUP_AsyncIO, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Outstanding read size"),STAT_AsyncIO_OutstandingReadSize,STATGROUP_AsyncIO, CORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Platform read time"),STAT_AsyncIO_PlatformReadTime,STATGROUP_AsyncIO, CORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Uncompressor wait time"),STAT_AsyncIO_UncompressorWaitTime,STATGROUP_AsyncIO, CORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Main thread block time"),STAT_AsyncIO_MainThreadBlockTime,STATGROUP_AsyncIO, CORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Async package precache wait time"),STAT_AsyncIO_AsyncPackagePrecacheWaitTime,STATGROUP_AsyncIO, CORE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Bandwidth (MByte/ sec)"),STAT_AsyncIO_Bandwidth,STATGROUP_AsyncIO, CORE_API);

