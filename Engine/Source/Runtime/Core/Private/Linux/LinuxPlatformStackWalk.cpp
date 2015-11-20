// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxPlatformStackWalk.cpp: Linux implementations of stack walk functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "Misc/App.h"
#include "LinuxPlatformCrashContext.h"
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <stdio.h>

// these are not actually system headers, but a TPS library (see ThirdParty/elftoolchain)
#include <libelf.h>
#include <_libelf.h>
#include <libdwarf.h>
#include <dwarf.h>

namespace LinuxStackWalkHelpers
{
	struct LinuxBacktraceSymbols
	{
		/** Lock for thread-safe initialization. */
		FCriticalSection CriticalSection;

		/** Initialized flag. If initialization failed, it don't run again. */
		bool Inited;

		/** File descriptor needed for libelf to open (our own) binary */
		int ExeFd;

		/** Elf header as used by libelf (forward-declared in the same way as libelf does it, it's normally of Elf type) */
		struct _Elf* ElfHdr;

		/** DWARF handle used by libdwarf (forward-declared in the same way as libdwarf does it, it's normally of Dwarf_Debug type) */
		struct _Dwarf_Debug	* DebugInfo;

		LinuxBacktraceSymbols()
		:	Inited(false)
		,	ExeFd(-1)
		,	ElfHdr(NULL)
		,	DebugInfo(NULL)
		{
		}

		~LinuxBacktraceSymbols();

		void Init();

		/**
		 * Gets information for the crash.
		 *
		 * @param Address the address to look up info for
		 * @param OutFunctionNamePtr pointer to function name (may be NULL). Caller doesn't have to free it, but need to consider it temporary (i.e. next GetInfoForAddress() call on any thread may change it).
		 * @param OutSourceFilePtr pointer to source filename (may be NULL). Caller doesn't have to free it, but need to consider it temporary (i.e. next GetInfoForAddress() call on any thread may change it).
		 * @param OutLineNumberPtr pointer to line in a source file (may be NULL). Caller doesn't have to free it, but need to consider it temporary (i.e. next GetInfoForAddress() call on any thread may change it).
		 *
		 * @return true if succeeded in getting the info. If false is returned, none of above parameters should be trusted to contain valid data!
		 */
		bool GetInfoForAddress(void * Address, const char **OutFunctionNamePtr, const char **OutSourceFilePtr, int *OutLineNumberPtr);

		/**
		 * Check check if address is inside this entry.
		 */
		static bool CheckAddressInRange(Dwarf_Debug DebugInfo, Dwarf_Die Die, Dwarf_Unsigned Addr);

		/**
		 * Finds a function name in DWARF DIE (Debug Information Entry) and its children.
		 * For more info on DWARF format, see http://www.dwarfstd.org/Download.php , http://www.ibm.com/developerworks/library/os-debugging/
		 * Note: that function is not exactly traversing the tree, but this "seems to work"(tm). Not sure if we need to descend properly (taking child of every sibling), this
		 * takes too much time (and callstacks seem to be fine without it).
		 */
		static void FindFunctionNameInDIEAndChildren(Dwarf_Debug DebugInfo, Dwarf_Die Die, Dwarf_Addr Addr, const char **OutFuncName);

		/**
		 * Finds a function name in DWARF DIE (Debug Information Entry).
		 * For more info on DWARF format, see http://www.dwarfstd.org/Download.php , http://www.ibm.com/developerworks/library/os-debugging/
		 *
		 * @return true if we need to stop search (i.e. either found it or some error happened)
		 */
		static bool FindFunctionNameInDIE(Dwarf_Debug DebugInfo, Dwarf_Die Die, Dwarf_Addr Addr, const char **OutFuncName);

	};

	enum
	{
		MaxMangledNameLength = 1024,
		MaxDemangledNameLength = 1024
	};

