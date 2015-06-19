// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "CorePrivatePCH.h"
#include "ICUInternationalization.h"

#if UE_ENABLE_ICU

#include "ICUUtilities.h"
#include "ICUBreakIterator.h"
#include <unicode/locid.h>
#include <unicode/timezone.h>
#include <unicode/uclean.h>
#include <unicode/udata.h>

namespace
{
	struct FICUOverrides
	{
#if STATS
		static int64 BytesInUseCount;
		static int64 CachedBytesInUseCount;
#endif

		static void* U_CALLCONV Malloc(const void* context, size_t size)
		{
			void* Result = FMemory::Malloc(size);
#if STATS
			BytesInUseCount += FMemory::GetAllocSize(Result);
			if(FThreadStats::IsThreadingReady() && CachedBytesInUseCount != BytesInUseCount)
			{
				// The amount of startup stats messages for STAT_MemoryICUTotalAllocationSize is about 700k
				// It needs to be replaced with something like this
				//  FEngineLoop.Tick.{ 
				//		FPlatformMemory::UpdateStats();
				//		
				//	And called once per frame.
				//SET_MEMORY_STAT(STAT_MemoryICUTotalAllocationSize, BytesInUseCount);
				CachedBytesInUseCount = BytesInUseCount;
			}
#endif
			return Result;
		}

		static void* U_CALLCONV Realloc(const void* context, void* mem, size_t size)
		{
			return FMemory::Realloc(mem, size);
		}

		static void U_CALLCONV Free(const void* context, void* mem)
		{
#if STATS
			BytesInUseCount -= FMemory::GetAllocSize(mem);
			if(FThreadStats::IsThreadingReady() && CachedBytesInUseCount != BytesInUseCount)
			{
				//SET_MEMORY_STAT(STAT_MemoryICUTotalAllocationSize, BytesInUseCount);
				CachedBytesInUseCount = BytesInUseCount;
			}
#endif
			FMemory::Free(mem);
		}
	};

#if STATS
	int64 FICUOverrides::BytesInUseCount = 0;
	int64 FICUOverrides::CachedBytesInUseCount = 0;
#endif
}

FICUInternationalization::FICUInternationalization(FInternationalization* const InI18N)
	: I18N(InI18N)
{

}

bool FICUInternationalization::Initialize()
{
	UErrorCode ICUStatus = U_ZERO_ERROR;

#if NEEDS_ICU_DLLS
	LoadDLLs();
#endif /*IS_PROGRAM || !IS_MONOLITHIC*/

	u_setMemoryFunctions(NULL, &(FICUOverrides::Malloc), &(FICUOverrides::Realloc), &(FICUOverrides::Free), &(ICUStatus));

	const FString DataDirectoryRelativeToContent = FString(TEXT("Internationalization"));
	IFileManager& FileManager = IFileManager::Get();

	const FString PotentialDataDirectories[] = 
	{
		FPaths::GameContentDir() / DataDirectoryRelativeToContent, // Try game content directory.
		FPaths::EngineContentDir() / DataDirectoryRelativeToContent, // Try engine content directory.
	};

	bool HasFoundDataDirectory = false;
	for(const auto& DataDirectoryString : PotentialDataDirectories)
	{
		if( FileManager.DirectoryExists( *DataDirectoryString ) )
		{
			u_setDataDirectory( StringCast<char>( *DataDirectoryString ).Get() );
			HasFoundDataDirectory = true;
			break;
		}
	}

	auto GetPrioritizedDataDirectoriesString = [&]() -> FString
	{
		FString Result;
		for(const auto& DataDirectoryString : PotentialDataDirectories)
		{
			if(!Result.IsEmpty())
			{
				Result += TEXT("\n");
			}
			Result += DataDirectoryString;
		}
		return Result;
	};
	checkf( HasFoundDataDirectory, TEXT("ICU data directory was not discovered:\n%s"), *(GetPrioritizedDataDirectoriesString()) );

	u_setDataFileFunctions(nullptr, &FICUInternationalization::OpenDataFile, &FICUInternationalization::CloseDataFile, &(ICUStatus));
	u_init(&(ICUStatus));

	FICUBreakIteratorManager::Create();

	I18N->InvariantCulture = FindOrMakeCulture(TEXT("en-US-POSIX"), false);
	if (!I18N->InvariantCulture.IsValid())
	{
		I18N->InvariantCulture = FindOrMakeCulture(TEXT(""), true);
	}
	I18N->DefaultCulture = FindOrMakeCulture(FPlatformMisc::GetDefaultLocale(), true);
	SetCurrentCulture( I18N->GetDefaultCulture()->GetName() );
	return U_SUCCESS(ICUStatus) ? true : false;
}

