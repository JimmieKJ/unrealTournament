// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#if PLATFORM_HTML5_BROWSER
#	include <emscripten.h>
#endif
#include <string.h>

static char BacktraceLog[4096];

static void ParseError(FProgramCounterSymbolInfo& out_SymbolInfo) 
{
	///Fail with some defaults
	out_SymbolInfo.LineNumber = 0;
	strcpy(out_SymbolInfo.Filename,"???");
	strcpy(out_SymbolInfo.FunctionName,"???");
}

void FHTML5PlatformStackWalk::ProgramCounterToSymbolInfo(uint64 ProgramCounter,FProgramCounterSymbolInfo& out_SymbolInfo)
{
#if PLATFORM_HTML5_BROWSER
	char* TP = (char*)ProgramCounter;
	// No module name, SymbolDisplacement, OffsetInModule or PC support
	out_SymbolInfo.ModuleName[0] = 0; 
	out_SymbolInfo.SymbolDisplacement = 0;
	out_SymbolInfo.OffsetInModule = 0;
	out_SymbolInfo.ProgramCounter = 0;
	for (int Index = 0; Index < FProgramCounterSymbolInfo::MAX_NAME_LENGHT && out_SymbolInfo.FunctionName[Index] != 0; ++Index) 
	{
		out_SymbolInfo.FunctionName[Index] = 0;
	}
	for(int Index = 0; Index < FProgramCounterSymbolInfo::MAX_NAME_LENGHT && out_SymbolInfo.Filename[Index] != 0; ++Index)
	{
		out_SymbolInfo.Filename[Index] = 0;
	}
	// Parse string to get this info out...*sad face* format is "   at  $FunctionName($FunctionParameters) ($Filepath:$LineNumber:$Column)"
	char* Ptr = strstr(TP, "at");
	if (!Ptr)
	{
		return ParseError(out_SymbolInfo);
	}
	//skip at and any whitespace.
	Ptr+=2;
	while (isspace(*Ptr))
	{
		if (*Ptr == 0)
		{
			return ParseError(out_SymbolInfo);
		}
		++Ptr;
	}
	//Now assume that any non-whitespace is function symbol name, this may include a mangled js name
	int32 Out=0;
	bool ParameterParse=false;
	while ((!isspace(*Ptr) || ParameterParse))
	{
		if(*Ptr == 0 || Out >= FProgramCounterSymbolInfo::MAX_NAME_LENGHT)
		{
			return ParseError(out_SymbolInfo);
		}
		if (*Ptr == '(') 
		{
			ParameterParse = true;
		}
		if(*Ptr == ')')
		{
			ParameterParse = false;
		}
		out_SymbolInfo.FunctionName[Out++] = *Ptr;
		++Ptr;
	}
	//skip whitespace
	while(isspace(*Ptr))
	{
		if(*Ptr == 0)
		{
			return ParseError(out_SymbolInfo);
		}
		++Ptr;
	}
	// filename is surrounded by ()
	char* Colon1 = nullptr;
	char* Colon2 = nullptr;
	Out=0;
	while(*Ptr != '\n' && *Ptr != '\r')
	{
		if(*Ptr == 0 || Out >= FProgramCounterSymbolInfo::MAX_NAME_LENGHT)
		{
			return ParseError(out_SymbolInfo);
		}
		if (*Ptr == '(' || *Ptr == ')')
		{
			++Ptr;
			continue;
		}
		if (*Ptr == ':')
		{// It's possible for the file path to contain : characters (e.g. http://) we only care about the last two.
			Colon1 = Colon2;
			Colon2 = out_SymbolInfo.Filename+Out;
		}
		out_SymbolInfo.Filename[Out++] = *Ptr;
		++Ptr;
	}
	if (Colon1 && Colon2) 
	{
		*Colon2 = 0;
		out_SymbolInfo.LineNumber = atoi(Colon1+1);
		*Colon1 = 0;
	}
#else
	return ParseError(out_SymbolInfo);
#endif
}

void FHTML5PlatformStackWalk::CaptureStackBackTrace(uint64* BackTrace,uint32 MaxDepth,void* Context)
{
#if PLATFORM_HTML5_BROWSER
	if (MaxDepth < 1) 
	{
		return;
	}

	--MaxDepth;
	emscripten_get_callstack(EM_LOG_C_STACK|EM_LOG_DEMANGLE, BacktraceLog, sizeof(BacktraceLog));

	int32 SP = 0;
	for (char* P = strchr(BacktraceLog, '\n'); P != nullptr && SP < MaxDepth; P = strchr(P+1, '\n'))
	{
		BackTrace[SP++] = (uint64)(P+1);
	}

	BackTrace[SP] = 0;
#else
	BackTrace[0] = 0;
#endif
}

int32 FHTML5PlatformStackWalk::GetStackBackTraceString(char* OutputString, int32 MaxLen)
{
#if PLATFORM_HTML5_BROWSER
	auto l = emscripten_get_callstack(EM_LOG_C_STACK|EM_LOG_DEMANGLE, OutputString, MaxLen);
	char* p = OutputString;
	while (*p && l) {
		if (*p == '\n') *p = ' ';
		p++;
	}
	return l;
#else
	return 0;
#endif
}