	void LinuxBacktraceSymbols::Init()
	{
		FScopeLock ScopeLock( &CriticalSection );

		if (!Inited)
		{
			// open ourselves for examination
			if (!FParse::Param( FCommandLine::Get(), TEXT(CMDARG_SUPPRESS_DWARF_PARSING)))
			{
				ExeFd = open("/proc/self/exe", O_RDONLY);
				if (ExeFd >= 0)
				{
					Dwarf_Error ErrorInfo;
					// allocate DWARF debug descriptor
					if (dwarf_init(ExeFd, DW_DLC_READ, NULL, NULL, &DebugInfo, &ErrorInfo) == DW_DLV_OK)
					{
						// get ELF descritor
						if (dwarf_get_elf(DebugInfo, &ElfHdr, &ErrorInfo) != DW_DLV_OK)
						{
							dwarf_finish(DebugInfo, &ErrorInfo);
							DebugInfo = NULL;

							close(ExeFd);
							ExeFd = -1;
						}
					}
					else
					{
						DebugInfo = NULL;
						close(ExeFd);
						ExeFd = -1;
					}
				}			
			}
			Inited = true;
		}
	}

	LinuxBacktraceSymbols::~LinuxBacktraceSymbols()
	{
		if (DebugInfo)
		{
			Dwarf_Error ErrorInfo;
			dwarf_finish(DebugInfo, &ErrorInfo);
			DebugInfo = NULL;
		}

		if (ElfHdr)
		{
			elf_end(ElfHdr);
			ElfHdr = NULL;
		}

		if (ExeFd >= 0)
		{
			close(ExeFd);
			ExeFd = -1;
		}
	}

	bool LinuxBacktraceSymbols::GetInfoForAddress(void * Address, const char **OutFunctionNamePtr, const char **OutSourceFilePtr, int *OutLineNumberPtr)
	{
		if (DebugInfo == NULL)
		{
			return false;
		}

		Dwarf_Die Die;
		Dwarf_Unsigned Addr = reinterpret_cast< Dwarf_Unsigned >( Address ), LineNumber = 0;
		const char * SrcFile = NULL;

		static_assert(sizeof(Dwarf_Unsigned) >= sizeof(Address), "Dwarf_Unsigned type should be long enough to represent pointers. Check libdwarf bitness.");

		int ReturnCode = DW_DLV_OK;
		Dwarf_Error ErrorInfo;
		bool bExitHeaderLoop = false;
		int32 MaxCompileUnitsAllowed = 16 * 1024 * 1024;	// safeguard to make sure we never get into an infinite loop
		const int32 kMaxBufferLinesAllowed = 16 * 1024 * 1024;	// safeguard to prevent too long line loop
		for(;;)
		{
			if (--MaxCompileUnitsAllowed <= 0)
			{
				fprintf(stderr, "Breaking out from what seems to be an infinite loop during DWARF parsing (too many compile units).\n");
				ReturnCode = DW_DLE_DIE_NO_CU_CONTEXT;	// invalidate
				break;
			}

			if (bExitHeaderLoop)
				break;

			ReturnCode = dwarf_next_cu_header(DebugInfo, NULL, NULL, NULL, NULL, NULL, &ErrorInfo);
			if (ReturnCode != DW_DLV_OK)
				break;

			Die = NULL;

			while(dwarf_siblingof(DebugInfo, Die, &Die, &ErrorInfo) == DW_DLV_OK)
			{
				Dwarf_Half Tag;
				if (dwarf_tag(Die, &Tag, &ErrorInfo) != DW_DLV_OK)
				{
					bExitHeaderLoop = true;
					break;
				}

				if (Tag == DW_TAG_compile_unit)
				{
					break;
				}
			}

			if (Die == NULL)
			{
				break;
			}

			// check if address is inside this CU
			if (!CheckAddressInRange(DebugInfo, Die, Addr))
			{
				continue;
			}

			Dwarf_Line * LineBuf;
			Dwarf_Signed NumLines = kMaxBufferLinesAllowed;
			if (dwarf_srclines(Die, &LineBuf, &NumLines, &ErrorInfo) != DW_DLV_OK)
			{
				// could not get line info for some reason
				break;
			}

			if (NumLines >= kMaxBufferLinesAllowed)
			{
				fprintf(stderr, "Number of lines associated with a DIE looks unreasonable (%d), early quitting.\n", static_cast<int32>(NumLines));
				ReturnCode = DW_DLE_DIE_NO_CU_CONTEXT;	// invalidate
				break;
			}

			// look which line is that
			Dwarf_Addr LineAddress, PrevLineAddress = ~0ULL;
			Dwarf_Unsigned LineIdx = NumLines;
			for (int Idx = 0; Idx < NumLines; ++Idx)
			{
				if (dwarf_lineaddr(LineBuf[Idx], &LineAddress, &ErrorInfo) != 0)
				{
					bExitHeaderLoop = true;
					break;
				}
				// check if we hit the exact line
				if (Addr == LineAddress)
				{
					LineIdx = Idx;
					bExitHeaderLoop = true;
					break;
				}
				else if (PrevLineAddress < Addr && Addr < LineAddress)
				{
					LineIdx = Idx - 1;
					break;
				}
				PrevLineAddress = LineAddress;
			}
			if (LineIdx < NumLines)
			{
				if (dwarf_lineno(LineBuf[LineIdx], &LineNumber, &ErrorInfo) != 0)
				{
					fprintf(stderr, "Can't get line number by dwarf_lineno.\n");
					break;
				}
				for (int Idx = LineIdx; Idx >= 0; --Idx)
				{
					char * SrcFileTemp = NULL;
					if (!dwarf_linesrc(LineBuf[Idx], &SrcFileTemp, &ErrorInfo))
					{
						SrcFile = SrcFileTemp;
						break;
					}
				}
				bExitHeaderLoop = true;
			}
		}

		const char * FunctionName = NULL;
		if (ReturnCode == DW_DLV_OK)
		{
			FindFunctionNameInDIEAndChildren(DebugInfo, Die, Addr, &FunctionName);
		}

		if (OutFunctionNamePtr != NULL && FunctionName != NULL)
		{
			*OutFunctionNamePtr = FunctionName;
		}

		if (OutSourceFilePtr != NULL && SrcFile != NULL)
		{
			*OutSourceFilePtr = SrcFile;
			
			if (OutLineNumberPtr != NULL)
			{
				*OutLineNumberPtr = LineNumber;
			}
		}

		// Resets internal CU pointer, so next time we get here it begins from the start
		while (ReturnCode != DW_DLV_NO_ENTRY) 
		{
			if (ReturnCode == DW_DLV_ERROR)
				break;
			ReturnCode = dwarf_next_cu_header(DebugInfo, NULL, NULL, NULL, NULL, NULL, &ErrorInfo);
		}

		// if we weren't able to find a function name, don't trust the source file either
		return FunctionName != NULL;
	}

