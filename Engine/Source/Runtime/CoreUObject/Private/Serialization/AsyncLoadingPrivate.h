// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Linker.h"
#include "Async/AsyncFileHandle.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLoadingDev, Fatal, All);

#if USE_NEW_ASYNC_IO

class COREUOBJECT_API FArchiveAsync2 final: public FArchive
{
public:
	enum class ELoadPhase
	{
		WaitingForSize,
		WaitingForSummary,
		WaitingForHeader,
		WaitingForFirstExport,
		ProcessingExports,
	};

	enum
	{
		// this is used for the initial precache and should be large enough to find the actual Sum.TotalHeaderSize
		MAX_SUMMARY_SIZE = 4096
	};

	FArchiveAsync2(const TCHAR* InFileName
#if USE_EVENT_DRIVEN_ASYNC_LOAD
		, TFunction<void()>&& InSummaryReadyCallback
#endif
		);
	virtual ~FArchiveAsync2();
	virtual bool Close() override;
	virtual bool SetCompressionMap(TArray<FCompressedChunk>* CompressedChunks, ECompressionFlags CompressionFlags) override;
	virtual bool Precache(int64 PrecacheOffset, int64 PrecacheSize) override;
	virtual void Serialize(void* Data, int64 Num) override;
	FORCEINLINE virtual int64 Tell() override
	{
		return CurrentPos + (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer);
	}
	virtual int64 TotalSize() override;
	virtual void Seek(int64 InPos) override;
	virtual void FlushCache() override;

	bool Precache(int64 PrecacheOffset, int64 PrecacheSize, bool bUseTimeLimit, bool bUseFullTimeLimit, double TickStartTime, float TimeLimit);
	bool PrecacheForEvent(int64 PrecacheOffset, int64 PrecacheSize);
	void FlushPrecacheBlock();
	bool ReadyToStartReadingHeader(bool bUseTimeLimit, bool bUseFullTimeLimit, double TickStartTime, float TimeLimit);
	void StartReadingHeader();
	void EndReadingHeader();
	void FirstExportStarting();
#if USE_EVENT_DRIVEN_ASYNC_LOAD
	IAsyncReadRequest* MakeEventDrivenPrecacheRequest(int64 Offset, int64 BytesToRead, FAsyncFileCallBack* CompleteCallback);
#endif

	void LogItem(const TCHAR* Item, int64 Offset = 0, int64 Size = 0, double StartTime = 0.0);

private:
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	/**
	* Updates CurrentPos based on StartFastPathLoadBuffer and sets both StartFastPathLoadBuffer and EndFastPathLoadBuffer to null
	*/
	void DiscardInlineBufferAndUpdateCurrentPos();

	void SetPosAndUpdatePrecacheBuffer(int64 Pos);

#endif

	bool WaitRead(float TimeLimit = 0.0f);
	void CompleteRead();
	void CancelRead();
	void CompleteCancel();
	bool WaitForIntialPhases(float TimeLimit = 0.0f);
	void ReadCallback(bool bWasCancelled, IAsyncReadRequest*);
	bool PrecacheInternal(int64 PrecacheOffset, int64 PrecacheSize, bool bApplyMinReadSize = true);
	
	FORCEINLINE int64 TotalSizeOrMaxInt64IfNotReady()
	{

		return SizeRequestPtr ? MAX_int64 : (FileSize + HeaderSizeWhenReadingExportsFromSplitFile);
	}

	IAsyncReadFileHandle* Handle;
	IAsyncReadRequest* SizeRequestPtr;
#if !(SPLIT_COOKED_FILES && USE_EVENT_DRIVEN_ASYNC_LOAD)
	IAsyncReadRequest* SummaryRequestPtr;
	IAsyncReadRequest* SummaryPrecacheRequestPtr;
#endif
	IAsyncReadRequest* ReadRequestPtr;
	IAsyncReadRequest* CanceledReadRequestPtr;

	/** Buffer containing precached data.											*/
	uint8* PrecacheBuffer;
	/** Cached file size															*/
	int64 FileSize;
	/** Current position of archive.												*/
	int64 CurrentPos;
	/** Start position of current precache request.									*/
	int64 PrecacheStartPos;
	/** End position (exclusive) of current precache request.						*/
	int64 PrecacheEndPos;

	int64 ReadRequestOffset;
	int64 ReadRequestSize;

	int64 HeaderSize;
	int64 HeaderSizeWhenReadingExportsFromSplitFile;

	ELoadPhase LoadPhase;
	bool bKeepRestOfFilePrecached;
	FAsyncFileCallBack ReadCallbackFunction;
	/** Cached filename for debugging.												*/
	FString	FileName;
	double OpenTime;
	double SummaryReadTime;
	double ExportReadTime;
#if USE_EVENT_DRIVEN_ASYNC_LOAD
	TFunction<void()> SummaryReadyCallback;
	FAsyncFileCallBack ReadCallbackFunctionForLinkerLoad;
#endif

};

#endif
