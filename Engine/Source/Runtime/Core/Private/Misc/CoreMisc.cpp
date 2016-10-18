// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Core includes.
#include "CorePrivatePCH.h"
#include "ExceptionHandling.h"
#include "UniquePtr.h"

/** For FConfigFile in appInit							*/
#include "ConfigCacheIni.h"
#include "RemoteConfigIni.h"
#include "SecureHash.h"
#include "IntRect.h"

#include "ModuleManager.h"
#include "Ticker.h"
#include "DerivedDataCacheInterface.h"
#include "ITargetPlatformManagerModule.h"

DEFINE_LOG_CATEGORY(LogSHA);
DEFINE_LOG_CATEGORY(LogStats);
DEFINE_LOG_CATEGORY(LogStreaming);
DEFINE_LOG_CATEGORY(LogInit);
DEFINE_LOG_CATEGORY(LogExit);
DEFINE_LOG_CATEGORY(LogExec);
DEFINE_LOG_CATEGORY(LogScript);
DEFINE_LOG_CATEGORY(LogLocalization);
DEFINE_LOG_CATEGORY(LogLongPackageNames);
DEFINE_LOG_CATEGORY(LogProcess);
DEFINE_LOG_CATEGORY(LogLoad);
DEFINE_LOG_CATEGORY(LogCore);


/*-----------------------------------------------------------------------------
	FSelfRegisteringExec implementation.
-----------------------------------------------------------------------------*/

/** Constructor, registering this instance. */
FSelfRegisteringExec::FSelfRegisteringExec()
{
	GetRegisteredExecs().Add( this );
}

/** Destructor, unregistering this instance. */
FSelfRegisteringExec::~FSelfRegisteringExec()
{
	verify( GetRegisteredExecs().Remove( this ) == 1 );
}

bool FSelfRegisteringExec::StaticExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TArray<FSelfRegisteringExec*>& RegisteredExecs = GetRegisteredExecs();
	for(int32 ExecIndex = 0;ExecIndex < RegisteredExecs.Num();++ExecIndex)
	{
		if(RegisteredExecs[ExecIndex]->Exec( InWorld, Cmd,Ar ))
		{
			return true;
		}
	}

	return false;
}

TArray<FSelfRegisteringExec*>& FSelfRegisteringExec::GetRegisteredExecs()
{
	static TArray<FSelfRegisteringExec*>* RegisteredExecs = new TArray<FSelfRegisteringExec*>();
	return *RegisteredExecs;
}

FStaticSelfRegisteringExec::FStaticSelfRegisteringExec(bool (*InStaticExecFunc)(UWorld* Inworld, const TCHAR* Cmd,FOutputDevice& Ar))
:	StaticExecFunc(InStaticExecFunc)
{}

bool FStaticSelfRegisteringExec::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	return (*StaticExecFunc)( InWorld, Cmd,Ar);
}



/*-----------------------------------------------------------------------------
	FFileHelper
-----------------------------------------------------------------------------*/

/**
 * Load a binary file to a dynamic array.
 */
bool FFileHelper::LoadFileToArray( TArray<uint8>& Result, const TCHAR* Filename, uint32 Flags )
{
	FArchive* Reader = IFileManager::Get().CreateFileReader( Filename, Flags );
	if( !Reader )
	{
		if (!(Flags & FILEREAD_Silent))
		{
			UE_LOG(LogStreaming,Warning,TEXT("Failed to read file '%s' error."),Filename);
		}
		return 0;
	}
	int64 TotalSize = Reader->TotalSize();
	Result.Reset( TotalSize );
	Result.AddUninitialized( TotalSize );
	Reader->Serialize(Result.GetData(), Result.Num());
	bool Success = Reader->Close();
	delete Reader;
	return Success;
}

/**
 * Converts an arbitrary text buffer to an FString.
 * Supports all combination of ANSI/Unicode files and platforms.
 */
void FFileHelper::BufferToString( FString& Result, const uint8* Buffer, int32 Size )
{
	TArray<TCHAR>& ResultArray = Result.GetCharArray();
	ResultArray.Empty();

	if( Size >= 2 && !( Size & 1 ) && Buffer[0] == 0xff && Buffer[1] == 0xfe )
	{
		// Unicode Intel byte order. Less 1 for the FFFE header, additional 1 for null terminator.
		ResultArray.AddUninitialized( Size / 2 );
		for( int32 i = 0; i < ( Size / 2 ) - 1; i++ )
		{
			ResultArray[ i ] = CharCast<TCHAR>( (UCS2CHAR)(( uint16 )Buffer[i * 2 + 2] + ( uint16 )Buffer[i * 2 + 3] * 256) );
		}
	}
	else if( Size >= 2 && !( Size & 1 ) && Buffer[0] == 0xfe && Buffer[1] == 0xff )
	{
		// Unicode non-Intel byte order. Less 1 for the FFFE header, additional 1 for null terminator.
		ResultArray.AddUninitialized( Size / 2 );
		for( int32 i = 0; i < ( Size / 2 ) - 1; i++ )
		{
			ResultArray[ i ] = CharCast<TCHAR>( (UCS2CHAR)(( uint16 )Buffer[i * 2 + 3] + ( uint16 )Buffer[i * 2 + 2] * 256) );
		}
	}
	else
	{
		if ( Size >= 3 && Buffer[0] == 0xef && Buffer[1] == 0xbb && Buffer[2] == 0xbf )
		{
			// Skip over UTF-8 BOM if there is one
			Buffer += 3;
			Size   -= 3;
		}

		FUTF8ToTCHAR Conv((const ANSICHAR*)Buffer, Size);
		int32 Length = Conv.Length();
		ResultArray.AddUninitialized(Length + 1); // For the null terminator
		CopyAssignItems(ResultArray.GetData(), Conv.Get(), Length);
	}

	if (ResultArray.Num() == 1)
	{
		// If it's only a zero terminator then make the result actually empty
		ResultArray.Empty();
	}
	else
	{
		// Else ensure null terminator is present
		ResultArray.Last() = 0;
	}
}

