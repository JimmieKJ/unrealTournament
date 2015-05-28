// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DsymExporterApp.h"

#include "GenericPlatformSymbolication.h"
#if PLATFORM_MAC
#include "ApplePlatformSymbolication.h"
#endif
#include "Serialization/Archive.h"

int32 RunDsymExporter(int32 ArgC, TCHAR* Argv[])
{
	// Make sure we have at least a single parameter
	if( ArgC < 2 )
	{
		UE_LOG( LogInit, Error, TEXT( "DsymExporter - not enough parameters." ) );
		UE_LOG( LogInit, Error, TEXT( " ... usage: DsymExporter <Mach-O Binary Path> [Output Folder]" ) );
		UE_LOG( LogInit, Error, TEXT( "<Mach-O Binary Path>: This is an absolute path to a Mach-O binary containing symbols, which may be the payload binary within an application, framework or dSYM bundle, an executable or dylib." ) );
		UE_LOG( LogInit, Error, TEXT( "[Output Folder]: The folder to write the new symbol database to, the database will take the filename of the input plus the .udebugsymbols extension." ) );
		return 1;
	}
	
#if PLATFORM_MAC
	FApplePlatformSymbolication::EnableCoreSymbolication(true);
#endif
	
	FPlatformSymbolDatabase Symbols;
	FString Signature;
	bool bOK = FPlatformSymbolication::LoadSymbolDatabaseForBinary(TEXT(""), Argv[1], Signature, Symbols);
	if(bOK)
	{
		FString OutputFolder = FPaths::GetPath(Argv[1]);
		if(ArgC == 3)
		{
			OutputFolder = Argv[2];
		}
		
		if(FPlatformSymbolication::SaveSymbolDatabaseForBinary(OutputFolder, FPaths::GetBaseFilename(Argv[1]), Symbols))
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		UE_LOG( LogInit, Error, TEXT( "DsymExporter - unable to parse debug symbols for Mach-O file." ) );
		return 1;
	}
}