	/**
	 * Check check if address is inside this entry.
	 */
	bool LinuxBacktraceSymbols::CheckAddressInRange(Dwarf_Debug DebugInfo, Dwarf_Die Die, Dwarf_Unsigned Addr)
	{
		Dwarf_Attribute *AttrList;
		Dwarf_Signed AttrCount;

		if (dwarf_attrlist(Die, &AttrList, &AttrCount, NULL) != DW_DLV_OK)
			return true;

		for (int i = 0; i < AttrCount; i++)
		{
			Dwarf_Half Attr;
			if (dwarf_whatattr(AttrList[i], &Attr, NULL) != DW_DLV_OK)
				continue;
			switch (Attr)
			{
				case DW_AT_low_pc:
					{
						Dwarf_Unsigned LowOffset;
						if (dwarf_formudata(AttrList[i], &LowOffset, NULL) != DW_DLV_OK)
							continue;
						if (LowOffset > Addr)
							return false;
					}
					break;
				case DW_AT_high_pc:
					{
						Dwarf_Unsigned HighOffset;
						if (dwarf_formudata(AttrList[i], &HighOffset, NULL) != DW_DLV_OK)
							continue;
						if (HighOffset <= Addr)
							return false;
					}
					break;
				case DW_AT_ranges:
					{
						Dwarf_Unsigned Offset;
						if (dwarf_formudata(AttrList[i], &Offset, NULL) != DW_DLV_OK)
							continue;

						Dwarf_Ranges *Ranges;
						Dwarf_Signed Count;
						if (dwarf_get_ranges(DebugInfo, (Dwarf_Off) Offset, &Ranges, &Count, NULL, NULL) != DW_DLV_OK)
							continue;
						for (int j = 0; j < Count; j++)
						{
							if (Ranges[j].dwr_type == DW_RANGES_END)
							{
								break;
							}
							if (Ranges[j].dwr_type == DW_RANGES_ENTRY)
							{
								if ((Ranges[j].dwr_addr1 <= Addr) && (Addr < Ranges[j].dwr_addr2))
								{
									return true;
								}
								continue;
							}
						}
						return false;
					}
					break;
				default:
					break;
			}
		}
		return true;
	}