/**
 * Load a text file to an FString.
 * Supports all combination of ANSI/Unicode files and platforms.
 * @param Result string representation of the loaded file
 * @param Filename name of the file to load
 * @param VerifyFlags flags controlling the hash verification behavior ( see EHashOptions )
 */
bool FFileHelper::LoadFileToString( FString& Result, const TCHAR* Filename, uint32 VerifyFlags )
{
	TUniquePtr<FArchive> Reader( IFileManager::Get().CreateFileReader( Filename ) );
	if( !Reader )
	{
		return 0;
	}
	
	int32 Size = Reader->TotalSize();
	if( !Size )
	{
		Result.Empty();
		return true;
	}

	uint8* Ch = (uint8*)FMemory::Malloc(Size);
	Reader->Serialize( Ch, Size );
	bool Success = Reader->Close();
	Reader = nullptr;
	BufferToString( Result, Ch, Size );

	// handle SHA verify of the file
	if( (VerifyFlags & EHashOptions::EnableVerify) && ( (VerifyFlags & EHashOptions::ErrorMissingHash) || FSHA1::GetFileSHAHash(Filename, NULL) ) )
	{
		// kick off SHA verify task. this frees the buffer on close
		FBufferReaderWithSHA Ar( Ch, Size, true, Filename, false, true );
	}
	else
	{
		// free manually since not running SHA task
		FMemory::Free(Ch);
	}

	return Success;
}

/**
 * Save a binary array to a file.
 */
bool FFileHelper::SaveArrayToFile(TArrayView<const uint8> Array, const TCHAR* Filename, IFileManager* FileManager /*= &IFileManager::Get()*/, uint32 WriteFlags)
{
	FArchive* Ar = FileManager->CreateFileWriter( Filename, WriteFlags );
	if( !Ar )
	{
		return 0;
	}
	Ar->Serialize(const_cast<uint8*>(Array.GetData()), Array.Num());
	delete Ar;
	return true;
}

/**
 * Write the FString to a file.
 * Supports all combination of ANSI/Unicode files and platforms.
 */
bool FFileHelper::SaveStringToFile( const FString& String, const TCHAR* Filename,  EEncodingOptions::Type EncodingOptions, IFileManager* FileManager /*= &IFileManager::Get()*/, uint32 WriteFlags )
{
	// max size of the string is a UCS2CHAR for each character and some UNICODE magic 
	auto Ar = TUniquePtr<FArchive>( FileManager->CreateFileWriter( Filename, WriteFlags ) );
	if( !Ar )
		return false;

	if( String.IsEmpty() )
		return true;

	const TCHAR* StrPtr = *String;

	bool SaveAsUnicode = EncodingOptions == EEncodingOptions::ForceUnicode || ( EncodingOptions == EEncodingOptions::AutoDetect && !FCString::IsPureAnsi(StrPtr) );
	if( EncodingOptions == EEncodingOptions::ForceUTF8 )
	{
		UTF8CHAR UTF8BOM[] = { 0xEF, 0xBB, 0xBF };
		Ar->Serialize( &UTF8BOM, ARRAY_COUNT(UTF8BOM) * sizeof(UTF8CHAR) );

		FTCHARToUTF8 UTF8String(StrPtr);
		Ar->Serialize( (UTF8CHAR*)UTF8String.Get(), UTF8String.Length() * sizeof(UTF8CHAR) );
	}
	else if ( EncodingOptions == EEncodingOptions::ForceUTF8WithoutBOM )
	{
		FTCHARToUTF8 UTF8String(StrPtr);
		Ar->Serialize((UTF8CHAR*)UTF8String.Get(), UTF8String.Length() * sizeof(UTF8CHAR));
	}
	else if (SaveAsUnicode)
	{
		UCS2CHAR BOM = UNICODE_BOM;
		Ar->Serialize( &BOM, sizeof(UCS2CHAR) );

		auto Src = StringCast<UCS2CHAR>(StrPtr, String.Len());
		Ar->Serialize( (UCS2CHAR*)Src.Get(), Src.Length() * sizeof(UCS2CHAR) );
	}
	else
	{
		auto Src = StringCast<ANSICHAR>(StrPtr, String.Len());
		Ar->Serialize( (ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR) );
	}

	return true;
}

/**
 * Generates the next unique bitmap filename with a specified extension
 * 
 * @param Pattern		Filename with path, but without extension.
 * @oaran Extension		File extension to be appended
 * @param OutFilename	Reference to an FString where the newly generated filename will be placed
 * @param FileManager	Reference to a IFileManager (or the global instance by default)
 *
 * @return true if success
 */
bool FFileHelper::GenerateNextBitmapFilename( const FString& Pattern, const FString& Extension, FString& OutFilename, IFileManager* FileManager /*= &IFileManager::Get()*/ )
{
	FString File;
	OutFilename = "";
	bool bSuccess = false;

	for( int32 TestBitmapIndex = GScreenshotBitmapIndex + 1; TestBitmapIndex < 100000; ++TestBitmapIndex )
	{
		File = FString::Printf(TEXT("%s%05i.%s"), *Pattern, TestBitmapIndex, *Extension);
		if( FileManager->FileSize(*File) < 0 )
		{
			GScreenshotBitmapIndex = TestBitmapIndex;
			OutFilename = File;
			bSuccess = true;
			break;
		}
	}

	return bSuccess;
}

/**
 * Saves a 24Bit BMP file to disk
 * 
 * @param Pattern filename with path, must not be 0, if with "bmp" extension (e.g. "out.bmp") the filename stays like this, if without (e.g. "out") automatic index numbers are addended (e.g. "out00002.bmp")
 * @param DataWidth - Width of the bitmap supplied in Data >0
 * @param DataHeight - Height of the bitmap supplied in Data >0
 * @param Data must not be 0
 * @param SubRectangle optional, specifies a sub-rectangle of the source image to save out. If NULL, the whole bitmap is saved
 * @param FileManager must not be 0
 * @param OutFilename optional, if specified filename will be output
 *
 * @return true if success
 */