void FICUInternationalization::Terminate()
{
	FICUBreakIteratorManager::Destroy();
	CachedCultures.Empty();

	u_cleanup();
#if NEEDS_ICU_DLLS
	UnloadDLLs();
#endif //IS_PROGRAM || !IS_MONOLITHIC
}

#if NEEDS_ICU_DLLS
void FICUInternationalization::LoadDLLs()
{
	// The base directory for ICU binaries is consistent on all platforms.
	FString ICUBinariesRoot = FPaths::EngineDir() / TEXT("Binaries") / TEXT("ThirdParty") / TEXT("ICU") / TEXT("icu4c-53_1");

#if PLATFORM_WINDOWS
#if PLATFORM_64BITS
	const FString PlatformFolderName = TEXT("Win64");
#elif PLATFORM_32BITS
	const FString PlatformFolderName = TEXT("Win32");
#endif //PLATFORM_*BITS

#if _MSC_VER >= 1800
	const FString VSVersionFolderName = TEXT("VS2013");
#else
	const FString VSVersionFolderName = TEXT("VS2012");
#endif //_MSVC_VER

	// Windows requires support for 32/64 bit and also VC11 and VC12 runtimes.
	const FString TargetSpecificPath = ICUBinariesRoot / PlatformFolderName / VSVersionFolderName;

	// Windows libraries use a specific naming convention.
	FString LibraryNameStems[] =
	{
		TEXT("dt"),   // Data
		TEXT("uc"),   // Unicode Common
		TEXT("in"),   // Internationalization
		TEXT("le"),   // Layout Engine
		TEXT("lx"),   // Layout Extensions
		TEXT("io")	// Input/Output
	};
#else
	// Non-Windows libraries use a consistent naming convention.
	FString LibraryNameStems[] =
	{
		TEXT("data"),   // Data
		TEXT("uc"),   // Unicode Common
		TEXT("i18n"),   // Internationalization
		TEXT("le"),   // Layout Engine
		TEXT("lx"),   // Layout Extensions
		TEXT("io")	// Input/Output
	};
#if PLATFORM_LINUX
	const FString TargetSpecificPath = ICUBinariesRoot / TEXT("Linux") / TEXT("x86_64-unknown-linux-gnu");
#elif PLATFORM_MAC
	const FString TargetSpecificPath = ICUBinariesRoot / TEXT("Mac");
#endif //PLATFORM_*
#endif //PLATFORM_*

#if UE_BUILD_DEBUG && !defined(NDEBUG)
	const FString LibraryNamePostfix = TEXT("d");
#else
	const FString LibraryNamePostfix = TEXT("");
#endif //DEBUG

	for(FString Stem : LibraryNameStems)
	{
#if PLATFORM_WINDOWS
		FString LibraryName = "icu" + Stem + LibraryNamePostfix + "53" "." "dll";
#elif PLATFORM_LINUX
		FString LibraryName = "lib" "icu" + Stem + LibraryNamePostfix + ".53.1" "." "so";
#elif PLATFORM_MAC
		FString LibraryName = "lib" "icu" + Stem + ".53.1" + LibraryNamePostfix + "." "dylib";
#endif //PLATFORM_*

		void* DLLHandle = FPlatformProcess::GetDllHandle(*(TargetSpecificPath / LibraryName));
		check(DLLHandle != nullptr);
		DLLHandles.Add(DLLHandle);
	}
}