	/**
	 * Finds a function name in DWARF DIE (Debug Information Entry).
	 * For more info on DWARF format, see http://www.dwarfstd.org/Download.php , http://www.ibm.com/developerworks/library/os-debugging/
	 *
	 * @return true if we need to stop search (i.e. either found it or some error happened)
	 */
	bool LinuxBacktraceSymbols::FindFunctionNameInDIE(Dwarf_Debug DebugInfo, Dwarf_Die Die, Dwarf_Addr Addr, const char **OutFuncName)
	{
		Dwarf_Error ErrorInfo;
		Dwarf_Half Tag;
		//Dwarf_Unsigned LowerPC, HigherPC;
		char *TempFuncName;
		int ReturnCode;

		if (dwarf_tag(Die, &Tag, &ErrorInfo) != DW_DLV_OK || Tag != DW_TAG_subprogram)
		{
			return false;
		}

		// check if address is inside this entry
		if (!CheckAddressInRange(DebugInfo, Die, Addr))
		{
			return false;
		}
		
		// found it
		*OutFuncName = NULL;
		Dwarf_Attribute SubAt;
		ReturnCode = dwarf_attr(Die, DW_AT_name, &SubAt, &ErrorInfo);
		if (ReturnCode == DW_DLV_ERROR)
		{
			return true;	// error, but stop the search
		}
		else if (ReturnCode == DW_DLV_OK) 
		{
			if (dwarf_formstring(SubAt, &TempFuncName, &ErrorInfo))
			{
				*OutFuncName = NULL;
			}
			else
			{
				*OutFuncName = TempFuncName;
			}
			return true;
		}

		// DW_AT_Name is not present, look in DW_AT_specification
		Dwarf_Attribute SpecAt;
		if (dwarf_attr(Die, DW_AT_specification, &SpecAt, &ErrorInfo))
		{
			// not found, tough luck
			return false;
		}

		Dwarf_Off Offset;
		if (dwarf_global_formref(SpecAt, &Offset, &ErrorInfo))
		{
			return false;
		}

		Dwarf_Die SpecDie;
		if (dwarf_offdie(DebugInfo, Offset, &SpecDie, &ErrorInfo))
		{
			return false;
		}

		if (dwarf_attrval_string(SpecDie, DW_AT_name, OutFuncName, &ErrorInfo))
		{
			*OutFuncName = NULL;
		}

		return true;
	}

	/**
	 * Finds a function name in DWARF DIE (Debug Information Entry) and its children.
	 * For more info on DWARF format, see http://www.dwarfstd.org/Download.php , http://www.ibm.com/developerworks/library/os-debugging/
	 * Note: that function is not exactly traversing the tree, but this "seems to work"(tm). Not sure if we need to descend properly (taking child of every sibling), this
	 * takes too much time (and callstacks seem to be fine without it).
	 */
	void LinuxBacktraceSymbols::FindFunctionNameInDIEAndChildren(Dwarf_Debug DebugInfo, Dwarf_Die Die, Dwarf_Addr Addr, const char **OutFuncName)
	{
		if (OutFuncName == NULL || *OutFuncName != NULL)
		{
			return;
		}

		// search for this Die
		if (FindFunctionNameInDIE(DebugInfo, Die, Addr, OutFuncName))
		{
			return;
		}

		Dwarf_Die PrevChild = Die, Current = NULL;
		Dwarf_Error ErrorInfo;

		int32 MaxChildrenAllowed = 32 * 1024 * 1024;	// safeguard to make sure we never get into an infinite loop
		for(;;)
		{
			if (--MaxChildrenAllowed <= 0)
			{
				fprintf(stderr, "Breaking out from what seems to be an infinite loop during DWARF parsing (too many children).\n");
				return;
			}

			// Get the child
			if (dwarf_child(PrevChild, &Current, &ErrorInfo) != DW_DLV_OK)
			{
				return;	// bail out
			}

			PrevChild = Current;

			// look for in the child
			if (FindFunctionNameInDIE(DebugInfo, Current, Addr, OutFuncName))
			{
				return;	// got the function name!
			}

			// search among child's siblings
			int32 MaxSiblingsAllowed = 64 * 1024 * 1024;	// safeguard to make sure we never get into an infinite loop
			for(;;)
			{
				if (--MaxSiblingsAllowed <= 0)
				{
					fprintf(stderr, "Breaking out from what seems to be an infinite loop during DWARF parsing (too many siblings).\n");
					break;
				}

				Dwarf_Die Prev = Current;
				if (dwarf_siblingof(DebugInfo, Prev, &Current, &ErrorInfo) != DW_DLV_OK || Current == NULL)
				{
					break;
				}

				if (FindFunctionNameInDIE(DebugInfo, Current, Addr, OutFuncName))
				{
					return;	// got the function name!
				}
			}
		};
	}