bool FFileHelper::CreateBitmap( const TCHAR* Pattern, int32 SourceWidth, int32 SourceHeight, const FColor* Data, struct FIntRect* SubRectangle, IFileManager* FileManager /*= &IFileManager::Get()*/, FString* OutFilename /*= NULL*/, bool bInWriteAlpha /*= false*/ )
{
#if ALLOW_DEBUG_FILES
	FIntRect Src(0, 0, SourceWidth, SourceHeight);
	if (SubRectangle == NULL || SubRectangle->Area() == 0)
	{
		SubRectangle = &Src;
	}

	FString File;
	// if the Pattern already has a .bmp extension, then use that the file to write to
	if (FPaths::GetExtension(Pattern) == TEXT("bmp"))
	{
		File = Pattern;
	}
	else
	{
		if (GenerateNextBitmapFilename(Pattern, TEXT("bmp"), File, FileManager))
		{
			if ( OutFilename )
			{
				*OutFilename = File;
			}
		}
		else
		{
			return false;
		}
	}

	FArchive* Ar = FileManager->CreateDebugFileWriter( *File );
	if( Ar )
	{
		// Types.
		#if PLATFORM_SUPPORTS_PRAGMA_PACK
			#pragma pack (push,1)
		#endif
		struct BITMAPFILEHEADER
		{
			uint16 bfType GCC_PACK(1);
			uint32 bfSize GCC_PACK(1);
			uint16 bfReserved1 GCC_PACK(1); 
			uint16 bfReserved2 GCC_PACK(1);
			uint32 bfOffBits GCC_PACK(1);
		} FH; 
		struct BITMAPINFOHEADER
		{
			uint32 biSize GCC_PACK(1); 
			int32  biWidth GCC_PACK(1);
			int32  biHeight GCC_PACK(1);
			uint16 biPlanes GCC_PACK(1);
			uint16 biBitCount GCC_PACK(1);
			uint32 biCompression GCC_PACK(1);
			uint32 biSizeImage GCC_PACK(1);
			int32  biXPelsPerMeter GCC_PACK(1); 
			int32  biYPelsPerMeter GCC_PACK(1);
			uint32 biClrUsed GCC_PACK(1);
			uint32 biClrImportant GCC_PACK(1); 
		} IH;
		struct BITMAPV4HEADER
		{
			uint32 bV4RedMask GCC_PACK(1);
			uint32 bV4GreenMask GCC_PACK(1);
			uint32 bV4BlueMask GCC_PACK(1);
			uint32 bV4AlphaMask GCC_PACK(1);
			uint32 bV4CSType GCC_PACK(1);
			uint32 bV4EndpointR[3] GCC_PACK(1);
			uint32 bV4EndpointG[3] GCC_PACK(1);
			uint32 bV4EndpointB[3] GCC_PACK(1);
			uint32 bV4GammaRed GCC_PACK(1);
			uint32 bV4GammaGreen GCC_PACK(1);
			uint32 bV4GammaBlue GCC_PACK(1);
		} IHV4;
		#if PLATFORM_SUPPORTS_PRAGMA_PACK
			#pragma pack (pop)
		#endif

		int32 Width = SubRectangle->Width();
		int32 Height = SubRectangle->Height();
		uint32 BytesPerPixel = bInWriteAlpha ? 4 : 3;
		uint32 BytesPerLine = Align(Width * BytesPerPixel, 4);

		uint32 InfoHeaderSize = sizeof(BITMAPINFOHEADER) + (bInWriteAlpha ? sizeof(BITMAPV4HEADER) : 0);

		// File header.
		FH.bfType       		= INTEL_ORDER16((uint16) ('B' + 256*'M'));
		FH.bfSize       		= INTEL_ORDER32((uint32) (sizeof(BITMAPFILEHEADER) + InfoHeaderSize + BytesPerLine * Height));
		FH.bfReserved1  		= INTEL_ORDER16((uint16) 0);
		FH.bfReserved2  		= INTEL_ORDER16((uint16) 0);
		FH.bfOffBits    		= INTEL_ORDER32((uint32) (sizeof(BITMAPFILEHEADER) + InfoHeaderSize));
		Ar->Serialize( &FH, sizeof(FH) );

		// Info header.
		IH.biSize               = INTEL_ORDER32((uint32) InfoHeaderSize);
		IH.biWidth              = INTEL_ORDER32((uint32) Width);
		IH.biHeight             = INTEL_ORDER32((uint32) Height);
		IH.biPlanes             = INTEL_ORDER16((uint16) 1);
		IH.biBitCount           = INTEL_ORDER16((uint16) BytesPerPixel * 8);
		if(bInWriteAlpha)
		{
			IH.biCompression    = INTEL_ORDER32((uint32) 3); //BI_BITFIELDS
		}
		else
		{
			IH.biCompression    = INTEL_ORDER32((uint32) 0); //BI_RGB
		}
		IH.biSizeImage          = INTEL_ORDER32((uint32) BytesPerLine * Height);
		IH.biXPelsPerMeter      = INTEL_ORDER32((uint32) 0);
		IH.biYPelsPerMeter      = INTEL_ORDER32((uint32) 0);
		IH.biClrUsed            = INTEL_ORDER32((uint32) 0);
		IH.biClrImportant       = INTEL_ORDER32((uint32) 0);
		Ar->Serialize( &IH, sizeof(IH) );

		// If we're writing alpha, we need to write the extra portion of the V4 header
		if (bInWriteAlpha)
		{
			IHV4.bV4RedMask     = INTEL_ORDER32((uint32) 0x00ff0000);
			IHV4.bV4GreenMask   = INTEL_ORDER32((uint32) 0x0000ff00);
			IHV4.bV4BlueMask    = INTEL_ORDER32((uint32) 0x000000ff);
			IHV4.bV4AlphaMask   = INTEL_ORDER32((uint32) 0xff000000);
			IHV4.bV4CSType      = INTEL_ORDER32((uint32) 'Win ');
			IHV4.bV4GammaRed    = INTEL_ORDER32((uint32) 0);
			IHV4.bV4GammaGreen  = INTEL_ORDER32((uint32) 0);
			IHV4.bV4GammaBlue   = INTEL_ORDER32((uint32) 0);
			Ar->Serialize( &IHV4, sizeof(IHV4) );
		}

		// Colors.
		for( int32 i = SubRectangle->Max.Y - 1; i >= SubRectangle->Min.Y; i-- )
		{
			for( int32 j = SubRectangle->Min.X; j < SubRectangle->Max.X; j++ )
			{
				Ar->Serialize( (void *)&Data[i*SourceWidth+j].B, 1 );
				Ar->Serialize( (void *)&Data[i*SourceWidth+j].G, 1 );
				Ar->Serialize( (void *)&Data[i*SourceWidth+j].R, 1 );

				if (bInWriteAlpha)
				{
					Ar->Serialize( (void *)&Data[i * SourceWidth + j].A, 1 );
				}
			}

			// Pad each row's length to be a multiple of 4 bytes.

			for(uint32 PadIndex = Width * BytesPerPixel; PadIndex < BytesPerLine; PadIndex++)
			{
				uint8 B = 0;
				Ar->Serialize(&B, 1);
			}
		}

		// Success.
		delete Ar;
		if (!GIsEditor)
		{
			SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!BUGIT:"), File );
		}
	}
	else 
	{
		return 0;
	}
#endif
	// Success.
	return 1;
}

