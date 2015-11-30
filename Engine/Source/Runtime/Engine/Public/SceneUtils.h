// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This file contains the various draw mesh macros that display draw calls
 * inside of PIX.
 */

// Colors that are defined for a particular mesh type
// Each event type will be displayed using the defined color
#pragma once

// Disable draw mesh events for final builds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PLATFORM_SUPPORTS_DRAW_MESH_EVENTS
#define WANTS_DRAW_MESH_EVENTS 1
#else
#define WANTS_DRAW_MESH_EVENTS 0
#endif

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
		void CDECL Start(TRHICmdList& RHICmdList, const TCHAR* Fmt, ...);
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
		void CDECL Start(IRHIComputeContext& InRHICommandContext, const TCHAR* Fmt, ...);
		void Stop();
	};

	// Macros to allow for scoping of draw events outside of RHI function implementations
	#define SCOPED_DRAW_EVENT(RHICmdList, Name) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, TEXT(#Name));
	#define SCOPED_DRAW_EVENTF(RHICmdList, Name, Format, ...) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, Name, Condition) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, TEXT(#Name));
	#define SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, Name, Condition, Format, ...) TDrawEvent<FRHICommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Format, ##__VA_ARGS__);
	#define BEGIN_DRAW_EVENTF(RHICmdList, Name, Event, Format, ...) if(GEmitDrawEvents) Event.Start(RHICmdList, Format, ##__VA_ARGS__);
	#define STOP_DRAW_EVENT(Event) Event.Stop();

	#define SCOPED_COMPUTE_EVENT(RHICmdList, Name) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, TEXT(#Name));
	#define SCOPED_COMPUTE_EVENTF(RHICmdList, Name, Format, ...) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_COMPUTE_EVENT(RHICmdList, Name, Condition) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, TEXT(#Name));
	#define SCOPED_CONDITIONAL_COMPUTE_EVENTF(RHICmdList, Name, Condition, Format, ...) TDrawEvent<FRHIAsyncComputeCommandList> PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Format, ##__VA_ARGS__);

	// Macros to allow for scoping of draw events within RHI function implementations
	#define SCOPED_RHI_DRAW_EVENT(RHICmdContext, Name) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, TEXT(#Name));
	#define SCOPED_RHI_DRAW_EVENTF(RHICmdContext, Name, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Format, ##__VA_ARGS__);
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT(RHICmdContext, Name, Condition) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, TEXT(#Name));
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(RHICmdContext, Name, Condition, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Format, ##__VA_ARGS__);

#else

	template<typename TRHICmdList>
	struct ENGINE_API TDrawEvent
	{
	};

	#define SCOPED_DRAW_EVENT(...)
	#define SCOPED_DRAW_EVENTF(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENT(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENTF(...)
	#define BEGIN_DRAW_EVENTF(...)
	#define STOP_DRAW_EVENT(...)

	#define SCOPED_COMPUTE_EVENT(RHICmdList, Name)
	#define SCOPED_COMPUTE_EVENTF(RHICmdList, Name, Format, ...)
	#define SCOPED_CONDITIONAL_COMPUTE_EVENT(RHICmdList, Name, Condition)
	#define SCOPED_CONDITIONAL_COMPUTE_EVENTF(RHICmdList, Name, Condition, Format, ...)

	#define SCOPED_RHI_DRAW_EVENT(...)
	#define SCOPED_RHI_DRAW_EVENTF(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(...)

#endif