	/**
	 * Finds mangled name and returns it in internal buffer.
	 * Caller doesn't have to deallocate that.
	 */
	const char * GetMangledName(const char * SourceInfo)
	{
		static char MangledName[MaxMangledNameLength + 1];
		const char * Current = SourceInfo;

		MangledName[0] = 0;
		if (Current == NULL)
		{
			return MangledName;
		}

		// find '('
		for (; *Current != 0 && *Current != '('; ++Current);

		// if unable to find, return original
		if (*Current == 0)
		{
			return SourceInfo;
		}

		// copy everything until '+'
		++Current;
		size_t BufferIdx = 0;
		for (; *Current != 0 && *Current != '+' && BufferIdx < MaxMangledNameLength; ++Current, ++BufferIdx)
		{
			MangledName[BufferIdx] = *Current;
		}

		// if unable to find, return original
		if (*Current == 0)
		{
			return SourceInfo;
		}

		MangledName[BufferIdx] = 0;
		return MangledName;
	}

	/**
	 * Returns source filename for particular callstack depth (or NULL).
	 * Caller doesn't have to deallocate that.
	 */
	const char * GetFunctionName(FGenericCrashContext* Context, int32 CurrentCallDepth)
	{
		static char DemangledName[MaxDemangledNameLength + 1];
		if (Context == NULL || CurrentCallDepth < 0)
		{
			return NULL;
		}

		FLinuxCrashContext* LinuxContext = static_cast< FLinuxCrashContext* >( Context );

		if (LinuxContext->BacktraceSymbols == NULL)
		{
			return NULL;
		}

		const char * SourceInfo = LinuxContext->BacktraceSymbols[CurrentCallDepth];
		if (SourceInfo == NULL)
		{
			return NULL;
		}

		// see http://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html for C++ demangling
		int DemangleStatus = 0xBAD;
		char * Demangled = abi::__cxa_demangle(GetMangledName( SourceInfo ), NULL, NULL, &DemangleStatus);
		if (Demangled != NULL && DemangleStatus == 0)
		{
			FCStringAnsi::Strncpy(DemangledName, Demangled, ARRAY_COUNT(DemangledName) - 1);
		}
		else
		{
			FCStringAnsi::Strncpy(DemangledName, SourceInfo, ARRAY_COUNT(DemangledName) - 1);
		}

		if (Demangled)
		{
			free(Demangled);
		}
		return DemangledName;
	}

	void AppendToString(ANSICHAR * HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext * Context, const ANSICHAR * Text)
	{
		FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, Text);
	}

	void AppendFunctionNameIfAny(FLinuxCrashContext & LinuxContext, const char * FunctionName, uint64 ProgramCounter)
	{
		if (FunctionName)
		{
			FCStringAnsi::Strcat(LinuxContext.MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext.MinidumpCallstackInfo ) - 1, FunctionName);
			FCStringAnsi::Strcat(LinuxContext.MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext.MinidumpCallstackInfo ) - 1, " + some bytes");	// this is just to conform to crashreporterue4 standard
		}
		else
		{
			ANSICHAR TempArray[MAX_SPRINTF];

			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "0x%016llx", ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "0x%08x", (uint32)ProgramCounter);
			}
			FCStringAnsi::Strcat(LinuxContext.MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext.MinidumpCallstackInfo ) - 1, TempArray);
		}
	}

	LinuxBacktraceSymbols *GetBacktraceSymbols()
	{
		static LinuxBacktraceSymbols Symbols;
		Symbols.Init();
		return &Symbols;
	}
}