/**
 *	Load the given ANSI text file to an array of strings - one FString per line of the file.
 *	Intended for use in simple text parsing actions
 *
 *	@param	InFilename			The text file to read, full path
 *	@param	InFileManager		The filemanager to use - NULL will use &IFileManager::Get()
 *	@param	OutStrings			The array of FStrings to fill in
 *
 *	@return	bool				true if successful, false if not
 */
bool FFileHelper::LoadANSITextFileToStrings(const TCHAR* InFilename, IFileManager* InFileManager, TArray<FString>& OutStrings)
{
	IFileManager* FileManager = (InFileManager != NULL) ? InFileManager : &IFileManager::Get();
	// Read and parse the file, adding the pawns and their sounds to the list
	FArchive* TextFile = FileManager->CreateFileReader(InFilename, 0);
	if (TextFile != NULL)
	{
		// get the size of the file
		int32 Size = TextFile->TotalSize();
		// read the file
		TArray<uint8> Buffer;
		Buffer.Empty(Size);
		Buffer.AddUninitialized(Size);
		TextFile->Serialize(Buffer.GetData(), Size);
		// zero terminate it
		Buffer.Add(0);
		// Release the file
		delete TextFile;

		// Now read it
		// init traveling pointer
		ANSICHAR* Ptr = (ANSICHAR*)Buffer.GetData();

		// iterate over the lines until complete
		bool bIsDone = false;
		while (!bIsDone)
		{
			// Store the location of the first character of this line
			ANSICHAR* Start = Ptr;

			// Advance the char pointer until we hit a newline character
			while (*Ptr && *Ptr!='\r' && *Ptr!='\n')
			{
				Ptr++;
			}

			// If this is the end of the file, we're done
			if (*Ptr == 0)
			{
				bIsDone = 1;
			}
			// Handle different line endings. If \r\n then NULL and advance 2, otherwise NULL and advance 1
			// This handles \r, \n, or \r\n
			else if ( *Ptr=='\r' && *(Ptr+1)=='\n' )
			{
				// This was \r\n. Terminate the current line, and advance the pointer forward 2 characters in the stream
				*Ptr++ = 0;
				*Ptr++ = 0;
			}
			else
			{
				// Terminate the current line, and advance the pointer to the next character in the stream
				*Ptr++ = 0;
			}

			FString CurrLine = ANSI_TO_TCHAR(Start);
			OutStrings.Add(CurrLine);
		}

		return true;
	}
	else
	{
		UE_LOG(LogStreaming, Warning, TEXT("Failed to open ANSI TEXT file %s"), InFilename);
		return false;
	}
}

/*-----------------------------------------------------------------------------
	FCommandLine
-----------------------------------------------------------------------------*/

bool FCommandLine::bIsInitialized = false;
TCHAR FCommandLine::CmdLine[FCommandLine::MaxCommandLineSize] = TEXT("");
TCHAR FCommandLine::OriginalCmdLine[FCommandLine::MaxCommandLineSize] = TEXT("");
TCHAR FCommandLine::LoggingCmdLine[FCommandLine::MaxCommandLineSize] = TEXT("");
TCHAR FCommandLine::LoggingOriginalCmdLine[FCommandLine::MaxCommandLineSize] = TEXT("");
FString FCommandLine::SubprocessCommandLine(TEXT(" -Multiprocess"));

bool FCommandLine::IsInitialized()
{
	return bIsInitialized;
}

const TCHAR* FCommandLine::Get()
{
	UE_CLOG(!bIsInitialized, LogInit, Fatal, TEXT("Attempting to get the command line but it hasn't been initialized yet."));
	return CmdLine;
}

const TCHAR* FCommandLine::GetForLogging()
{
	UE_CLOG(!bIsInitialized, LogInit, Fatal, TEXT("Attempting to get the command line but it hasn't been initialized yet."));
	return LoggingCmdLine;
}

const TCHAR* FCommandLine::GetOriginal()
{
	UE_CLOG(!bIsInitialized, LogInit, Fatal, TEXT("Attempting to get the command line but it hasn't been initialized yet."));
	return OriginalCmdLine;
}

const TCHAR* FCommandLine::GetOriginalForLogging()
{
	UE_CLOG(!bIsInitialized, LogInit, Fatal, TEXT("Attempting to get the command line but it hasn't been initialized yet."));
	return LoggingOriginalCmdLine;
}

