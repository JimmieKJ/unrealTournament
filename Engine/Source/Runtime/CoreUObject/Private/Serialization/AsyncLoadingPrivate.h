// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

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

	FArchiveAsync2(const TCHAR* InFileName);
	virtual ~FArchiveAsync2();
	virtual bool Close() override;
	virtual bool SetCompressionMap(TArray<FCompressedChunk>* CompressedChunks, ECompressionFlags CompressionFlags) override;
	virtual bool Precache(int64 PrecacheOffset, int64 PrecacheSize) override;
	virtual void Serialize(void* Data, int64 Num) override;
	virtual int64 Tell() override
	{
		return CurrentPos;
	}
	virtual int64 TotalSize() override;
	virtual void Seek(int64 InPos) override;
	virtual void FlushCache() override;

	bool Precache(int64 PrecacheOffset, int64 PrecacheSize, bool bUseTimeLimit, bool bUseFullTimeLimit, double TickStartTime, float TimeLimit, bool bTrimPrecache);
	bool ReadyToStartReadingHeader(bool bUseTimeLimit, bool bUseFullTimeLimit, double TickStartTime, float TimeLimit);
	void StartReadingHeader();
	void EndReadingHeader();

private:

	bool WaitRead(float TimeLimit = 0.0f);
	void CompleteRead();
	void CancelRead();
	void CompleteCancel();
	void CompletePrecache();
	bool WaitForIntialPhases(float TimeLimit = 0.0f);
	void DispatchPrecache(int64 PrecacheAt = 0, bool bForceTrimPrecache = false);
	void ReadCallback(bool bWasCancelled, IAsyncReadRequest*);
	bool PrecacheInternal(int64 PrecacheOffset, int64 PrecacheSize);
	
	void LogItem(const TCHAR* Item, int64 Offset = 0, int64 Size = 0, double StartTime = 0.0);

	FORCEINLINE int64 TotalSizeOrMaxInt64IfNotReady()
	{
		return SizeRequestPtr ? MAX_int64 : FileSize;
	}

	IAsyncReadFileHandle* Handle;
	IAsyncReadRequest* SizeRequestPtr;
	IAsyncReadRequest* SummaryRequestPtr;
	IAsyncReadRequest* SummaryPrecacheRequestPtr;
	IAsyncReadRequest* ReadRequestPtr;
	IAsyncReadRequest* PrecacheRequestPtr;
	IAsyncReadRequest* CanceledReadRequestPtr;
	/** Buffer containing precached data.											*/
	uint8* PrecacheBuffer;
	/** Buffer containing precached data.											*/
	uint8* NextPrecacheBuffer;
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
	int64 PrecacheOffset;

	int64 HeaderSize;

	ELoadPhase LoadPhase;
	bool bKeepRestOfFilePrecached;
	FAsyncFileCallBack ReadCallbackFunction;
	/** Cached filename for debugging.												*/
	FString	FileName;
	double OpenTime;
	double SummaryReadTime;
	double ExportReadTime;
	double PrecacheEOFTime;
};

#endif