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
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define WANTS_DRAW_MESH_EVENTS 1
#else
#define WANTS_DRAW_MESH_EVENTS 0
#endif

#if WANTS_DRAW_MESH_EVENTS && PLATFORM_SUPPORTS_DRAW_MESH_EVENTS

	/**
	 * Class that logs draw events based upon class scope. Draw events can be seen
	 * in PIX
	 */
	struct ENGINE_API FDrawEvent
	{
		/** Cmdlist to push onto. */
		class FRHICommandList* RHICmdList;

		/** Default constructor, initializing all member variables. */
		FORCEINLINE FDrawEvent()
			: RHICmdList(nullptr)
		{}

		/**
		 * Terminate the event based upon scope
		 */
		FORCEINLINE ~FDrawEvent()
		{
			if (RHICmdList)
			{
				Stop();
			}
		}

		/**
		 * Function for logging a PIX event with var args
		 */
		void CDECL Start(FRHICommandList& RHICmdList, const TCHAR* Fmt, ...);
		void Stop();
	};

	// Macros to allow for scoping of draw events
	#define SCOPED_DRAW_EVENT(RHICmdList, Name) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, TEXT(#Name));
	#define SCOPED_DRAW_EVENTF(RHICmdList, Name, Format, ...) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, Name, Condition) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, TEXT(#Name));
	#define SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, Name, Condition, Format, ...) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GEmitDrawEvents && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdList, Format, ##__VA_ARGS__);

#else

	#define SCOPED_DRAW_EVENT(...)
	#define SCOPED_DRAW_EVENTF(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENT(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENTF(...)

#endif