bool FCommandLine::Set(const TCHAR* NewCommandLine)
{
	if (!bIsInitialized)
	{
		FCString::Strncpy(OriginalCmdLine, NewCommandLine, ARRAY_COUNT(OriginalCmdLine));
		FCString::Strncpy(LoggingOriginalCmdLine, NewCommandLine, ARRAY_COUNT(LoggingOriginalCmdLine));
	}

	FCString::Strncpy( CmdLine, NewCommandLine, ARRAY_COUNT(CmdLine) );
	FCString::Strncpy(LoggingCmdLine, NewCommandLine, ARRAY_COUNT(LoggingCmdLine));
	// If configured as part of the build, strip out any unapproved args
	WhitelistCommandLines();

	bIsInitialized = true;

	// Check for the '-' that normal ones get converted to in Outlook. It's important to do it AFTER the command line is initialized
	if (StringHasBadDashes(NewCommandLine))
	{
		FText ErrorMessage = FText::Format(NSLOCTEXT("Engine", "ComdLineHasInvalidChar", "Error: Command-line contains an invalid '-' character, likely pasted from an email.\nCmdline = {0}"), FText::FromString(NewCommandLine));
#if !UE_BUILD_SHIPPING
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return false;
#else
		UE_LOG(LogInit, Fatal, TEXT("%s"), *ErrorMessage.ToString());
#endif
	}

	return true;
}

void FCommandLine::Append(const TCHAR* AppendString)
{
	FCString::Strncat( CmdLine, AppendString, ARRAY_COUNT(CmdLine) );
	// If configured as part of the build, strip out any unapproved args
	WhitelistCommandLines();
}

#if WANTS_COMMANDLINE_WHITELIST
TArray<FString> FCommandLine::ApprovedArgs;
TArray<FString> FCommandLine::FilterArgsForLogging;

#ifdef OVERRIDE_COMMANDLINE_WHITELIST
/**
 * When overriding this setting make sure that your define looks like the following in your .cs file:
 *
 *		OutCPPEnvironmentConfiguration.Definitions.Add("OVERRIDE_COMMANDLINE_WHITELIST=\"-arg1 -arg2 -arg3 -arg4\"");
 *
 * The important part is the \" as they quotes get stripped off by the compiler without them
 */
const TCHAR* OverrideList = TEXT(OVERRIDE_COMMANDLINE_WHITELIST);
#else
// Default list most conservative restrictions
const TCHAR* OverrideList = TEXT("-fullscreen /windowed");
#endif

#ifdef FILTER_COMMANDLINE_LOGGING
/**
 * When overriding this setting make sure that your define looks like the following in your .cs file:
 *
 *		OutCPPEnvironmentConfiguration.Definitions.Add("FILTER_COMMANDLINE_LOGGING=\"-arg1 -arg2 -arg3 -arg4\"");
 *
 * The important part is the \" as they quotes get stripped off by the compiler without them
 */
const TCHAR* FilterForLoggingList = TEXT(FILTER_COMMANDLINE_LOGGING);
#else
const TCHAR* FilterForLoggingList = TEXT("");
#endif

void FCommandLine::WhitelistCommandLines()
{
	if (ApprovedArgs.Num() == 0)
	{
		TArray<FString> Ignored;
		FCommandLine::Parse(OverrideList, ApprovedArgs, Ignored);
	}
	if (FilterArgsForLogging.Num() == 0)
	{
		TArray<FString> Ignored;
		FCommandLine::Parse(FilterForLoggingList, FilterArgsForLogging, Ignored);
	}
	// Process the original command line
	TArray<FString> OriginalList = FilterCommandLine(OriginalCmdLine);
	BuildWhitelistCommandLine(OriginalCmdLine, ARRAY_COUNT(OriginalCmdLine), OriginalList);
	// Process the current command line
	TArray<FString> CmdList = FilterCommandLine(CmdLine);
	BuildWhitelistCommandLine(CmdLine, ARRAY_COUNT(CmdLine), CmdList);
	// Process the command line for logging purposes
	TArray<FString> LoggingCmdList = FilterCommandLineForLogging(LoggingCmdLine);
	BuildWhitelistCommandLine(LoggingCmdLine, ARRAY_COUNT(LoggingCmdLine), LoggingCmdList);
	// Process the original command line for logging purposes
	TArray<FString> LoggingOriginalCmdList = FilterCommandLineForLogging(LoggingOriginalCmdLine);
	BuildWhitelistCommandLine(LoggingOriginalCmdLine, ARRAY_COUNT(LoggingOriginalCmdLine), LoggingOriginalCmdList);
}

TArray<FString> FCommandLine::FilterCommandLine(TCHAR* CommandLine)
{
	TArray<FString> Ignored;
	TArray<FString> ParsedList;
	// Parse the command line list
	FCommandLine::Parse(CommandLine, ParsedList, Ignored);
	// Remove any that are not in our approved list
	for (int32 Index = 0; Index < ParsedList.Num(); Index++)
	{
		bool bFound = false;
		for (auto ApprovedArg : ApprovedArgs)
		{
			if (ParsedList[Index].StartsWith(ApprovedArg))
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			ParsedList.RemoveAt(Index);
			Index--;
		}
	}
	return ParsedList;
}

TArray<FString> FCommandLine::FilterCommandLineForLogging(TCHAR* CommandLine)
{
	TArray<FString> Ignored;
	TArray<FString> ParsedList;
	// Parse the command line list
	FCommandLine::Parse(CommandLine, ParsedList, Ignored);
	// Remove any that are not in our approved list
	for (int32 Index = 0; Index < ParsedList.Num(); Index++)
	{
		for (auto Filter : FilterArgsForLogging)
		{
			if (ParsedList[Index].StartsWith(Filter))
			{
				ParsedList.RemoveAt(Index);
				Index--;
				break;
			}
		}
	}
	return ParsedList;
}