void FICUInternationalization::UnloadDLLs()
{
	for( const auto DLLHandle : DLLHandles )
	{
		FPlatformProcess::FreeDllHandle(DLLHandle);
	}
	DLLHandles.Reset();
}
#endif // NEEDS_ICU_DLLS

#if STATS
namespace
{
	int64 DataFileBytesInUseCount = 0;
	int64 CachedDataFileBytesInUseCount = 0;
}
#endif

bool FICUInternationalization::SetCurrentCulture(const FString& Name)
{
	FCulturePtr NewCurrentCulture = FindOrMakeCulture(Name);

	if (NewCurrentCulture.IsValid())
	{
		if (NewCurrentCulture.ToSharedRef() != I18N->CurrentCulture)
		{
			I18N->CurrentCulture = NewCurrentCulture.ToSharedRef();

			UErrorCode ICUStatus = U_ZERO_ERROR;
			uloc_setDefault(StringCast<char>(*Name).Get(), &ICUStatus);

			FInternationalization::Get().BroadcastCultureChanged();
		}
	}

	return I18N->CurrentCulture == NewCurrentCulture;
}

void FICUInternationalization::GetCultureNames(TArray<FString>& CultureNames) const
{
	int32_t LocaleCount;
	const icu::Locale* const AvailableLocales = icu::Locale::getAvailableLocales(LocaleCount);

	CultureNames.Reset(LocaleCount);
	for (int32 i = 0; i < LocaleCount; ++i)
	{
		CultureNames.Add(AvailableLocales[i].getName());
	}
}

FCulturePtr FICUInternationalization::GetCulture(const FString& Name)
{
	return FindOrMakeCulture(Name);
}

FCulturePtr FICUInternationalization::FindOrMakeCulture(const FString& Name, const bool AllowDefaultFallback)
{
	const FString CanonicalName = FCulture::GetCanonicalName(Name);

	// Find the cached culture.
	FCultureRef* FoundCulture = CachedCultures.Find(CanonicalName);

	// If no cached culture is found, try to make one.
	if (!FoundCulture)
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;

		// Confirm if data for the desired culture exists.
		if (UResourceBundle* ICUResourceBundle = ures_open(nullptr, StringCast<char>(*CanonicalName).Get(), &ICUStatus))
		{
			// Make the culture only if it actually has some form of data and doesn't fallback to default "root" data, unless overidden to allow it.
			if (ICUStatus != U_USING_DEFAULT_WARNING || AllowDefaultFallback)
			{
				FCulturePtr NewCulture = FCulture::Create(CanonicalName);
				if (NewCulture.IsValid())
				{
					FoundCulture = &(CachedCultures.Add(CanonicalName, NewCulture.ToSharedRef()));
				}
			}

			ures_close(ICUResourceBundle);
		}
	}

	return FoundCulture ? FCulturePtr(*FoundCulture) : FCulturePtr(nullptr);
}

