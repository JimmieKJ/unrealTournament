// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocLeakDetection.cpp: Helper class to track memory allocations
=============================================================================*/

#include "HAL/MallocLeakDetection.h"
#include "Logging/LogMacros.h"
#include "HAL/PlatformTLS.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDeviceArchiveWrapper.h"
#include "Misc/DateTime.h"
#include "ProfilingDebugging/ProfilingHelpers.h"

#if MALLOC_LEAKDETECTION

FMallocLeakDetection::FMallocLeakDetection()
	: bRecursive(false)
	, bCaptureAllocs(false)
	, MinAllocationSize(0)
	, TotalTracked(0)
{
	Contexts.Empty(16);
}

FMallocLeakDetection& FMallocLeakDetection::Get()
{
	static FMallocLeakDetection Singleton;
	return Singleton;
}

FMallocLeakDetection::~FMallocLeakDetection()
{	
}

static uint32 ContextsTLSID = FPlatformTLS::AllocTlsSlot();

void FMallocLeakDetection::PushContext(const FString& Context)
{
	FMallocLeakDetectionProxy::Get().Lock();

	FScopeLock Lock(&AllocatedPointersCritical);

	TArray<ContextString>* TLContexts = (TArray<ContextString>*)FPlatformTLS::GetTlsValue(ContextsTLSID);
	if (!TLContexts)
	{
		TLContexts = new TArray<ContextString>();
		FPlatformTLS::SetTlsValue(ContextsTLSID, TLContexts);
	}

	bRecursive = true;

	ContextString Str;
	FCString::Strncpy(Str.Buffer, *Context, 128);
	TLContexts->Push(Str);
	bRecursive = false;

	FMallocLeakDetectionProxy::Get().Unlock();
}

void FMallocLeakDetection::PopContext()
{
	TArray<ContextString>* TLContexts = (TArray<ContextString>*)FPlatformTLS::GetTlsValue(ContextsTLSID);
	check(TLContexts);
	TLContexts->Pop(false);
}

bool FMallocLeakDetection::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("mallocleak")))
	{
		if (FParse::Command(&Cmd, TEXT("start")))
		{
			uint32 FilterSize = 64;
			FParse::Value(Cmd, TEXT("filtersize="), FilterSize);

			UE_LOG(LogConsoleResponse, Display, TEXT("Starting tracking of allocations >= %d KB"), FilterSize);
			SetAllocationCollection(true, FilterSize * 1024);
		}
		else if (FParse::Command(&Cmd, TEXT("stop")))
		{
			uint32 FilterSize = 64;
			FParse::Value(Cmd, TEXT("filtersize="), FilterSize);

			FilterSize *= 1024;

			UE_LOG(LogConsoleResponse, Display, TEXT("Stopping allocation tracking and clearing data."));
			SetAllocationCollection(false);

			UE_LOG(LogConsoleResponse, Display, TEXT("Dumping unique calltacks with %i bytes or more oustanding."), FilterSize);

			FMallocLeakReportOptions Options;
			Options.FilterSize = FilterSize;
			DumpOpenCallstacks(Options);

			ClearData();
		}
		else if (FParse::Command(&Cmd, TEXT("Clear")))
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Clearing tracking data."));
			ClearData();
		}
		else if (FParse::Command(&Cmd, TEXT("Dump")))
		{
			uint32 FilterSize = 128;
			FParse::Value(Cmd, TEXT("filtersize="), FilterSize);
			FilterSize *= 1024;

			FString FileName;
			FParse::Value(Cmd, TEXT("name="), FileName);

			UE_LOG(LogConsoleResponse, Display, TEXT("Dumping unique calltacks with %i KB or more oustanding."), FilterSize/1024);

			FMallocLeakReportOptions Options;
			Options.FilterSize = FilterSize;
			Options.OutputFile = *FileName;

			DumpOpenCallstacks(Options);
		}
		else if (FParse::Command(&Cmd, TEXT("dumpleaks")))
		{
			uint32 FilterSize = 0;
			FParse::Value(Cmd, TEXT("filtersize="), FilterSize);
			FilterSize *= 1024;

			FString FileName;
			FParse::Value(Cmd, TEXT("name="), FileName);

			FMallocLeakReportOptions Options;
			Options.FilterSize = FilterSize;
			Options.OutputFile = *FileName;

			UE_LOG(LogConsoleResponse, Display, TEXT("Dumping potential leakers with %i KB or more oustanding."), FilterSize / 1024);
			DumpPotentialLeakers(Options);
		}
		else if (FParse::Command(&Cmd, TEXT("Checkpoint")))
		{
			CheckpointLinearFit();
		}
		else
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("'MallocLeak Start [filtersize=Size]'  Begins accumulating unique callstacks of allocations > Size KBytes\n")
				TEXT("'MallocLeak stop filtersize=[Size]'  Dumps outstanding unique callstacks and stops accumulation (with an optional filter size in KBytes, if not specified 128 KB is assumed).  Also clears data.\n")
				TEXT("'MallocLeak clear'  Clears all accumulated data.  Does not change start/stop state.\n")
				TEXT("'MallocLeak dump filtersize=[Size] name=[file]' Dumps oustanding unique callstacks with optional size filter in KBytes (if size is not specified it is assumed to be 128 KB).\n")
				TEXT("'MallocLeak dumpleaks filtersize=[Size] name=[file]' Dumps oustanding unique callstacks with optional size filter in KBytes (if size is not specified it is assumed to be 0).\n")
			);
		}

		return true;
	}
	return false;
}