void FCommandLine::BuildWhitelistCommandLine(TCHAR* CommandLine, uint32 ArrayCount, const TArray<FString>& FilteredArgs)
{
	check(ArrayCount > 0);
	// Zero the whole string
	FMemory::Memzero(CommandLine, sizeof(TCHAR) * ArrayCount);

	uint32 StartIndex = 0;
	for (auto Arg : FilteredArgs)
	{
		if ((StartIndex + Arg.Len() + 2) < ArrayCount)
		{
			if (StartIndex != 0)
			{
				CommandLine[StartIndex++] = TEXT(' ');
			}
			CommandLine[StartIndex++] = TEXT('-');
			FCString::Strncpy(&CommandLine[StartIndex], *Arg, ArrayCount - StartIndex);
			StartIndex += Arg.Len();
		}
	}
}
#endif

void FCommandLine::AddToSubprocessCommandline( const TCHAR* Param )
{
	check( Param != NULL );
	if (Param[0] != ' ')
	{
		SubprocessCommandLine += TEXT(" ");
	}
	SubprocessCommandLine += Param;
}

const FString& FCommandLine::GetSubprocessCommandline()
{
	return SubprocessCommandLine;
}

/** 
 * Removes the executable name from a commandline, denoted by parentheses.
 */
const TCHAR* FCommandLine::RemoveExeName(const TCHAR* InCmdLine)
{
	// Skip over executable that is in the commandline
	if( *InCmdLine=='\"' )
	{
		InCmdLine++;
		while( *InCmdLine && *InCmdLine!='\"' )
		{
			InCmdLine++;
		}
		if( *InCmdLine )
		{
			InCmdLine++;
		}
	}
	while( *InCmdLine && *InCmdLine!=' ' )
	{
		InCmdLine++;
	}
	// skip over any spaces at the start, which Vista likes to toss in multiple
	while (*InCmdLine == ' ')
	{
		InCmdLine++;
	}
	return InCmdLine;
}


/**
 * Parses a string into tokens, separating switches (beginning with - or /) from
 * other parameters
 *
 * @param	CmdLine		the string to parse
 * @param	Tokens		[out] filled with all parameters found in the string
 * @param	Switches	[out] filled with all switches found in the string
 */
void FCommandLine::Parse(const TCHAR* InCmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	FString NextToken;
	while (FParse::Token(InCmdLine, NextToken, false))
	{
		if ((**NextToken == TCHAR('-')) || (**NextToken == TCHAR('/')))
		{
			new(Switches) FString(NextToken.Mid(1));
			new(Tokens) FString(NextToken.Right(NextToken.Len() - 1));
		}
		else
		{
			new(Tokens) FString(NextToken);
		}
	}
}


/*-----------------------------------------------------------------------------
	FMaintenance
-----------------------------------------------------------------------------*/
void FMaintenance::DeleteOldLogs()
{
	int32 PurgeLogsDays = -1; // -1 means don't delete old files
	int32 MaxLogFilesOnDisk = -1; // -1 means keep all files
	GConfig->GetInt(TEXT("LogFiles"), TEXT("PurgeLogsDays"), PurgeLogsDays, GEngineIni);
	GConfig->GetInt(TEXT("LogFiles"), TEXT("MaxLogFilesOnDisk"), MaxLogFilesOnDisk, GEngineIni);
	if (PurgeLogsDays >= 0 || MaxLogFilesOnDisk >= 0)
	{
		// get a list of files in the log dir
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FString::Printf(TEXT("%s*.*"), *FPaths::GameLogDir()), true, false);
		for (FString& Filename : Files)
		{
			Filename = FPaths::GameLogDir() / Filename;
		}

		struct FSortByDateNewestFirst
		{
			bool operator()(const FString& A, const FString& B) const
			{
				const FDateTime TimestampA = IFileManager::Get().GetTimeStamp(*A);
				const FDateTime TimestampB = IFileManager::Get().GetTimeStamp(*B);
				return TimestampB < TimestampA;
			}
		};
		Files.Sort(FSortByDateNewestFirst());

		// delete all those with the backup text in their name and that are older than the specified number of days
		double MaxFileAgeSeconds = 60.0 * 60.0 * 24.0 * double(PurgeLogsDays);
		for (int32 FileIndex = Files.Num() - 1; FileIndex >= 0; --FileIndex)
		{
			const FString& Filename = Files[FileIndex];
			if (FOutputDeviceFile::IsBackupCopy(*Filename) && IFileManager::Get().GetFileAgeSeconds(*Filename) > MaxFileAgeSeconds)
			{
				UE_LOG(LogStreaming, Log, TEXT("Deleting old log file %s"), *Filename);
				IFileManager::Get().Delete(*Filename);
				Files.RemoveAt(FileIndex);
			}
		}

		// If required, trim the number of files on disk
		if (MaxLogFilesOnDisk >= 0 && Files.Num() > MaxLogFilesOnDisk)
		{
			for (int32 FileIndex = Files.Num() - 1; FileIndex >= 0 && Files.Num() > MaxLogFilesOnDisk; --FileIndex)
			{
				if (FOutputDeviceFile::IsBackupCopy(*Files[FileIndex]))
				{
					IFileManager::Get().Delete(*Files[FileIndex]);
					Files.RemoveAt(FileIndex);
				}
			}
		}

		// Remove old UE4 crash contexts
		TArray<FString> Directories;
		IFileManager::Get().FindFiles( Directories, *FString::Printf( TEXT( "%s/UE4CC*" ), *FPaths::GameLogDir() ), false, true );

		for (const FString& Dir : Directories)
		{
			const FString CrashContextDirectory = FPaths::GameLogDir() / Dir;
			const FDateTime DirectoryAccessTime = IFileManager::Get().GetTimeStamp( *CrashContextDirectory );
			if (FDateTime::Now() - DirectoryAccessTime > FTimespan::FromDays( PurgeLogsDays ))
			{
				UE_LOG( LogStreaming, Log, TEXT( "Deleting old crash context %s" ), *Dir );
				IFileManager::Get().DeleteDirectory( *CrashContextDirectory, false, true );
			}
		}
	}
}