UBool FICUInternationalization::OpenDataFile(const void* context, void** fileContext, void** contents, const char* path)
{
	auto& PathToCachedFileDataMap = FInternationalization::Get().Implementation->PathToCachedFileDataMap;

	// Try to find existing buffer
	FICUInternationalization::FICUCachedFileData* CachedFileData = PathToCachedFileDataMap.Find(path);

	// If there's no file context, we might have to load the file.
	if (!CachedFileData)
	{
		// Attempt to load the file.
		FArchive* FileAr = IFileManager::Get().CreateFileReader(StringCast<TCHAR>(path).Get());
		if (FileAr)
		{
			const int64 FileSize = FileAr->TotalSize();

			// Create file data.
			CachedFileData = &(PathToCachedFileDataMap.Emplace(FString(path), FICUInternationalization::FICUCachedFileData(FileSize)));

			// Load file into buffer.
			FileAr->Serialize(CachedFileData->Buffer, FileSize); 
			delete FileAr;

			// Stat tracking.
#if STATS
			DataFileBytesInUseCount += FMemory::GetAllocSize(CachedFileData->Buffer);
			if (FThreadStats::IsThreadingReady() && CachedDataFileBytesInUseCount != DataFileBytesInUseCount)
			{
				SET_MEMORY_STAT(STAT_MemoryICUDataFileAllocationSize, DataFileBytesInUseCount);
				CachedDataFileBytesInUseCount = DataFileBytesInUseCount;
			}
#endif
		}
	}

	// Add a reference, either the initial one or an additional one.
	if (CachedFileData)
	{
		++(CachedFileData->ReferenceCount);
	}

	// Use the file path as the context, so we can look up the cached file data later and decrement its reference count.
	*fileContext = CachedFileData ? new FString(path) : nullptr;

	// Use the buffer from the cached file data.
	*contents = CachedFileData ? CachedFileData->Buffer : nullptr;

	// If we have cached file data, we must have loaded new data or found existing data, so we've successfully "opened" and "read" the file into "contents".
	return CachedFileData != nullptr;
}

void FICUInternationalization::CloseDataFile(const void* context, void* const fileContext, void* const contents)
{
	// Early out on null context.
	if (fileContext == nullptr)
	{
		return;
	}

	auto& PathToCachedFileDataMap = FInternationalization::Get().Implementation->PathToCachedFileDataMap;

	// The file context is the path to the file.
	FString* const Path = reinterpret_cast<FString*>(fileContext);
	check(Path);

	// Look up the cached file data so we can maintain references.
	FICUInternationalization::FICUCachedFileData* const CachedFileData = PathToCachedFileDataMap.Find(*Path);
	check(CachedFileData);
	check(CachedFileData->Buffer == contents);

	// Remove a reference.
	--(CachedFileData->ReferenceCount);

	// If the last reference has been removed, the cached file data is not longer needed.
	if (CachedFileData->ReferenceCount == 0)
	{
		// Stat tracking.
#if STATS
		DataFileBytesInUseCount -= FMemory::GetAllocSize(CachedFileData->Buffer);
		if (FThreadStats::IsThreadingReady() && CachedDataFileBytesInUseCount != DataFileBytesInUseCount)
		{
			SET_MEMORY_STAT(STAT_MemoryICUDataFileAllocationSize, DataFileBytesInUseCount);
			CachedDataFileBytesInUseCount = DataFileBytesInUseCount;
		}
#endif

		// Delete the cached file data.
		PathToCachedFileDataMap.Remove(*Path);
	}

	// The path string we allocated for tracking is no longer necessary.
	delete Path;
}

FICUInternationalization::FICUCachedFileData::FICUCachedFileData(const int64 FileSize)
	: ReferenceCount(0)
	, Buffer( FICUOverrides::Malloc(nullptr, FileSize) )
{
}

FICUInternationalization::FICUCachedFileData::FICUCachedFileData(const FICUCachedFileData& Source)
{
	checkf(false, TEXT("Cached file data for ICU may not be copy constructed. Something is trying to copy construct FICUCachedFileData."));
}

FICUInternationalization::FICUCachedFileData::FICUCachedFileData(FICUCachedFileData&& Source)
	: ReferenceCount(Source.ReferenceCount)
	, Buffer( Source.Buffer )
{
	// Make sure the moved source object doesn't retain the pointer and free the memory we now point to.
	Source.Buffer = nullptr;
}

FICUInternationalization::FICUCachedFileData::~FICUCachedFileData()
{
	if (Buffer)
	{
		check(ReferenceCount == 0);
		FICUOverrides::Free(nullptr, Buffer);
	}
}

#endif