void FMallocLeakDetection::AddCallstack(FCallstackTrack& Callstack)
{
	FScopeLock Lock(&AllocatedPointersCritical);
	uint32 CallstackHash = Callstack.GetHash();
	FCallstackTrack& UniqueCallstack = UniqueCallstacks.FindOrAdd(CallstackHash);
	//if we had a hash collision bail and lose the data rather than corrupting existing data.
	if ((UniqueCallstack.Count > 0 || UniqueCallstack.NumCheckPoints > 0) && UniqueCallstack != Callstack)
	{
		ensureMsgf(false, TEXT("Callstack hash collision.  Throwing away new stack."));
		return;
	}

	if (UniqueCallstack.Count == 0 && UniqueCallstack.NumCheckPoints == 0)
	{
		UniqueCallstack = Callstack;
	}
	else
	{
		UniqueCallstack.Size += Callstack.Size;
		UniqueCallstack.LastFrame = Callstack.LastFrame;
	}
	UniqueCallstack.Count++;	
	TotalTracked += Callstack.Size;
}

void FMallocLeakDetection::RemoveCallstack(FCallstackTrack& Callstack)
{
	FScopeLock Lock(&AllocatedPointersCritical);
	uint32 CallstackHash = FCrc::MemCrc32(Callstack.CallStack, sizeof(Callstack.CallStack), 0);
	FCallstackTrack* UniqueCallstack = UniqueCallstacks.Find(CallstackHash);
	if (UniqueCallstack)
	{
		UniqueCallstack->Count--;
		UniqueCallstack->Size -= Callstack.Size;
		if (UniqueCallstack->Count == 0)
		{
			UniqueCallstack = nullptr;
			UniqueCallstacks.Remove(CallstackHash);
		}
		TotalTracked -= Callstack.Size;
	}
}

void FMallocLeakDetection::SetAllocationCollection(bool bEnabled, int32 Size)
{
	FScopeLock Lock(&AllocatedPointersCritical);
	bCaptureAllocs = bEnabled;

	if (bEnabled)
	{
		MinAllocationSize = Size;
	}
}