/*-----------------------------------------------------------------------------
	Module singletons.
-----------------------------------------------------------------------------*/

class FDerivedDataCacheInterface* GetDerivedDataCache()
{
	static class FDerivedDataCacheInterface* SingletonInterface = NULL;
	if (!FPlatformProperties::RequiresCookedData())
	{
		static bool bInitialized = false;
		if (!bInitialized)
		{
			check(IsInGameThread());
			bInitialized = true;
			class IDerivedDataCacheModule* Module = FModuleManager::LoadModulePtr<IDerivedDataCacheModule>("DerivedDataCache");
			if (Module)
			{
				SingletonInterface = &Module->GetDDC();
			}
		}
	}
	return SingletonInterface;
}

class FDerivedDataCacheInterface& GetDerivedDataCacheRef()
{
	class FDerivedDataCacheInterface* SingletonInterface = GetDerivedDataCache();
	if (!SingletonInterface)
	{
		UE_LOG(LogInit, Fatal, TEXT("Derived Data Cache was requested, but not available."));
		CA_ASSUME( SingletonInterface != NULL );	// Suppress static analysis warning in unreachable code (fatal error)
	}
	return *SingletonInterface;
}

class ITargetPlatformManagerModule* GetTargetPlatformManager()
{
	static class ITargetPlatformManagerModule* SingletonInterface = NULL;
	if (!FPlatformProperties::RequiresCookedData())
	{
		static bool bInitialized = false;
		if (!bInitialized)
		{
			check(IsInGameThread());
			bInitialized = true;
			SingletonInterface = FModuleManager::LoadModulePtr<ITargetPlatformManagerModule>("TargetPlatform");
		}
	}
	return SingletonInterface;
}

class ITargetPlatformManagerModule& GetTargetPlatformManagerRef()
{
	class ITargetPlatformManagerModule* SingletonInterface = GetTargetPlatformManager();
	if (!SingletonInterface)
	{
		UE_LOG(LogInit, Fatal, TEXT("Target platform manager was requested, but not available."));
		CA_ASSUME( SingletonInterface != NULL );	// Suppress static analysis warning in unreachable code (fatal error)
	}
	return *SingletonInterface;
}

//-----------------------------------------------------------------------------


FTicker& FTicker::GetCoreTicker()
{
	static FTicker Singleton;
	return Singleton;
}


/*----------------------------------------------------------------------------
	Runtime functions.
----------------------------------------------------------------------------*/

FQueryIsRunningServer GIsServerDelegate;

bool IsServerForOnlineSubsystems(FName WorldContextHandle)
{
	if (GIsServerDelegate.IsBound())
	{
		return GIsServerDelegate.Execute(WorldContextHandle);
	}
	else
	{
		return IsRunningDedicatedServer();
	}
}

void SetIsServerForOnlineSubsystemsDelegate(FQueryIsRunningServer NewDelegate)
{
	GIsServerDelegate = NewDelegate;
}

#if UE_EDITOR

/** Checks the command line for the presence of switches to indicate running as "dedicated server only" */
int32 CORE_API StaticDedicatedServerCheck()
{
	static int32 HasServerSwitch = -1;
	if (HasServerSwitch == -1)
	{
		const FString CmdLine = FString(FCommandLine::Get()).Trim();
		const TCHAR* TCmdLine = *CmdLine;

		TArray<FString> Tokens;
		TArray<FString> Switches;
		FCommandLine::Parse(TCmdLine, Tokens, Switches);

		HasServerSwitch = (Switches.Contains(TEXT("SERVER")) || Switches.Contains(TEXT("RUN=SERVER"))) ? 1 : 0;
	}
	return HasServerSwitch;
}

/** Checks the command line for the presence of switches to indicate running as "game only" */
int32 CORE_API StaticGameCheck()
{
	static int32 HasGameSwitch = -1;
	if (HasGameSwitch == -1)
	{
		const FString CmdLine = FString(FCommandLine::Get()).Trim();
		const TCHAR* TCmdLine = *CmdLine;

		TArray<FString> Tokens;
		TArray<FString> Switches;
		FCommandLine::Parse(TCmdLine, Tokens, Switches);

		if (Switches.Contains(TEXT("GAME")))
		{
			HasGameSwitch = 1;
		}
		else
		{
			HasGameSwitch = 0;
		}
	}
	return HasGameSwitch;
}

/** Checks the command line for the presence of switches to indicate running as "client only" */
int32 CORE_API StaticClientOnlyCheck()
{
	static int32 HasClientSwitch = -1;
	if (HasClientSwitch == -1)
	{
		const FString CmdLine = FString(FCommandLine::Get()).Trim();
		const TCHAR* TCmdLine = *CmdLine;

		TArray<FString> Tokens;
		TArray<FString> Switches;
		FCommandLine::Parse(TCmdLine, Tokens, Switches);

		HasClientSwitch = (StaticGameCheck() && Switches.Contains(TEXT("ClientOnly"))) ? 1 : 0;
	}
	return HasClientSwitch;
}

#endif //UE_EDITOR

void FUrlConfig::Init()
{
	DefaultProtocol = GConfig->GetStr( TEXT("URL"), TEXT("Protocol"), GEngineIni );
	DefaultName = GConfig->GetStr( TEXT("URL"), TEXT("Name"), GEngineIni );
	// strip off any file extensions from map names
	DefaultHost = GConfig->GetStr( TEXT("URL"), TEXT("Host"), GEngineIni );
	DefaultPortal = GConfig->GetStr( TEXT("URL"), TEXT("Portal"), GEngineIni );
	DefaultSaveExt = GConfig->GetStr( TEXT("URL"), TEXT("SaveExt"), GEngineIni );
	
	FString Port;
	// Allow the command line to override the default port
	if (FParse::Value(FCommandLine::Get(),TEXT("Port="),Port) == false)
	{
		Port = GConfig->GetStr( TEXT("URL"), TEXT("Port"), GEngineIni );
	}
	DefaultPort = FCString::Atoi( *Port );
}

