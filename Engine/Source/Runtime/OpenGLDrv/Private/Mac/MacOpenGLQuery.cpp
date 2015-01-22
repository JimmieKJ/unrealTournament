// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OpenGLDrvPrivate.h"

#include "MacOpenGLQuery.h"

/*------------------------------------------------------------------------------
 OpenGL static variables.
 ------------------------------------------------------------------------------*/

bool GIsEmulatingTimestamp = false; // @todo: Now crashing on Nvidia cards, but not on AMD...
static int32 GNVDelayEmulatingTimeStamp = 60;
static FAutoConsoleVariableRef CVarNVDelayEmulatingTimeStamp(
	  TEXT("r.Mac.Nvidia.DelayEmulatingTimeStamp"),
	  GNVDelayEmulatingTimeStamp,
	  TEXT("The number of frames to defer emulating GL_TIMESTAMP on Nvidia cards under Mac OS X to avoid crashes on startup, set to 0 to disable. (Default: 60)"),
	  ECVF_RenderThreadSafe
	  );

/*------------------------------------------------------------------------------
 OpenGL query emulation.
 ------------------------------------------------------------------------------*/

FMacOpenGLTimer::FMacOpenGLTimer(FPlatformOpenGLContext* InContext)
: Context(InContext)
, Name(0)
, Result(0)
, Accumulated(0)
, Available(0)
, Cached(false)
, Running(false)
{
	check(Context);
	if (!IsRHIDeviceNVIDIA() || GFrameNumberRenderThread > GNVDelayEmulatingTimeStamp)
	{
		glGenQueries(1, &Name);
	}
}

FMacOpenGLTimer::~FMacOpenGLTimer()
{
	End();
	Next.Reset();
	if (Name)
	{
		glDeleteQueries(1, &Name);
	}
}

void FMacOpenGLTimer::Begin(void)
{
	if(!Running)
	{
		if (!IsRHIDeviceNVIDIA() || GFrameNumberRenderThread > GNVDelayEmulatingTimeStamp)
		{
			glBeginQuery(GL_TIME_ELAPSED, Name);
		}
		Running = true;
	}
}

void FMacOpenGLTimer::End(void)
{
	if(Running)
	{
		Running = false;
		if (!IsRHIDeviceNVIDIA() || GFrameNumberRenderThread > GNVDelayEmulatingTimeStamp)
		{
			glEndQuery(GL_TIME_ELAPSED);
		}
	}
}

uint64 FMacOpenGLTimer::GetResult(void)
{
	check(!Running);
	CacheResult();
	return Result;
}

uint64 FMacOpenGLTimer::GetAccumulatedResult(void)
{
	check(!Running);
	CacheResult();
	return Accumulated;
}

int32 FMacOpenGLTimer::GetResultAvailable(void)
{
	check(!Running);
	if(!Available)
	{
		if (!IsRHIDeviceNVIDIA() || GFrameNumberRenderThread > GNVDelayEmulatingTimeStamp)
		{
			glGetQueryObjectuiv(Name, GL_QUERY_RESULT_AVAILABLE, &Available);
		}
		else
		{
			Available = true;
		}
	}
	return Available;
}

void FMacOpenGLTimer::CacheResult(void)
{
	check(!Running);
	if(!Cached)
	{
		if (!IsRHIDeviceNVIDIA() || GFrameNumberRenderThread > GNVDelayEmulatingTimeStamp)
		{
			glGetQueryObjectui64v(Name, GL_QUERY_RESULT, &Result);
		}
		else
		{
			Result = 0;
		}
		
		if(Previous.IsValid())
		{
			Accumulated += Result;
			Accumulated += Previous->GetAccumulatedResult();
			
			Previous->Next.Reset();
			Previous.Reset();
		}
		
		Available = true;
		Cached = true;
	}
}

FMacOpenGLQuery::FMacOpenGLQuery(FPlatformOpenGLContext* InContext)
	: Name(0)
	, Target(0)
	, Context(InContext)
	, Result(0)
	, Available(0)
	, Running(0)
	, Cached(false)
{
	check(Context);
}

FMacOpenGLQuery::~FMacOpenGLQuery()
{
	if(Running)
	{
		End();
	}
	if(Name)
	{
		glDeleteQueries(1, &Name);
	}
}

void FMacOpenGLQuery::Begin(GLenum InTarget)
{
	if(Target == 0 || Target == InTarget)
	{
		Target = InTarget;
		Result = 0;
		Running = true;
		Available = false;
		Cached = false;
		Start.Reset();
		Finish.Reset();
		switch(Target)
		{
			case GL_TIMESTAMP:
			{
				// There can be a lot of timestamps, clear emulated queries out to avoid problems.
				for( auto It : Context->EmulatedQueries->Queries )
				{
					if(It.IsValid() && It->Running == false && (It->Target == GL_TIMESTAMP || It->Target == GL_TIME_ELAPSED))
					{
						if(It->GetResultAvailable())
						{
							It->GetResult();
						}
						else
						{
							break;
						}
					}
				}
				
				check(Context->EmulatedQueries->LastTimer.IsValid());
				Context->EmulatedQueries->LastTimer->End();
				Start = Context->EmulatedQueries->LastTimer;
				
				TSharedPtr<FMacOpenGLTimer, ESPMode::Fast> Current(new FMacOpenGLTimer(Context));
				Current->Previous = Start;
				Start->Next = Current;
				Current->Begin();
				Context->EmulatedQueries->LastTimer = Current;
				
				break;
			}
			case GL_TIME_ELAPSED:
			{
				check(Context->EmulatedQueries->LastTimer.IsValid());
				Context->EmulatedQueries->LastTimer->End();
				
				Start = TSharedPtr<FMacOpenGLTimer, ESPMode::Fast>(new FMacOpenGLTimer(Context));
				Start->Previous = Context->EmulatedQueries->LastTimer;
				Context->EmulatedQueries->LastTimer->Next = Start;
				Start->Begin();
				
				Context->EmulatedQueries->LastTimer = Start;
				break;
			}
			default:
			{
				if(!Name)
				{
					glGenQueries(1, &Name);
				}
				glBeginQuery(Target, Name);
				break;
			}
		}
	}
}