void FMallocLeakDetection::GetOpenCallstacks(TArray<uint32>& OutCallstacks, const FMallocLeakReportOptions& Options /*= FMallocLeakReportOptions()*/)
{
	// Make sure we preallocate memory to avoid
	OutCallstacks.Empty(UniqueCallstacks.Num() + 32);

	{
		FScopeLock Lock(&AllocatedPointersCritical);

		for (const auto& Pair : UniqueCallstacks)
		{

			// Filter on type?
			if (Options.OnlyNonDeleters && KnownDeleters.Contains(Pair.Value.GetHash()))
			{
				continue;
			}

			// frame number?
			if (Options.FrameStart > Pair.Value.LastFrame)
			{
				continue;
			}

			if (Options.FrameEnd && Options.FrameEnd < Pair.Value.LastFrame)
			{
				continue;
			}

			// Filter on size?
			if (Pair.Value.Size < Options.FilterSize)
			{
				continue;
			}

			OutCallstacks.Add(Pair.Key);
		}
	}
}

void FMallocLeakDetection::CheckpointLinearFit()
{
	FScopeLock Lock(&AllocatedPointersCritical);

	float FrameNum = float(GFrameCounter);
	float FrameNum2 = FrameNum * FrameNum;

	for (auto& Pair : UniqueCallstacks)
	{
		FCallstackTrack& Callstack = Pair.Value;

		Callstack.NumCheckPoints++;
		Callstack.SumOfFramesNumbers += FrameNum;
		Callstack.SumOfFramesNumbersSquared += FrameNum2;
		Callstack.SumOfMemory += float(Callstack.Size);
		Callstack.SumOfMemoryTimesFrameNumber += float(Callstack.Size) * FrameNum;
		Callstack.GetLinearFit();
	}
}

void FMallocLeakDetection::FCallstackTrack::GetLinearFit()
{
	Baseline = 0.0f;
	BytesPerFrame = 0.0f;
	float DetInv = (float(NumCheckPoints) * SumOfFramesNumbersSquared) - (2.0f * SumOfFramesNumbers);
	if (!NumCheckPoints || DetInv == 0.0f)
	{
		return;
	}
	float Det = 1.0f / DetInv;

	float AtAInv[2][2];

	AtAInv[0][0] = Det * SumOfFramesNumbersSquared;
	AtAInv[0][1] = -Det * SumOfFramesNumbers;
	AtAInv[1][0] = -Det * SumOfFramesNumbers;
	AtAInv[1][1] = Det * float(NumCheckPoints);

	Baseline = AtAInv[0][0] * SumOfMemory + AtAInv[0][1] * SumOfMemoryTimesFrameNumber;
	BytesPerFrame = AtAInv[1][0] * SumOfMemory + AtAInv[1][1] * SumOfMemoryTimesFrameNumber;
}

