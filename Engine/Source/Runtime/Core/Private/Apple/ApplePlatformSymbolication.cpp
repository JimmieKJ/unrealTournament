// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformSymbolication.cpp: Apple platform implementation of symbolication
=============================================================================*/

#include "CorePrivatePCH.h"

#include "ApplePlatformSymbolication.h"
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>

#if PLATFORM_MAC && IS_PROGRAM
extern "C"
{
	struct CSTypeRef
	{
		void* Buffer0;
		void* Buffer1;
	};
	
	typedef CSTypeRef CSSymbolicatorRef;
	typedef CSTypeRef CSSourceInfoRef;
	typedef CSTypeRef CSSymbolRef;
	typedef CSTypeRef CSSymbolOwnerRef;
	
	struct CSRange
	{
		uint64 Location;
		uint64 Length;
	};
	
	#define kCSNow								0x80000000u
	
	Boolean CSEqual(CSTypeRef CS1, CSTypeRef CS2);
	CFIndex CSGetRetainCount(CSTypeRef CS);
	Boolean CSIsNull(CSTypeRef CS);
	CSTypeRef CSRetain(CSTypeRef CS);
	void CSRelease(CSTypeRef CS);
	void CSShow(CSTypeRef CS);
	
	CSSymbolicatorRef CSSymbolicatorCreateWithPid(pid_t pid);
	CSSymbolicatorRef CSSymbolicatorCreateWithPathAndArchitecture(const char* path, cpu_type_t type);
	
	CSSourceInfoRef CSSymbolicatorGetSourceInfoWithAddressAtTime(CSSymbolicatorRef Symbolicator, vm_address_t Address, uint64_t Time);
	CSSymbolRef CSSymbolicatorGetSymbolWithMangledNameFromSymbolOwnerWithNameAtTime(CSSymbolicatorRef Symbolicator, const char* Symbol, const char* Name, uint64_t Time);
	CSSymbolOwnerRef CSSymbolicatorGetSymbolOwnerWithUUIDAtTime(CSSymbolicatorRef Symbolicator, CFUUIDRef UUID, uint64_t Time);
	
	const char* CSSymbolGetName(CSSymbolRef Symbol);
	CSRange CSSymbolGetRange(CSSymbolRef Symbol);
	CSSymbolOwnerRef CSSourceInfoGetSymbolOwner(CSSourceInfoRef Info);
	
	const char* CSSymbolOwnerGetName(CSSymbolOwnerRef symbol);
	
	int CSSourceInfoGetLineNumber(CSSourceInfoRef Info);
	const char* CSSourceInfoGetPath(CSSourceInfoRef Info);
	CSRange CSSourceInfoGetRange(CSSourceInfoRef Info);
	CSSymbolRef CSSourceInfoGetSymbol(CSSourceInfoRef Info);
}
#endif

struct FApplePlatformSymbolCache
{
#if PLATFORM_MAC && IS_PROGRAM
	TMap< FString, CSSymbolicatorRef > Symbolicators;
#endif
};

static bool GAllowApplePlatformSymbolication = (PLATFORM_MAC != 0) && (IS_PROGRAM != 0);

void FApplePlatformSymbolication::SetSymbolicationAllowed(bool const bAllow)
{
	GAllowApplePlatformSymbolication = bAllow;
}

FApplePlatformSymbolCache* FApplePlatformSymbolication::CreateSymbolCache(void)
{
#if PLATFORM_MAC && IS_PROGRAM
	return GAllowApplePlatformSymbolication ? new FApplePlatformSymbolCache : nullptr;
#else 
	return nullptr;
#endif
}

void FApplePlatformSymbolication::DestroySymbolCache(FApplePlatformSymbolCache* Cache)
{
#if PLATFORM_MAC && IS_PROGRAM
	if(GAllowApplePlatformSymbolication && Cache)
	{
		for(auto It : Cache->Symbolicators)
		{
			CSRelease( It.Value );
		}
		delete Cache;
	}
#endif
}

bool FApplePlatformSymbolication::SymbolInfoForAddress(uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo)
{
#if PLATFORM_MAC && IS_PROGRAM
	bool bOK = false;
	if (GAllowApplePlatformSymbolication)
	{
		CSSymbolicatorRef Symbolicator = CSSymbolicatorCreateWithPid(FPlatformProcess::GetCurrentProcessId());
		if(!CSIsNull(Symbolicator))
		{
			CSSourceInfoRef Symbol = CSSymbolicatorGetSourceInfoWithAddressAtTime(Symbolicator, (vm_address_t)ProgramCounter, kCSNow);
			
			if(!CSIsNull(Symbol))
			{
				out_SymbolInfo.LineNumber = CSSourceInfoGetLineNumber(Symbol);
				FCStringAnsi::Sprintf(out_SymbolInfo.Filename, CSSourceInfoGetPath(Symbol));
				FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, CSSymbolGetName(CSSourceInfoGetSymbol(Symbol)));
				CSRange CodeRange = CSSourceInfoGetRange(Symbol);
				out_SymbolInfo.SymbolDisplacement = (ProgramCounter - CodeRange.Location);
				
				CSSymbolOwnerRef Owner = CSSourceInfoGetSymbolOwner(Symbol);
				if(!CSIsNull(Owner))
				{
					ANSICHAR const* DylibName = CSSymbolOwnerGetName(Owner);
					FCStringAnsi::Strcpy(out_SymbolInfo.ModuleName, DylibName);
					
					bOK = out_SymbolInfo.LineNumber != 0;
				}
			}
			
			CSRelease(Symbolicator);
		}
	}
	
	return bOK;
