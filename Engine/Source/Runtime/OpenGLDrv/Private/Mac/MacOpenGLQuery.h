// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SharedPointer.h"
#include "MacOpenGLContext.h"

class FMacOpenGLQueryEmu;
typedef TMap<GLenum, TArray<GLuint>> FOpenGLQueryCache;

class FMacOpenGLTimer : public TSharedFromThis<FMacOpenGLTimer, ESPMode::Fast>
{
public:
	FMacOpenGLTimer(FPlatformOpenGLContext* InContext, FMacOpenGLQueryEmu* InEmu);
	virtual ~FMacOpenGLTimer();
	
	void Begin(void);
	void End(void);
	uint64 GetResult(void);
	uint64 GetAccumulatedResult(void);
	int32 GetResultAvailable(void);
	
public:
	TSharedPtr<FMacOpenGLTimer, ESPMode::Fast> Next;
	TSharedPtr<FMacOpenGLTimer, ESPMode::Fast> Previous;
	
private:
	void CacheResult(void);
	
private:
	FPlatformOpenGLContext* Context;
	FMacOpenGLQueryEmu* Emu;
	uint32 Name;
	uint64 Result;
	uint64 Accumulated;
	uint32 Available;
	bool Cached;
	bool Running;
};

class FMacOpenGLQuery : public TSharedFromThis<FMacOpenGLQuery, ESPMode::Fast>
{
public:
	FMacOpenGLQuery(FPlatformOpenGLContext* InContext, FMacOpenGLQueryEmu* InEmu);
	virtual ~FMacOpenGLQuery();
	
	void Begin(GLenum InTarget);
	void End();
	uint64 GetResult(void);
	int32 GetResultAvailable(void);
	
	uint32 Name;
	GLenum Target;
	
private:
	FPlatformOpenGLContext* Context;
	FMacOpenGLQueryEmu* Emu;
	uint64 Result;
	uint32 Available;
	bool Running;
	bool Cached;
	TSharedPtr<FMacOpenGLTimer, ESPMode::Fast> Start;
	TSharedPtr<FMacOpenGLTimer, ESPMode::Fast> Finish;
};

class FMacOpenGLQueryEmu
{
	friend class FMacOpenGLQuery;
	friend class FMacOpenGLTimer;
public:
	FMacOpenGLQueryEmu(FPlatformOpenGLContext* InContext);
	~FMacOpenGLQueryEmu();
	
	bool IsQuery(GLuint const QueryID) const;
	TSharedPtr<FMacOpenGLQuery, ESPMode::Fast> GetQuery(GLuint const QueryID) const;
	GLuint CreateQuery(void);
	void DeleteQuery(GLuint const QueryID);
	void BeginQuery(GLenum const QueryType, GLuint const QueryID);
	void EndQuery(GLenum const QueryType);

private:
	FPlatformOpenGLContext* PlatformContext;
	FOpenGLQueryCache FreeQueries;
	TArray<TSharedPtr<FMacOpenGLQuery, ESPMode::Fast>> Queries;
	TMap<GLenum, TSharedPtr<FMacOpenGLQuery, ESPMode::Fast>> RunningQueries;
	TSharedPtr<FMacOpenGLTimer, ESPMode::Fast> LastTimer;
};

extern bool GIsEmulatingTimestamp;