#define LOG_OUTPUT(Format, ...) \
	do {\
		if (ReportAr){\
			ReportAr->Logf(Format, ##__VA_ARGS__);\
		}\
		else {\
			FPlatformMisc::LowLevelOutputDebugStringf(Format, ##__VA_ARGS__);\
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\n"));\
		}\
	} while(0);\

int32 FMallocLeakDetection::DumpPotentialLeakers(const FMallocLeakReportOptions& Options /*= FMallocLeakReportOptions()*/)
{
	const int32 MaxCallstackLineChars = 2048;
	char CallstackString[MaxCallstackLineChars];
	TCHAR CallstackStringWide[MaxCallstackLineChars];
	FMemory::Memzero(CallstackString);

	TArray<uint32> CallstackHashes;

	GetOpenCallstacks(CallstackHashes, Options);

	CheckpointLinearFit();

	const int kMaxDump = 30;

	TMap<uint32, float> HashesToLeakRate;
	float TotalLeakRate = 0.0f;
	SIZE_T TotalSize = 0;


	for (uint32 Hash : CallstackHashes)
	{
		FCallstackTrack Callstack;

		{
			FScopeLock Lock(&AllocatedPointersCritical);
			if (UniqueCallstacks.Contains(Hash) == false)
				continue;
			
			Callstack = UniqueCallstacks[Hash];
		}
				
		if (Callstack.NumCheckPoints > 5 && Callstack.BytesPerFrame > .01f && Callstack.Count > 5)
		{		
			HashesToLeakRate.Add(Hash) = Callstack.BytesPerFrame;

			TotalLeakRate += Callstack.BytesPerFrame;
			TotalSize += Callstack.Size;

			if (HashesToLeakRate.Num() > kMaxDump)
			{
				float SmallestValue = MAX_FLT;
				uint32 SmallestKey = 0;
				for (const auto& KV : HashesToLeakRate)
				{
					if (KV.Value < SmallestValue)
					{
						SmallestValue = Callstack.BytesPerFrame;
						SmallestKey = KV.Key;
					}
				}

				check(SmallestKey);
				HashesToLeakRate.Remove(SmallestKey);
				TotalLeakRate -= SmallestValue;
				TotalSize -= UniqueCallstacks[SmallestKey].Size;
			}
		}
	}

	if (HashesToLeakRate.Num() > 0)
	{
		FOutputDevice* ReportAr = nullptr;
		FArchive* FileAr = nullptr;
		FOutputDeviceArchiveWrapper* FileArWrapper = nullptr;

		if (Options.OutputFile && FCString::Strlen(Options.OutputFile))
		{
			const FString PathName = *(FPaths::ProfilingDir() + TEXT("memreports/"));
			IFileManager::Get().MakeDirectory(*PathName);

			FString FilePath = PathName + CreateProfileFilename(Options.OutputFile, TEXT(".leaks"), true);

			FileAr = IFileManager::Get().CreateDebugFileWriter(*FilePath);
			FileArWrapper = new FOutputDeviceArchiveWrapper(FileAr);
			ReportAr = FileArWrapper;

			//UE_LOG(LogConsoleResponse, Log, TEXT("MemReportDeferred: saving to %s"), *FilePath);
		}

		const float InvToMb = 1.0 / (1024 * 1024);
		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();

		LOG_OUTPUT(TEXT("Current Time: %s, Current Frame %d"), *FDateTime::Now().ToString(TEXT("%m.%d-%H.%M.%S")), GFrameCounter);

		LOG_OUTPUT(TEXT("Current Memory: %.02fMB (Peak: %.02fMB)."),
			MemoryStats.UsedPhysical * InvToMb,
			MemoryStats.PeakUsedPhysical * InvToMb);

		LOG_OUTPUT(TEXT("Tracking %d callstacks that hold %.02fMB"), UniqueCallstacks.Num(), TotalTracked * InvToMb);

		LOG_OUTPUT(TEXT("Leak detection allocation filter: %dKB"), MinAllocationSize / 1024);
		LOG_OUTPUT(TEXT("Leak detection report filter: %dKB"), Options.FilterSize / 1024);
		LOG_OUTPUT(TEXT("Dumping %d callstacks that hold %.02fMB and may be leaking memory."), HashesToLeakRate.Num(), TotalSize * InvToMb);
		LOG_OUTPUT(TEXT("Current estimated leak rate: %.02fB/frame\n.") , TotalLeakRate);

		TArray<uint32> SortedKeys;
		HashesToLeakRate.GetKeys(SortedKeys);

		SortedKeys.Sort([&](uint32 lhs, uint32 rhs) {
			//return HashesToLeakRate[rhs] < HashesToLeakRate[lhs];
			return lhs < rhs;
		});

		for (uint32 Key : SortedKeys)
		{
			FCallstackTrack Callstack;

			{
				FScopeLock Lock(&AllocatedPointersCritical);
				if (UniqueCallstacks.Contains(Key) == false)
					continue;

				Callstack = UniqueCallstacks[Key];
			}

			bool KnownDeleter = KnownDeleters.Contains(Callstack.GetHash());

			LOG_OUTPUT(TEXT("AllocSize: %i KB, Num: %i, FirstFrame %i, LastFrame %i, KnownDeleter: %d, Baseline memory %.3fMB  Leak Rate %.2fB/frame"),
				Callstack.Size / 1024
				, Callstack.Count
				, Callstack.FirstFame
				, Callstack.LastFrame
				, KnownDeleter
				, Callstack.Baseline / 1024.0f / 1024.0f
				, Callstack.BytesPerFrame);

			for (int32 i = 0; i < FCallstackTrack::Depth; ++i)
			{
				FPlatformStackWalk::ProgramCounterToHumanReadableString(i, Callstack.CallStack[i], CallstackString, MaxCallstackLineChars);

				if (Callstack.CallStack[i] && FCStringAnsi::Strlen(CallstackString) > 0)
				{
					//convert ansi -> tchar without mallocs in case we are in OOM handler.
					for (int32 CurrChar = 0; CurrChar < MaxCallstackLineChars; ++CurrChar)
					{
						CallstackStringWide[CurrChar] = CallstackString[CurrChar];
					}

					LOG_OUTPUT(TEXT("%s"), CallstackStringWide);
				}
				FMemory::Memzero(CallstackString);
			}

			{
				FScopeLock Lock(&AllocatedPointersCritical);

				TArray<FString> SortedContexts;

				for (const auto& Pair : OpenPointers)
				{
					if (Pair.Value.GetHash() == Key)
					{
						if (PointerContexts.Contains(Pair.Key))
						{
							SortedContexts.Add(*PointerContexts.Find(Pair.Key));
						}
					}
				}

				if (SortedContexts.Num())
				{
					LOG_OUTPUT(TEXT("%d contexts:"), SortedContexts.Num(), CallstackStringWide);

					SortedContexts.Sort();

					for (const auto& Str : SortedContexts)
					{
						LOG_OUTPUT(TEXT("\t%s"), *Str);
					}
				}
			}

			LOG_OUTPUT(TEXT("\n"));
		}

		if (FileArWrapper != nullptr)
		{
			FileArWrapper->TearDown();
			delete FileArWrapper;
			delete FileAr;
		}
	}

	return HashesToLeakRate.Num();
}

int32 FMallocLeakDetection::DumpOpenCallstacks(const FMallocLeakReportOptions& Options /*= FMallocLeakReportOptions()*/)
{
	const int32 MaxCallstackLineChars = 2048;
	char CallstackString[MaxCallstackLineChars];
	TCHAR CallstackStringWide[MaxCallstackLineChars];
	FMemory::Memzero(CallstackString);

	SIZE_T ReportedSize = 0;
	SIZE_T TotalSize = 0;

	TArray<uint32> SortedKeys;

	GetOpenCallstacks(SortedKeys, Options);

	{
		FScopeLock Lock(&AllocatedPointersCritical);

		SortedKeys.Sort([this](uint32 lhs, uint32 rhs) {

			FCallstackTrack& Left = UniqueCallstacks[lhs];
			FCallstackTrack& Right = UniqueCallstacks[rhs];

			//return Right.Size < Left.Size;
			return Left.GetHash() < Right.GetHash();
		});

		for (uint32 ID : SortedKeys)
		{
			ReportedSize += UniqueCallstacks[ID].Size;
		}
	}
	
	FOutputDevice* ReportAr = nullptr;
	FArchive* FileAr = nullptr;
	FOutputDeviceArchiveWrapper* FileArWrapper = nullptr;

	if (Options.OutputFile && FCString::Strlen(Options.OutputFile))
	{
		const FString PathName = *(FPaths::ProfilingDir() + TEXT("memreports/"));
		IFileManager::Get().MakeDirectory(*PathName);
		
		FString FilePath = PathName + CreateProfileFilename(Options.OutputFile, TEXT(".allocs"), true);

		FileAr = IFileManager::Get().CreateDebugFileWriter(*FilePath);
		FileArWrapper = new FOutputDeviceArchiveWrapper(FileAr);
		ReportAr = FileArWrapper;

		//UE_LOG(LogConsoleResponse, Log, TEXT("MemReportDeferred: saving to %s"), *FilePath);
	}

	const float InvToMb = 1.0 / (1024 * 1024);
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();

	LOG_OUTPUT(TEXT("Current Time: %s, Current Frame %d"), *FDateTime::Now().ToString(TEXT("%m.%d-%H.%M.%S")), GFrameCounter);

	LOG_OUTPUT(TEXT("Current Memory: %.02fMB (Peak: %.02fMB)."),
		MemoryStats.UsedPhysical * InvToMb,
		MemoryStats.PeakUsedPhysical * InvToMb);

	LOG_OUTPUT(TEXT("Tracking %d callstacks that hold %.02fMB"), UniqueCallstacks.Num(), TotalTracked * InvToMb);

	LOG_OUTPUT(TEXT("Leak detection allocation filter: %dKB"), MinAllocationSize / 1024);
	LOG_OUTPUT(TEXT("Leak detection report filter: %dKB"), Options.FilterSize / 1024);

	LOG_OUTPUT(TEXT("Have  %i open callstacks at frame: %i"), UniqueCallstacks.Num(),(int32)GFrameCounter);
	
	//for (const auto& Key : SortedKeys)

	LOG_OUTPUT(TEXT("Dumping %d callstacks that hold more than %uKBs and total %uKBs"),
		SortedKeys.Num(), Options.FilterSize / 1024, ReportedSize / 1024);

	for (const auto& Key : SortedKeys)
	{		
		FCallstackTrack Callstack;

		{
			FScopeLock Lock(&AllocatedPointersCritical);
			if (UniqueCallstacks.Contains(Key) == false)
				continue;

			Callstack = UniqueCallstacks[Key];
		}

		bool KnownDeleter = KnownDeleters.Contains(Callstack.GetHash());

		LOG_OUTPUT(TEXT("\nAllocSize: %i KB, Num: %i, FirstFrame %i, LastFrame %i, KnownDeleter: %d")
			, Callstack.Size / 1024
			, Callstack.Count
			, Callstack.FirstFame
			, Callstack.LastFrame
			, KnownDeleter);

		for (int32 i = 0; i < FCallstackTrack::Depth; ++i)
		{
			FPlatformStackWalk::ProgramCounterToHumanReadableString(i, Callstack.CallStack[i], CallstackString, MaxCallstackLineChars);

			if (Callstack.CallStack[i] && FCStringAnsi::Strlen(CallstackString) > 0)
			{
				//convert ansi -> tchar without mallocs in case we are in OOM handler.
				for (int32 CurrChar = 0; CurrChar < MaxCallstackLineChars; ++CurrChar)
				{
					CallstackStringWide[CurrChar] = CallstackString[CurrChar];
				}

				LOG_OUTPUT(TEXT("%s"), CallstackStringWide);
			}
			FMemory::Memzero(CallstackString);
		}

		TArray<FString> SortedContexts;

		for (const auto& Pair : OpenPointers)
		{
			if (Pair.Value.GetHash() == Key)
			{
				if (PointerContexts.Contains(Pair.Key))
				{
					SortedContexts.Add(*PointerContexts.Find(Pair.Key));
				}
			}
		}

		if (SortedContexts.Num())
		{
			LOG_OUTPUT(TEXT("%d contexts:"), SortedContexts.Num(), CallstackStringWide);

			SortedContexts.Sort();

			for (const auto& Str : SortedContexts)
			{
				LOG_OUTPUT(TEXT("\t%s"), *Str);
			}
		}

		LOG_OUTPUT(TEXT("\n"));
	}

	if (FileArWrapper != nullptr)
	{
		FileArWrapper->TearDown();
		delete FileArWrapper;
		delete FileAr;
	}

	return SortedKeys.Num();
}

#undef LOG_OUTPUT

void FMallocLeakDetection::ClearData()
{
	bool OldCapture = bCaptureAllocs;
	{
		FScopeLock Lock(&AllocatedPointersCritical);
		bCaptureAllocs = false;
	}

	OpenPointers.Empty();
	UniqueCallstacks.Empty();
	PointerContexts.Empty();

	{
		FScopeLock Lock(&AllocatedPointersCritical);
		bCaptureAllocs = OldCapture;
	}
}

void FMallocLeakDetection::Malloc(void* Ptr, SIZE_T Size)
{
	if (Ptr)
	{
		if (bCaptureAllocs && (MinAllocationSize == 0 || Size >= MinAllocationSize))
		{			
			FScopeLock Lock(&AllocatedPointersCritical);

			if (!bRecursive)
			{
				bRecursive = true;
				FCallstackTrack Callstack;
				FPlatformStackWalk::CaptureStackBackTrace(Callstack.CallStack, FCallstackTrack::Depth);
				Callstack.FirstFame = GFrameCounter;
				Callstack.LastFrame = GFrameCounter;
				Callstack.Size = Size;
				AddCallstack(Callstack);
				OpenPointers.Add(Ptr, Callstack);

				TArray<ContextString>* TLContexts = (TArray<ContextString>*)FPlatformTLS::GetTlsValue(ContextsTLSID);

				if (TLContexts && TLContexts->Num())
				{
					PointerContexts.Add(Ptr, TLContexts->Top().Buffer);
				}

				bRecursive = false;
			}
		}		
	}
}

void FMallocLeakDetection::Realloc(void* OldPtr, void* NewPtr, SIZE_T Size)
{	
	if (bRecursive == false && (bCaptureAllocs || OpenPointers.Num()))
	{
		FScopeLock Lock(&AllocatedPointersCritical);

		// realloc may return the same pointer, if so skip this
	
		if (OldPtr != NewPtr)
		{
			FCallstackTrack* Callstack = OpenPointers.Find(OldPtr);
			uint32 OldHash = 0;
			bool WasKnownDeleter = false;
			if (Callstack)
			{
				OldHash = Callstack->GetHash();
				WasKnownDeleter = KnownDeleters.Contains(OldHash);
			}

			// Note - malloc and then free so linearfit checkpoints in the callstack are
			// preserved
			Malloc(NewPtr, Size);

			// See if we should/need to copy the context across
			if (OldPtr && NewPtr)
			{
				FString* OldContext = PointerContexts.Find(OldPtr);
				FString* NewContext = PointerContexts.Find(NewPtr);

				if (OldContext && !NewContext)
				{
					FString Tmp = *OldContext;
					PointerContexts.Add(NewPtr) = Tmp;
				}
			}

			Free(OldPtr);

			// if we had a tracked callstack, and it was not a deleter, remark it as such because this 'free'
			// was not actually deallocating memory.
			if (Size > 0 && OldHash > 0 && WasKnownDeleter == false)
			{
				KnownDeleters.Remove(OldHash);
			}
		}
		else
		{
			// realloc returned the same pointer, if there was a context when the call happened then
			// update it.
			TArray<ContextString>* TLContexts = (TArray<ContextString>*)FPlatformTLS::GetTlsValue(ContextsTLSID);

			if (TLContexts && TLContexts->Num())
			{
				bRecursive = true;
				PointerContexts.Remove(OldPtr);
				PointerContexts.FindOrAdd(NewPtr) = TLContexts->Top().Buffer;
				bRecursive = false;
			}
		}
	}
}

void FMallocLeakDetection::Free(void* Ptr)
{
	if (Ptr)
	{
		if (bCaptureAllocs || OpenPointers.Num())
		{
			FScopeLock Lock(&AllocatedPointersCritical);			
			if (!bRecursive)
			{
				bRecursive = true;
				FCallstackTrack* Callstack = OpenPointers.Find(Ptr);
				if (Callstack)
				{
					RemoveCallstack(*Callstack);
					KnownDeleters.Add(Callstack->GetHash());
				}
				OpenPointers.Remove(Ptr);
				bRecursive = false;
			}

			if (PointerContexts.Contains(Ptr))
			{
				PointerContexts.Remove(Ptr);
			}
		}
	}
}

static FMallocLeakDetectionProxy* ProxySingleton = nullptr;

FMallocLeakDetectionProxy::FMallocLeakDetectionProxy(FMalloc* InMalloc)
	: UsedMalloc(InMalloc)
	, Verify(FMallocLeakDetection::Get())
{
	check(ProxySingleton == nullptr);
	ProxySingleton = this;
}

FMallocLeakDetectionProxy& FMallocLeakDetectionProxy::Get()
{
	check(ProxySingleton);
	return *ProxySingleton;
}

#endif // MALLOC_LEAKDETECTION