#else
	return false;
#endif
}

bool FApplePlatformSymbolication::SymbolInfoForFunctionFromModule(ANSICHAR const* MangledName, ANSICHAR const* ModuleName, FProgramCounterSymbolInfo& Info)
{
#if PLATFORM_MAC && IS_PROGRAM
	bool bOK = false;
	
	if (GAllowApplePlatformSymbolication)
	{
		CSSymbolicatorRef Symbolicator = CSSymbolicatorCreateWithPid(FPlatformProcess::GetCurrentProcessId());
		if(!CSIsNull(Symbolicator))
		{
			CSSymbolRef Symbol = CSSymbolicatorGetSymbolWithMangledNameFromSymbolOwnerWithNameAtTime(Symbolicator, MangledName, ModuleName, kCSNow);
			
			if(!CSIsNull(Symbol))
			{
				CSRange CodeRange = CSSymbolGetRange(Symbol);
			
				bOK = SymbolInfoForAddress(CodeRange.Location, Info);
			}
			
			CSRelease(Symbolicator);
		}
	}
	
	return bOK;
#else
	return false;
#endif
}

bool FApplePlatformSymbolication::SymbolInfoForStrippedSymbol(FApplePlatformSymbolCache* Cache, uint64 ProgramCounter, ANSICHAR const* ModulePath, ANSICHAR const* ModuleUUID, FProgramCounterSymbolInfo& Info)
{
#if PLATFORM_MAC && IS_PROGRAM
	bool bOK = false;
	
	if(GAllowApplePlatformSymbolication && IFileManager::Get().FileSize(UTF8_TO_TCHAR(ModulePath)) > 0)
	{
		CSSymbolicatorRef Symbolicator = { nullptr, nullptr };
		if ( Cache )
		{
			Symbolicator = Cache->Symbolicators.FindOrAdd( FString(ModulePath) );
		}
		if( CSIsNull(Symbolicator) )
		{
			Symbolicator = CSSymbolicatorCreateWithPathAndArchitecture(ModulePath, CPU_TYPE_X86_64);
		}
		if( !CSIsNull(Symbolicator) )
		{
			if ( Cache )
			{
				Cache->Symbolicators[ FString(ModulePath) ] = Symbolicator;
			}
			
			FString ModuleID(ModuleUUID);
			CFUUIDRef UUID = CFUUIDCreateFromString(nullptr, (CFStringRef)ModuleID.GetNSString());
			check(UUID);
			
			CSSymbolOwnerRef SymbolOwner = CSSymbolicatorGetSymbolOwnerWithUUIDAtTime(Symbolicator, UUID, kCSNow);
			if(!CSIsNull(SymbolOwner))
			{
				CSSourceInfoRef Symbol = CSSymbolicatorGetSourceInfoWithAddressAtTime(Symbolicator, (vm_address_t)ProgramCounter, kCSNow);
				
				if(!CSIsNull(Symbol))
				{
					Info.LineNumber = CSSourceInfoGetLineNumber(Symbol);
					
					CSRange CodeRange = CSSourceInfoGetRange(Symbol);
					Info.SymbolDisplacement = (ProgramCounter - CodeRange.Location);
					
					ANSICHAR const* FileName = CSSourceInfoGetPath(Symbol);
					if(FileName)
					{
						FCStringAnsi::Sprintf(Info.Filename, FileName);
					}
					
					CSSymbolRef SymbolData = CSSourceInfoGetSymbol(Symbol);
					if(!CSIsNull(SymbolData))
					{
						ANSICHAR const* FunctionName = CSSymbolGetName(SymbolData);
						if(FunctionName)
						{
							FCStringAnsi::Sprintf(Info.FunctionName, FunctionName);
						}
					}
					
					CSSymbolOwnerRef Owner = CSSourceInfoGetSymbolOwner(Symbol);
					if(!CSIsNull(Owner))
					{
						ANSICHAR const* DylibName = CSSymbolOwnerGetName(Owner);
						FCStringAnsi::Strcpy(Info.ModuleName, DylibName);
						
						bOK = Info.LineNumber != 0;
					}
				}
			}
			
			CFRelease(UUID);
			
			if ( !Cache )
			{
				CSRelease(Symbolicator);
			}
		}
	}
	
	return bOK;
#else
	return false;
#endif
}