void FUrlConfig::Reset()
{
	DefaultProtocol = TEXT("");
	DefaultName = TEXT("");
	DefaultHost = TEXT("");
	DefaultPortal = TEXT("");
	DefaultSaveExt = TEXT("");
}

bool CORE_API StringHasBadDashes(const TCHAR* Str)
{
	// Detect dashes (0x2013) and report an error if they're found
	while (TCHAR Ch = *Str++)
	{
		if ((UCS2CHAR)Ch == 0x2013)
			return true;
	}

	return false;
}

void GenerateConvenientWindowedResolutions(const FDisplayMetrics& InDisplayMetrics, TArray<FIntPoint>& OutResolutions)
{
	bool bInPortraitMode = InDisplayMetrics.PrimaryDisplayWidth < InDisplayMetrics.PrimaryDisplayHeight;

	// Generate windowed resolutions as scaled versions of primary monitor size
	static const float Scales[] = { 3.0f / 6.0f, 4.0f / 6.0f, 4.5f / 6.0f, 5.0f / 6.0f };
	static const float Ratios[] = { 9.0f, 10.0f, 12.0f };
	static const float MinWidth = 1280.0f;
	static const float MinHeight = 720.0f; // UI layout doesn't work well below this, as the accept/cancel buttons go off the bottom of the screen

	static const uint32 NumScales = sizeof(Scales) / sizeof(float);
	static const uint32 NumRatios = sizeof(Ratios) / sizeof(float);

	for (uint32 ScaleIndex = 0; ScaleIndex < NumScales; ++ScaleIndex)
	{
		for (uint32 AspectIndex = 0; AspectIndex < NumRatios; ++AspectIndex)
		{
			float TargetWidth, TargetHeight;
			float Aspect = Ratios[AspectIndex] / 16.0f;

			if (bInPortraitMode)
			{
				TargetHeight = FMath::RoundToFloat(InDisplayMetrics.PrimaryDisplayHeight * Scales[ScaleIndex]);
				TargetWidth = TargetHeight * Aspect;
			}
			else
			{
				TargetWidth = FMath::RoundToFloat(InDisplayMetrics.PrimaryDisplayWidth * Scales[ScaleIndex]);
				TargetHeight = TargetWidth * Aspect;
			}

			if (TargetWidth < InDisplayMetrics.PrimaryDisplayWidth && TargetHeight < InDisplayMetrics.PrimaryDisplayHeight && TargetWidth >= MinWidth && TargetHeight >= MinHeight)
			{
				OutResolutions.Add(FIntPoint(TargetWidth, TargetHeight));
			}
		}
	}
	
	// if no convenient resolutions have been found, add a minimum one
	if (OutResolutions.Num() == 0)
	{
		if (InDisplayMetrics.PrimaryDisplayHeight > MinHeight && InDisplayMetrics.PrimaryDisplayWidth > MinWidth)
		{
			//Add the minimum size if it fit
			OutResolutions.Add(FIntPoint(MinWidth, MinHeight));
		}
		else
		{
			//Force a resolution even if its smaller then the minimum height and width to avoid a bigger window then the desktop
			float TargetWidth = FMath::RoundToFloat(InDisplayMetrics.PrimaryDisplayWidth) * Scales[NumScales - 1];
			float TargetHeight = FMath::RoundToFloat(InDisplayMetrics.PrimaryDisplayHeight) * Scales[NumScales - 1];
			OutResolutions.Add(FIntPoint(TargetWidth, TargetHeight));
		}
	}
}

/*----------------------------------------------------------------------------
FBoolConfigValueHelper
----------------------------------------------------------------------------*/
FBoolConfigValueHelper::FBoolConfigValueHelper(const TCHAR* Section, const TCHAR* Key, const FString& Filename)
	: bValue(false)
{
	GConfig->GetBool(Section, Key, bValue, Filename);
}

/*----------------------------------------------------------------------------
FBlueprintExceptionTracker
----------------------------------------------------------------------------*/
#if DO_BLUEPRINT_GUARD
void FBlueprintExceptionTracker::ResetRunaway()
{
	Runaway = 0;
	Recurse = 0;
	bRanaway = false;
}

FBlueprintExceptionTracker& FBlueprintExceptionTracker::Get()
{
	return TThreadSingleton<FBlueprintExceptionTracker>::Get();
}
#endif // DO_BLUEPRINT_GUARD

#if WITH_HOT_RELOAD_CTORS
bool GIsRetrievingVTablePtr = false;

void EnsureRetrievingVTablePtrDuringCtor(const TCHAR* CtorSignature)
{
	UE_CLOG(!GIsRetrievingVTablePtr, LogCore, Fatal, TEXT("The %s constructor is for internal usage only for hot-reload purposes. Please do NOT use it."), CtorSignature);
}
#endif // WITH_HOT_RELOAD_CTORS

/*----------------------------------------------------------------------------
NAN Diagnostic Failure
----------------------------------------------------------------------------*/

int32 GEnsureOnNANDiagnostic = false;

#if ENABLE_NAN_DIAGNOSTIC
static FAutoConsoleVariableRef CVarGEnsureOnNANDiagnostic(
	TEXT( "EnsureOnNaNFail" ),
	GEnsureOnNANDiagnostic,
	TEXT( "If set to 1 NaN Diagnostic failures will result in ensures being emitted" )
	);
#endif

#if DO_CHECK
namespace UE4Asserts_Private
{
	void VARARGS InternalLogNANDiagnosticMessage(const TCHAR* FormattedMsg, ...)
	{		
		const int32 TempStrSize = 4096;
		TCHAR TempStr[TempStrSize];
		GET_VARARGS(TempStr, TempStrSize, TempStrSize - 1, FormattedMsg, FormattedMsg);
		UE_LOG(LogCore, Error, TempStr);
	}
}
#endif