void FLinuxPlatformStackWalk::ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo )
{
	// Set the program counter.
	out_SymbolInfo.ProgramCounter = ProgramCounter;

	// Get function, filename and line number.
	// @TODO
}

bool FLinuxPlatformStackWalk::ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context )
{
	if (HumanReadableString && HumanReadableStringSize > 0)
	{
		ANSICHAR TempArray[MAX_SPRINTF];
		if (CurrentCallDepth < 0)
		{
			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "[Callstack] 0x%016llx ", ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "[Callstack] 0x%08x ", (uint32) ProgramCounter);
			}
			LinuxStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);

			// won't be able to display names here
		}
		else
		{
			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "[Callstack]  %02d  0x%016llx  ", CurrentCallDepth, ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "[Callstack]  %02d  0x%08x  ", CurrentCallDepth, (uint32) ProgramCounter);
			}
			LinuxStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);

			// Get filename.
			{
				const char * FunctionName = LinuxStackWalkHelpers::GetFunctionName(Context, CurrentCallDepth);
				if (FunctionName)
				{
					LinuxStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, FunctionName);
				}

				// try to add source file and line number, too
				FLinuxCrashContext* LinuxContext = static_cast< FLinuxCrashContext* >( Context );
				if (LinuxContext)
				{
					const char * SourceFilename = NULL;
					int LineNumber;
					if (LinuxStackWalkHelpers::GetBacktraceSymbols()->GetInfoForAddress(reinterpret_cast< void* >( ProgramCounter ), NULL, &SourceFilename, &LineNumber) && FunctionName != NULL)
					{
						FCStringAnsi::Sprintf(TempArray, " [%s, line %d]", SourceFilename, LineNumber);
						LinuxStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);
						FCStringAnsi::Sprintf(TempArray, " [%s:%d]", SourceFilename, LineNumber);

						// if we were able to find info for this line, it means it's in our code
						// append program name (strong assumption of monolithic build here!)						
						FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, TCHAR_TO_ANSI(*FApp::GetName()));
						FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, "!");
						LinuxStackWalkHelpers::AppendFunctionNameIfAny(*LinuxContext, FunctionName, ProgramCounter);
						FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, TempArray);
					}
					else
					{
						// if we were NOT able to find info for this line, it means it's something else
						FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, "Unknown!");
						LinuxStackWalkHelpers::AppendFunctionNameIfAny(*LinuxContext, FunctionName, ProgramCounter);
					}
					FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, "\r\n");	// this one always uses Windows line terminators
				}
			}
		}
		return true;
	}
	return true;
}

void FLinuxPlatformStackWalk::StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context )
{
	if ( Context == nullptr )
	{
		FLinuxCrashContext CrashContext;
		CrashContext.InitFromSignal(0, nullptr, nullptr);
		FGenericPlatformStackWalk::StackWalkAndDump(HumanReadableString, HumanReadableStringSize, IgnoreCount + 1, &CrashContext);
	}
	else
	{
		FGenericPlatformStackWalk::StackWalkAndDump(HumanReadableString, HumanReadableStringSize, IgnoreCount + 1, Context);
	}
}

void FLinuxPlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
{
	// Make sure we have place to store the information before we go through the process of raising
	// an exception and handling it.
	if( BackTrace == NULL || MaxDepth == 0 )
	{
		return;
	}

	size_t size = backtrace(reinterpret_cast< void** >( BackTrace ), MaxDepth);

	if (Context)
	{
		FLinuxCrashContext* LinuxContext = reinterpret_cast< FLinuxCrashContext* >( Context );

		if (LinuxContext->BacktraceSymbols == NULL)
		{
			// @TODO yrx 2014-09-29 Replace with backtrace_symbols_fd due to malloc()
			LinuxContext->BacktraceSymbols = backtrace_symbols(reinterpret_cast< void** >( BackTrace ), MaxDepth);
		}
	}
}

void NewReportEnsure( const TCHAR* ErrorMessage )
{
}