void FMacOpenGLQuery::End()
{
	Running = false;
	if(Target == GL_TIME_ELAPSED)
	{
		Finish = Context->EmulatedQueries->LastTimer;
		TSharedPtr<FMacOpenGLTimer, ESPMode::Fast> Current(new FMacOpenGLTimer(Context));
		Current->Previous = Finish;
		Finish->Next = Current;
		Finish->End();
		Current->Begin();
		Context->EmulatedQueries->LastTimer = Current;
		
		if (Start == Finish)
		{
			Start = Start->Previous;
		}
	}
	else if(Target != GL_TIMESTAMP)
	{
		glEndQuery(Target);
	}
}

uint64 FMacOpenGLQuery::GetResult(void)
{
	if(!Cached)
	{
		switch(Target)
		{
			case GL_TIMESTAMP:
				Result = Start->GetAccumulatedResult();
				Start.Reset();
				Finish.Reset();
				break;
			case GL_TIME_ELAPSED:
				Result = Finish->GetAccumulatedResult() - Start->GetAccumulatedResult();
				Start.Reset();
				Finish.Reset();
				break;
			default:
				glGetQueryObjectui64v(Name, GL_QUERY_RESULT, &Result);
				break;
		}
		Available = true;
		Cached = true;
	}
	return Result;
}

int32 FMacOpenGLQuery::GetResultAvailable(void)
{
	if(!Available)
	{
		switch(Target)
		{
			case GL_TIMESTAMP:
				Available = Start->GetResultAvailable();
				break;
			case GL_TIME_ELAPSED:
				Available = Finish->GetResultAvailable();
				break;
			default:
				glGetQueryObjectuiv(Name, GL_QUERY_RESULT_AVAILABLE, &Available);
				break;
		}
	}
	return Available;
}

FMacOpenGLQueryEmu::FMacOpenGLQueryEmu(FPlatformOpenGLContext* InContext)
: PlatformContext(InContext)
{
	check(PlatformContext);
	
	// Only use the timestamp emulation in a non-shipping build - end-users shouldn't care about this profiling feature.
#if (!UE_BUILD_SHIPPING)
	// Opt-in since it crashes Nvidia cards at the moment
	GIsEmulatingTimestamp = FParse::Param(FCommandLine::Get(), TEXT("EnableMacGPUTimestamp"));
#endif
	
	if ( GIsEmulatingTimestamp )
	{
		LastTimer = TSharedPtr<FMacOpenGLTimer, ESPMode::Fast>(new FMacOpenGLTimer(InContext));
		LastTimer->Begin();
	}
}

FMacOpenGLQueryEmu::~FMacOpenGLQueryEmu()
{
	if ( GIsEmulatingTimestamp && LastTimer.IsValid() )
	{
		LastTimer->End();
	}
}
	
bool FMacOpenGLQueryEmu::IsQuery(GLuint const QueryID) const
{
	return QueryID && Queries.Num() >= QueryID && Queries[QueryID-1].IsValid();
}

TSharedPtr<FMacOpenGLQuery, ESPMode::Fast> FMacOpenGLQueryEmu::GetQuery(GLuint const QueryID) const
{
	TSharedPtr<FMacOpenGLQuery, ESPMode::Fast> Query;
	if ( IsQuery(QueryID) )
	{
		Query = Queries[QueryID-1];
	}
	return Query;
}

GLuint FMacOpenGLQueryEmu::CreateQuery(void)
{
	GLuint Result = 0;
	if ( Queries.Num() < UINT_MAX )
	{
		TSharedPtr<FMacOpenGLQuery, ESPMode::Fast> Query(new FMacOpenGLQuery(PlatformContext));
		for(int32 Index = 0; Index < Queries.Num(); ++Index)
		{
			if(!Queries[Index].IsValid())
			{
				Queries[Index] = Query;
				Result = Index + 1;
				break;
			}
		}
		
		if(!Result)
		{
			Queries.Add(Query);
			Result = Queries.Num();
		}
	}
	return Result;
}

void FMacOpenGLQueryEmu::DeleteQuery(GLuint const QueryID)
{
	if ( IsQuery(QueryID) )
	{
		Queries[QueryID - 1].Reset();
	}
}

void FMacOpenGLQueryEmu::BeginQuery(GLenum const QueryType, GLuint const QueryID)
{
	TSharedPtr<FMacOpenGLQuery, ESPMode::Fast> Query = GetQuery( QueryID );
	if ( Query.IsValid() )
	{
		Query->Begin(QueryType);
		RunningQueries.Add(QueryType, Query);
	}
}

void FMacOpenGLQueryEmu::EndQuery(GLenum const QueryType)
{
	TSharedPtr<FMacOpenGLQuery> Query = RunningQueries.FindOrAdd(QueryType);
	if ( Query.IsValid() )
	{
		Query->End();
		RunningQueries[QueryType].Reset();
	}
}
