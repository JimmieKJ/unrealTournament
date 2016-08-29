// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This file contains the various draw mesh macros that display draw calls
 * inside of PIX.
 */

// Colors that are defined for a particular mesh type
// Each event type will be displayed using the defined color
#pragma once

// Note:  WITH_PROFILEGPU should be 0 for final builds
#define WANTS_DRAW_MESH_EVENTS (WITH_PROFILEGPU && PLATFORM_SUPPORTS_DRAW_MESH_EVENTS)

#if WANTS_DRAW_MESH_EVENTS

	/**
	 * Class that logs draw events based upon class scope. Draw events can be seen
	 * in PIX
	 */
	template<typename TRHICmdList>
	struct ENGINE_API TDrawEvent
	{
		/** Cmdlist to push onto. */
		TRHICmdList* RHICmdList;

		/** Default constructor, initializing all member variables. */
		FORCEINLINE TDrawEvent()
			: RHICmdList(nullptr)
		{}

		/**
		 * Terminate the event based upon scope
		 */
		FORCEINLINE ~TDrawEvent()
		{
			if (RHICmdList)
			{
				Stop();
			}
		}

		/**
		 * Function for logging a PIX event with var args
		 */
		void CDECL Start(TRHICmdList& RHICmdList, FColor Color, const TCHAR* Fmt, ...);
		void Stop();
	};

	struct ENGINE_API FDrawEventRHIExecute
	{
		/** Context to execute on*/
		class IRHIComputeContext* RHICommandContext;

		/** Default constructor, initializing all member variables. */
		FORCEINLINE FDrawEventRHIExecute()
			: RHICommandContext(nullptr)
		{}

		/**
		* Terminate the event based upon scope
		*/
		FORCEINLINE ~FDrawEventRHIExecute()
		{
			if (RHICommandContext)
			{
				Stop();
			}
		}

		/**
		* Function for logging a PIX event with var args
		*/
		void CDECL Start(IRHIComputeContext& InRHICommandContext, FColor Color, const TCHAR* Fmt, ...);
		void Stop();
	};

	// Macros to allow for scoping of draw events outside of RHI function implementations
	#define SCOPED_DRAW_EVENT(RHICmdList, Name) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, FColor(0), TEXT(#Name));
	#define SCOPED_DRAW_EVENT_COLOR(RHICmdList, Color, Name) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Color, TEXT(#Name));
	#define SCOPED_DRAW_EVENTF(RHICmdList, Name, Format, ...) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Format, ...) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Color, Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, Name, Condition) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, FColor(0), TEXT(#Name));
	#define SCOPED_CONDITIONAL_DRAW_EVENT_COLOR(RHICmdList, Name, Color, Condition) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Color, TEXT(#Name));
	#define SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, Name, Condition, Format, ...) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Condition, Format, ...) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Color, Format, ##__VA_ARGS__);
	#define BEGIN_DRAW_EVENTF(RHICmdList, Name, Event, Format, ...) if(GEmitDrawEvents) Event.Start(RHICmdList, FColor(0), Format, ##__VA_ARGS__);
	#define BEGIN_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Event, Format, ...) if(GEmitDrawEvents) Event.Start(RHICmdList, Color, Format, ##__VA_ARGS__);
	#define STOP_DRAW_EVENT(Event) Event.Stop();

	#define SCOPED_COMPUTE_EVENT(RHICmdList, Name) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, FColor(0), TEXT(#Name));
	#define SCOPED_COMPUTE_EVENT_COLOR(RHICmdList, Color, Name) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Color, TEXT(#Name));
	#define SCOPED_COMPUTE_EVENTF(RHICmdList, Name, Format, ...) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_COMPUTE_EVENTF_COLOR(RHICmdList, Color, Name, Format, ...) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Color, Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_COMPUTE_EVENT(RHICmdList, Name, Condition) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, FColor(0), TEXT(#Name));
	#define SCOPED_CONDITIONAL_COMPUTE_EVENT_COLOR(RHICmdList, Color, Name, Condition) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Color, TEXT(#Name));
	#define SCOPED_CONDITIONAL_COMPUTE_EVENTF(RHICmdList, Name, Condition, Format, ...) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_COMPUTE_EVENTF_COLOR(RHICmdList, Color, Name, Condition, Format, ...) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Color, Format, ##__VA_ARGS__);

	// Macros to allow for scoping of draw events within RHI function implementations
	#define SCOPED_RHI_DRAW_EVENT(RHICmdContext, Name) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, FColor(0), TEXT(#Name));
	#define SCOPED_RHI_DRAW_EVENT_COLOR(RHICmdContext, Color, Name) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Color, TEXT(#Name));
	#define SCOPED_RHI_DRAW_EVENTF(RHICmdContext, Name, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_RHI_DRAW_EVENTF_COLOR(RHICmdContext, Color, Name, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Color, Format, ##__VA_ARGS__);
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT(RHICmdContext, Name, Condition) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, FColor(0), TEXT(#Name));
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT_COLOR(RHICmdContext, Color, Name, Condition) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Color, TEXT(#Name));
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(RHICmdContext, Name, Condition, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF_COLOR(RHICmdContext, Color, Name, Condition, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Color, Format, ##__VA_ARGS__);

#else

	template<typename TRHICmdList>
	struct ENGINE_API TDrawEvent
	{
	};

	#define SCOPED_DRAW_EVENT(...)
	#define SCOPED_DRAW_EVENT_COLOR(...)
	#define SCOPED_DRAW_EVENTF(...)
	#define SCOPED_DRAW_EVENTF_COLOR(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENT(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENT_COLOR(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENTF(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENTF_COLOR(...)
	#define BEGIN_DRAW_EVENTF(...)
	#define BEGIN_DRAW_EVENTF_COLOR(...)
	#define STOP_DRAW_EVENT(...)

	#define SCOPED_COMPUTE_EVENT(RHICmdList, Name)
	#define SCOPED_COMPUTE_EVENT_COLOR(RHICmdList, Name)
	#define SCOPED_COMPUTE_EVENTF(RHICmdList, Name, Format, ...)
	#define SCOPED_COMPUTE_EVENTF_COLOR(RHICmdList, Name)
	#define SCOPED_CONDITIONAL_COMPUTE_EVENT(RHICmdList, Name, Condition)
	#define SCOPED_CONDITIONAL_COMPUTE_EVENT_COLOR(RHICmdList, Name, Condition)
	#define SCOPED_CONDITIONAL_COMPUTE_EVENTF(RHICmdList, Name, Condition, Format, ...)
	#define SCOPED_CONDITIONAL_COMPUTE_EVENTF_COLOR(RHICmdList, Name, Condition)

	#define SCOPED_RHI_DRAW_EVENT(...)
	#define SCOPED_RHI_DRAW_EVENT_COLOR(...)
	#define SCOPED_RHI_DRAW_EVENTF(...)
	#define SCOPED_RHI_DRAW_EVENTF_COLOR(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT_COLOR(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF_COLOR(...)

#endif

// GPU stats
#if STATS
#define HAS_GPU_STATS 1
#else
#define HAS_GPU_STATS 0
#endif

#if HAS_GPU_STATS
#define SCOPED_GPU_STAT(RHICmdList, Stat) FScopedGPUStatEvent PREPROCESSOR_JOIN(GPUStatEvent_##Stat,__LINE__); PREPROCESSOR_JOIN(GPUStatEvent_##Stat,__LINE__).Begin(RHICmdList, GET_STATID( Stat ) );
#define GPU_STATS_UPDATE(RHICmdList) FRealtimeGPUProfiler::Get()->Update(RHICmdList);
#else
#define SCOPED_GPU_STAT(RHICmdList, Stat) 
#define GPU_STATS_UPDATE(RHICmdList) 
#endif

#if HAS_GPU_STATS

class FRealtimeGPUProfilerEvent;
class FRealtimeGPUProfilerFrame;

/**
* FRealtimeGPUProfiler class. This manages recording and reporting all for GPU stats
*/
class FRealtimeGPUProfiler
{
	static FRealtimeGPUProfiler* Instance;
public:
	// Singleton interface
	static ENGINE_API FRealtimeGPUProfiler* Get();

	/** Per-frame update */
	ENGINE_API void Update(FRHICommandListImmediate& RHICmdList);

	/** Final cleanup */
	ENGINE_API void Release();

	/** Push/pop events */
	void PushEvent(FRHICommandListImmediate& RHICmdList, TStatId StatId);
	void PopEvent(FRHICommandListImmediate& RHICmdList);

private:
	FRealtimeGPUProfiler();
	void UpdateStats(FRHICommandListImmediate& RHICmdList);

	/** Ringbuffer of profiler frames */
	TArray<FRealtimeGPUProfilerFrame*> Frames;

	int32 WriteBufferIndex;
	int32 ReadBufferIndex;
	uint32 WriteFrameNumber;
};

/**
* Class that logs GPU Stat events for the realtime GPU profiler
*/
class FScopedGPUStatEvent
{
	/** Cmdlist to push onto. */
	FRHICommandListImmediate* RHICmdList;

	/** The stat event used to record timings */
	FRealtimeGPUProfilerEvent* RealtimeGPUProfilerEvent;

public:
	/** Default constructor, initializing all member variables. */
	FORCEINLINE FScopedGPUStatEvent()
		: RHICmdList(nullptr)
		, RealtimeGPUProfilerEvent(nullptr)
	{}

	/**
	* Terminate the event based upon scope
	*/
	FORCEINLINE ~FScopedGPUStatEvent()
	{
		if (RHICmdList)
		{
			End();
		}
	}

	/**
	* Start/Stop functions for timer stats
	*/
	ENGINE_API void Begin(FRHICommandList& InRHICmdList, TStatId StatID);
	ENGINE_API void End();
};
#endif // HAS_GPU_STATS

/** True if HDR is enabled for the mobile renderer. */
ENGINE_API bool IsMobileHDR();

/** True if the mobile renderer is emulating HDR in a 32bpp render target. */
ENGINE_API bool IsMobileHDR32bpp();

/** True if the mobile renderer is emulating HDR with mosaic. */
ENGINE_API bool IsMobileHDRMosaic();
