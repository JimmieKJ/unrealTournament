// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DesktopPlatformPrivatePCH.h"
#include "FeedbackContextMarkup.h"
#include "WindowsNativeFeedbackContext.h"

#include "AllowWindowsPlatformTypes.h"
	#include <commdlg.h>
	#include <shellapi.h>
	#include <shlobj.h>
	#include <Winver.h>
	#include <LM.h>
	#include <tlhelp32.h>
	#include <Psapi.h>
#include "HideWindowsPlatformTypes.h"

#pragma comment( lib, "version.lib" )

#define LOCTEXT_NAMESPACE "DesktopPlatform"
#define MAX_FILETYPES_STR 4096
#define MAX_FILENAME_STR 65536 // This buffer has to be big enough to contain the names of all the selected files as well as the null characters between them and the null character at the end

static const TCHAR *InstallationsSubKey = TEXT("SOFTWARE\\Epic Games\\Unreal Engine\\Builds");

bool FDesktopPlatformWindows::OpenFileDialog( const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex )
{
	return FileDialogShared(false, ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, OutFilterIndex );
}

bool FDesktopPlatformWindows::OpenFileDialog( const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
{
	int32 DummyFilterIndex;
	return FileDialogShared(false, ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, DummyFilterIndex );
}

bool FDesktopPlatformWindows::SaveFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
{
	int32 DummyFilterIndex = 0;
	return FileDialogShared(true, ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, DummyFilterIndex );
}

static ::INT CALLBACK BrowseCallbackProc(HWND hwnd, ::UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	// Set the path to the start path upon initialization.
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
		if ( lpData )
		{
			SendMessage(hwnd, BFFM_SETSELECTION, true, lpData);
		}
		break;
	}

	return 0;
}

bool FDesktopPlatformWindows::OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, FString& OutFolderName)
{
	FScopedSystemModalMode SystemModalScope;

	BROWSEINFO bi;
	ZeroMemory(&bi, sizeof(BROWSEINFO));

	TCHAR FolderName[MAX_PATH];
	ZeroMemory(FolderName, sizeof(TCHAR) * MAX_PATH);

	const FString PathToSelect = DefaultPath.Replace(TEXT("/"), TEXT("\\"));

	bi.hwndOwner = (HWND)ParentWindowHandle;
	bi.pszDisplayName = FolderName;
	bi.lpszTitle = *DialogTitle;
	bi.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS | BIF_SHAREABLE;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = (LPARAM)(*PathToSelect);
	bool bSuccess = false;
	LPCITEMIDLIST Folder = ::SHBrowseForFolder(&bi);
	if (Folder) 
	{
		bSuccess = ( ::SHGetPathFromIDList(Folder, FolderName) ? true : false );
		if ( bSuccess )
		{
			OutFolderName = FolderName;
			FPaths::NormalizeFilename(OutFolderName);
		}
		else
		{
			UE_LOG(LogDesktopPlatform, Warning, TEXT("Error converting the folder path to a string."));
		}
	}
	else
	{
		UE_LOG(LogDesktopPlatform, Warning, TEXT("Error reading results of folder dialog."));
	}

	return bSuccess;
}

bool FDesktopPlatformWindows::OpenFontDialog(const void* ParentWindowHandle, FString& OutFontName, float& OutHeight, EFontImportFlags& OutFlags)
{	
	FScopedSystemModalMode SystemModalScope;

	CHOOSEFONT cf;
	ZeroMemory(&cf, sizeof(CHOOSEFONT));

	LOGFONT lf;
	ZeroMemory(&lf, sizeof(LOGFONT));

	cf.lStructSize = sizeof(CHOOSEFONT);
	cf.hwndOwner = (HWND)ParentWindowHandle;
	cf.lpLogFont = &lf;
	cf.Flags = CF_EFFECTS | CF_SCREENFONTS;
	bool bSuccess = !!::ChooseFont(&cf);
	if ( bSuccess )
	{
		HDC DC = ::GetDC( cf.hwndOwner ); 
		const float LogicalPixelsY = static_cast<float>(GetDeviceCaps(DC, LOGPIXELSY));
		const int32 PixelHeight = static_cast<int32>(-lf.lfHeight * ( 72.0f / LogicalPixelsY ));	// Always target 72 DPI
		auto FontFlags = EFontImportFlags::None;
		if ( lf.lfUnderline )
		{
			FontFlags |= EFontImportFlags::EnableUnderline;
		}
		if ( lf.lfItalic )
		{
			FontFlags |= EFontImportFlags::EnableItalic;
		}
		if ( lf.lfWeight == FW_BOLD )
		{
			FontFlags |= EFontImportFlags::EnableBold;
		}

		OutFontName = (const TCHAR*)lf.lfFaceName;
		OutHeight = PixelHeight;
		OutFlags = FontFlags;

		::ReleaseDC( cf.hwndOwner, DC ); 
	}
	else
	{
		UE_LOG(LogDesktopPlatform, Warning, TEXT("Error reading results of font dialog."));
	}

	return bSuccess;
}

bool FDesktopPlatformWindows::CanOpenLauncher(bool Install)
{
	// Check if the launcher exists, or (optionally) if the installer exists
	FString Path;
	return GetLauncherPath(Path) || (Install && GetLauncherInstallerPath(Path));
}

bool FDesktopPlatformWindows::OpenLauncher(bool Install, FString CommandLineParams )
{
	// Try to launch it directly
	FString LauncherPath;
	if(GetLauncherPath(LauncherPath))
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("Dev")))
		{
			CommandLineParams += TEXT(" -noselfupdate");
		}
		return FPlatformProcess::CreateProc(*LauncherPath, *CommandLineParams, true, false, false, NULL, 0, *FPaths::GetPath(LauncherPath), NULL).IsValid();
	}

	// Otherwise see if we can install it
	FString InstallerPath;
	if(Install && GetLauncherInstallerPath(InstallerPath))
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*InstallerPath);
		return true;
	}

	return false;
}

bool FDesktopPlatformWindows::GetLauncherPath(FString& OutLauncherPath)
{
	// Try the default executable in the binaries directory
	if (FParse::Param(FCommandLine::Get(), TEXT("Dev")))
	{
		FString LauncherPath = FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries"), TEXT("Win64"), TEXT("UnrealEngineLauncher-Win64-Debug.exe"));
		if(FPaths::FileExists(LauncherPath))
		{
			OutLauncherPath = LauncherPath;
			return true;
		}
	}
	else
	{
		FString LauncherPath = FPaths::Combine(*FPaths::EngineDir(), TEXT("../../Launcher/Engine/Binaries/Win64/UnrealEngineLauncher.exe"));
		if(FPaths::FileExists(LauncherPath))
		{
			OutLauncherPath = LauncherPath;
			return true;
		}
	}

	// Otherwise locate it in the Registry...
	FString InstallDir;
	if (FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("Software\\EpicGames\\Unreal Engine"), TEXT("InstallDir"), InstallDir) && (InstallDir.Len() > 0))
	{
		FString LauncherPath = FPaths::Combine(*InstallDir, TEXT("Launcher/Engine/Binaries/Win64/UnrealEngineLauncher.exe"));
		if(FPaths::FileExists(LauncherPath))
		{
			OutLauncherPath = LauncherPath;
			return true;
		}
	}

	// Otherwise fail
	return false;
}

bool FDesktopPlatformWindows::GetLauncherInstallerPath(FString& OutInstallerPath)
{
	FString InstallerPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::EngineDir(), TEXT("Extras/UnrealEngineLauncher/UnrealEngineInstaller.msi")));
	if(FPaths::FileExists(InstallerPath))
	{
		OutInstallerPath = InstallerPath;
		return true;
	}
	return false;
}

bool FDesktopPlatformWindows::FileDialogShared(bool bSave, const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex)
{
	FScopedSystemModalMode SystemModalScope;

	WCHAR Filename[MAX_FILENAME_STR];
	FCString::Strcpy(Filename, MAX_FILENAME_STR, *(DefaultFile.Replace(TEXT("/"), TEXT("\\"))));

	// Convert the forward slashes in the path name to backslashes, otherwise it'll be ignored as invalid and use whatever is cached in the registry
	WCHAR Pathname[MAX_FILENAME_STR];
	FCString::Strcpy(Pathname, MAX_FILENAME_STR, *(FPaths::ConvertRelativePathToFull(DefaultPath).Replace(TEXT("/"), TEXT("\\"))));

	// Convert the "|" delimited list of filetypes to NULL delimited then add a second NULL character to indicate the end of the list
	WCHAR FileTypeStr[MAX_FILETYPES_STR];
	WCHAR* FileTypesPtr = NULL;
	const int32 FileTypesLen = FileTypes.Len();

	// Nicely formatted file types for lookup later and suitable to append to filenames without extensions
	TArray<FString> CleanExtensionList;

	// The strings must be in pairs for windows.
	// It is formatted as follows: Pair1String1|Pair1String2|Pair2String1|Pair2String2
	// where the second string in the pair is the extension.  To get the clean extensions we only care about the second string in the pair
	TArray<FString> UnformattedExtensions;
	FileTypes.ParseIntoArray( UnformattedExtensions, TEXT("|"), true );
	for( int32 ExtensionIndex = 1; ExtensionIndex < UnformattedExtensions.Num(); ExtensionIndex += 2)
	{
		const FString& Extension = UnformattedExtensions[ExtensionIndex];
		// Assume the user typed in an extension or doesnt want one when using the *.* extension. We can't determine what extension they wan't in that case
		if( Extension != TEXT("*.*") )
		{
			// Add to the clean extension list, first removing the * wildcard from the extension
			int32 WildCardIndex = Extension.Find( TEXT("*") );
			CleanExtensionList.Add( WildCardIndex != INDEX_NONE ? Extension.RightChop( WildCardIndex+1 ) : Extension );
		}
	}

	if (FileTypesLen > 0 && FileTypesLen - 1 < MAX_FILETYPES_STR)
	{
		FileTypesPtr = FileTypeStr;
		FCString::Strcpy(FileTypeStr, MAX_FILETYPES_STR, *FileTypes);

		TCHAR* Pos = FileTypeStr;
		while( Pos[0] != 0 )
		{
			if ( Pos[0] == '|' )
			{
				Pos[0] = 0;
			}

			Pos++;
		}

		// Add two trailing NULL characters to indicate the end of the list
		FileTypeStr[FileTypesLen] = 0;
		FileTypeStr[FileTypesLen + 1] = 0;
	}

	OPENFILENAME ofn;
	FMemory::Memzero(&ofn, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = (HWND)ParentWindowHandle;
	ofn.lpstrFilter = FileTypesPtr;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = Filename;
	ofn.nMaxFile = MAX_FILENAME_STR;
	ofn.lpstrInitialDir = Pathname;
	ofn.lpstrTitle = *DialogTitle;
	if(FileTypesLen > 0)
	{
		ofn.lpstrDefExt = &FileTypeStr[0];
	}

	ofn.Flags = OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER;

	if ( bSave )
	{
		ofn.Flags |= OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT | OFN_NOVALIDATE;
	}
	else
	{
		ofn.Flags |= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	}

	if ( Flags & EFileDialogFlags::Multiple )
	{
		ofn.Flags |= OFN_ALLOWMULTISELECT;
	}

	bool bSuccess;
	if ( bSave )
	{
		bSuccess = !!::GetSaveFileName(&ofn);
	}
	else
	{
		bSuccess = !!::GetOpenFileName(&ofn);
	}

	if ( bSuccess )
	{
		// GetOpenFileName/GetSaveFileName changes the CWD on success. Change it back immediately.
		FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

		if ( Flags & EFileDialogFlags::Multiple )
		{
			// When selecting multiple files, the returned string is a NULL delimited list
			// where the first element is the directory and all remaining elements are filenames.
			// There is an extra NULL character to indicate the end of the list.
			FString DirectoryOrSingleFileName = FString(Filename);
			TCHAR* Pos = Filename + DirectoryOrSingleFileName.Len() + 1;

			if ( Pos[0] == 0 )
			{
				// One item selected. There was an extra trailing NULL character.
				OutFilenames.Add(DirectoryOrSingleFileName);
			}
			else
			{
				// Multiple items selected. Keep adding filenames until two NULL characters.
				FString SelectedFile;
				do
				{
					SelectedFile = FString(Pos);
					new(OutFilenames) FString(DirectoryOrSingleFileName / SelectedFile);
					Pos += SelectedFile.Len() + 1;
				}
				while (Pos[0] != 0);
			}
		}
		else
		{
			new(OutFilenames) FString(Filename);
		}

		// The index of the filter in OPENFILENAME starts at 1.
		OutFilterIndex = ofn.nFilterIndex - 1;

		// Get the extension to add to the filename (if one doesnt already exist)
		FString Extension = CleanExtensionList.IsValidIndex( OutFilterIndex ) ? CleanExtensionList[OutFilterIndex] : TEXT("");

		// Make sure all filenames gathered have their paths normalized and proper extensions added
		for ( auto FilenameIt = OutFilenames.CreateIterator(); FilenameIt; ++FilenameIt )
		{
			FString& Filename = *FilenameIt;
			
			Filename = IFileManager::Get().ConvertToRelativePath(*Filename);

			if( FPaths::GetExtension(Filename).IsEmpty() && !Extension.IsEmpty() )
			{
				// filename does not have an extension. Add an extension based on the filter that the user chose in the dialog
				Filename += Extension;
			}

			FPaths::NormalizeFilename(Filename);
		}
	}
	else
	{
		uint32 Error = ::CommDlgExtendedError();
		if ( Error != ERROR_SUCCESS )
		{
			UE_LOG(LogDesktopPlatform, Warning, TEXT("Error reading results of file dialog. Error: 0x%04X"), Error);
		}
	}

	return bSuccess;
}

bool FDesktopPlatformWindows::RegisterEngineInstallation(const FString &RootDir, FString &OutIdentifier)
{
	bool bRes = false;
	if(IsValidRootDirectory(RootDir))
	{
		HKEY hRootKey;
		if(RegCreateKeyEx(HKEY_CURRENT_USER, InstallationsSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRootKey, NULL) == ERROR_SUCCESS)
		{
			FString NewIdentifier = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces);
			LRESULT SetResult = RegSetValueEx(hRootKey, *NewIdentifier, 0, REG_SZ, (const BYTE*)*RootDir, (RootDir.Len() + 1) * sizeof(TCHAR));
			RegCloseKey(hRootKey);

			if(SetResult == ERROR_SUCCESS)
			{
				OutIdentifier = NewIdentifier;
				bRes = true;
			}
		}
	}
	return bRes;
}

void FDesktopPlatformWindows::EnumerateEngineInstallations(TMap<FString, FString> &OutInstallations)
{
	// Enumerate the binary installations
	EnumerateLauncherEngineInstallations(OutInstallations);

	// Enumerate the per-user installations
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, InstallationsSubKey, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
	{
		// Get a list of all the directories
		TArray<FString> UniqueDirectories;
		OutInstallations.GenerateValueArray(UniqueDirectories);

		// Enumerate all the installations
		TArray<FString> InvalidKeys;
		for (::DWORD Index = 0;; Index++)
		{
			TCHAR ValueName[256];
			TCHAR ValueData[MAX_PATH];
			::DWORD ValueType = 0;
			::DWORD ValueNameLength = sizeof(ValueName) / sizeof(ValueName[0]);
			::DWORD ValueDataSize = sizeof(ValueData);

			LRESULT Result = RegEnumValue(hKey, Index, ValueName, &ValueNameLength, NULL, &ValueType, (BYTE*)&ValueData[0], &ValueDataSize);
			if(Result == ERROR_SUCCESS)
			{
				int32 ValueDataLength = ValueDataSize / sizeof(TCHAR);
				if(ValueDataLength > 0 && ValueData[ValueDataLength - 1] == 0) ValueDataLength--;

				FString NormalizedInstalledDirectory(ValueDataLength, ValueData);
				FPaths::NormalizeDirectoryName(NormalizedInstalledDirectory);
				FPaths::CollapseRelativeDirectories(NormalizedInstalledDirectory);

				if(IsValidRootDirectory(NormalizedInstalledDirectory) && !UniqueDirectories.Contains(NormalizedInstalledDirectory))
				{
					OutInstallations.Add(ValueName, NormalizedInstalledDirectory);
					UniqueDirectories.Add(NormalizedInstalledDirectory);
				}
				else
				{
					InvalidKeys.Add(ValueName);
				}
			}
			else if(Result == ERROR_NO_MORE_ITEMS)
			{
				break;
			}
		}

		// Remove all the keys which weren't valid
		for(const FString InvalidKey: InvalidKeys)
		{
			RegDeleteValue(hKey, *InvalidKey);
		}

		RegCloseKey(hKey);
	}
}

bool FDesktopPlatformWindows::IsSourceDistribution(const FString &RootDir)
{
	// Check for the existence of a GenerateProjectFiles.bat file. This allows compatibility with the GitHub 4.0 release.
	FString GenerateProjectFilesPath = RootDir / TEXT("GenerateProjectFiles.bat");
	if (IFileManager::Get().FileSize(*GenerateProjectFilesPath) >= 0)
	{
		return true;
	}

	// Otherwise use the default test
	return FDesktopPlatformBase::IsSourceDistribution(RootDir);
}

bool FDesktopPlatformWindows::VerifyFileAssociations()
{
	TIndirectArray<FRegistryRootedKey> Keys;
	GetRequiredRegistrySettings(Keys);

	for (int32 Idx = 0; Idx < Keys.Num(); Idx++)
	{
		if (!Keys[Idx].IsUpToDate(true))
		{
			return false;
		}
	}

	return true;
}

bool FDesktopPlatformWindows::UpdateFileAssociations()
{
	TIndirectArray<FRegistryRootedKey> Keys;
	GetRequiredRegistrySettings(Keys);

	for (int32 Idx = 0; Idx < Keys.Num(); Idx++)
	{
		if (!Keys[Idx].Write(true))
		{
			return false;
		}
	}

	return true;
}

bool FDesktopPlatformWindows::OpenProject(const FString &ProjectFileName)
{
	// Get the project filename in a native format
	FString PlatformProjectFileName = ProjectFileName;
	FPaths::MakePlatformFilename(PlatformProjectFileName);

	// Always treat projects as an Unreal.ProjectFile, don't allow the user overriding the file association to take effect
	SHELLEXECUTEINFO Info;
	ZeroMemory(&Info, sizeof(Info));
	Info.cbSize = sizeof(Info);
	Info.fMask = SEE_MASK_CLASSNAME;
	Info.lpVerb = TEXT("open");
	Info.nShow = SW_SHOWNORMAL;
	Info.lpFile = *PlatformProjectFileName;
	Info.lpClass = TEXT("Unreal.ProjectFile");
	return ::ShellExecuteExW(&Info) != 0;
}

bool FDesktopPlatformWindows::RunUnrealBuildTool(const FText& Description, const FString& RootDir, const FString& Arguments, FFeedbackContext* Warn)
{
	// Get the path to UBT
	FString UnrealBuildToolPath = RootDir / TEXT("Engine/Binaries/DotNET/UnrealBuildTool.exe");
	if(IFileManager::Get().FileSize(*UnrealBuildToolPath) < 0)
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("Couldn't find UnrealBuildTool at '%s'"), *UnrealBuildToolPath);
		return false;
	}

	// Write the output
	Warn->Logf(TEXT("Running %s %s"), *UnrealBuildToolPath, *Arguments);

	// Spawn UBT
	int32 ExitCode = 0;
	return FFeedbackContextMarkup::PipeProcessOutput(Description, UnrealBuildToolPath, Arguments, Warn, &ExitCode) && ExitCode == 0;
}

bool FDesktopPlatformWindows::IsUnrealBuildToolRunning()
{
	FString UBTPath = GetUnrealBuildToolExecutableFilename(FPaths::RootDir());
	FPaths::MakePlatformFilename(UBTPath);

	HANDLE SnapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (SnapShot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 Entry;
		Entry.dwSize = sizeof(PROCESSENTRY32);

		if (::Process32First(SnapShot, &Entry))
		{
			do
			{
				const FString EntryFullPath = FPlatformProcess::GetApplicationName(Entry.th32ProcessID);
				if (EntryFullPath == UBTPath)
				{
					::CloseHandle(SnapShot);
					return true;
				}
			} while (::Process32Next(SnapShot, &Entry));
		}
	}
	::CloseHandle(SnapShot);
	return false;
}

FFeedbackContext* FDesktopPlatformWindows::GetNativeFeedbackContext()
{
	static FWindowsNativeFeedbackContext FeedbackContext;
	return &FeedbackContext;
}

FString FDesktopPlatformWindows::GetUserTempPath()
{
	return FString(FPlatformProcess::UserTempDir());
}

void FDesktopPlatformWindows::GetRequiredRegistrySettings(TIndirectArray<FRegistryRootedKey> &RootedKeys)
{
	// Get the path to VersionSelector.exe. If we're running from UnrealVersionSelector itself, try to stick with the current configuration.
	FString DefaultVersionSelectorName = FPlatformProcess::ExecutableName(false);
	if (!DefaultVersionSelectorName.StartsWith(TEXT("UnrealVersionSelector")))
	{
		DefaultVersionSelectorName = TEXT("UnrealVersionSelector.exe");
	}
	FString ExecutableFileName = FString(FPlatformProcess::BaseDir()) / DefaultVersionSelectorName;

	// Defer to UnrealVersionSelector.exe in a launcher installation if it's got the same version number or greater.
	FString InstallDir;
	if (FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\EpicGames\\Unreal Engine"), TEXT("INSTALLDIR"), InstallDir) && (InstallDir.Len() > 0))
	{
		FString NormalizedInstallDir = InstallDir;
		FPaths::NormalizeDirectoryName(NormalizedInstallDir);

		FString InstalledExecutableFileName = NormalizedInstallDir / FString("Launcher/Engine/Binaries/Win64/UnrealVersionSelector.exe");
		if(GetShellIntegrationVersion(InstalledExecutableFileName) == GetShellIntegrationVersion(ExecutableFileName))
		{
			ExecutableFileName = InstalledExecutableFileName;
		}
	}

	// Get the path to the executable
	FPaths::MakePlatformFilename(ExecutableFileName);
	FString QuotedExecutableFileName = FString(TEXT("\"")) + ExecutableFileName + FString(TEXT("\""));

	// HKCU\SOFTWARE\Classes\.uproject
	FRegistryRootedKey *UserRootExtensionKey = new FRegistryRootedKey(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Classes\\.uproject"));
	RootedKeys.Add(UserRootExtensionKey);

	// HKLM\SOFTWARE\Classes\.uproject
	FRegistryRootedKey *RootExtensionKey = new FRegistryRootedKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Classes\\.uproject"));
	RootExtensionKey->Key = new FRegistryKey();
	RootExtensionKey->Key->SetValue(TEXT(""), TEXT("Unreal.ProjectFile"));
	RootedKeys.Add(RootExtensionKey);

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile
	FRegistryRootedKey *RootFileTypeKey = new FRegistryRootedKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Classes\\Unreal.ProjectFile"));
	RootFileTypeKey->Key = new FRegistryKey();
	RootFileTypeKey->Key->SetValue(TEXT(""), TEXT("Unreal Engine Project File"));
	RootFileTypeKey->Key->FindOrAddKey(L"DefaultIcon")->SetValue(TEXT(""), QuotedExecutableFileName);
	RootedKeys.Add(RootFileTypeKey);

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell
	FRegistryKey *ShellKey = RootFileTypeKey->Key->FindOrAddKey(TEXT("shell"));

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell\open
	FRegistryKey *ShellOpenKey = ShellKey->FindOrAddKey(TEXT("open"));
	ShellOpenKey->SetValue(L"", L"Open");
	ShellOpenKey->FindOrAddKey(L"command")->SetValue(L"", QuotedExecutableFileName + FString(TEXT(" /editor \"%1\"")));

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell\run
	FRegistryKey *ShellRunKey = ShellKey->FindOrAddKey(TEXT("run"));
	ShellRunKey->SetValue(TEXT(""), TEXT("Launch game"));
	ShellRunKey->SetValue(TEXT("Icon"), QuotedExecutableFileName);
	ShellRunKey->FindOrAddKey(L"command")->SetValue(TEXT(""), QuotedExecutableFileName + FString(TEXT(" /game \"%1\"")));

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell\rungenproj
	FRegistryKey *ShellProjectKey = ShellKey->FindOrAddKey(TEXT("rungenproj"));
	ShellProjectKey->SetValue(L"", L"Generate Visual Studio project files");
	ShellProjectKey->SetValue(L"Icon", QuotedExecutableFileName);
	ShellProjectKey->FindOrAddKey(L"command")->SetValue(TEXT(""), QuotedExecutableFileName + FString(TEXT(" /projectfiles \"%1\"")));

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell\switchversion
	FRegistryKey *ShellVersionKey = ShellKey->FindOrAddKey(TEXT("switchversion"));
	ShellVersionKey->SetValue(TEXT(""), TEXT("Switch Unreal Engine version..."));
	ShellVersionKey->SetValue(TEXT("Icon"), QuotedExecutableFileName);
	ShellVersionKey->FindOrAddKey(L"command")->SetValue(TEXT(""), QuotedExecutableFileName + FString(TEXT(" /switchversion \"%1\"")));

	// If the user has manually selected something other than our extension, we need to delete it. Explorer explicitly disables set access on that keys in that folder, but we can delete the whole thing.
	const TCHAR* UserChoicePath = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.uproject\\UserChoice");

	// Figure out whether we need to delete it. If it's already set to our own ProgId, leave it alone.
	bool bDeleteUserChoiceKey = true;
	HKEY hUserChoiceKey;
	if(RegOpenKeyEx(HKEY_CURRENT_USER, UserChoicePath, 0, KEY_READ, &hUserChoiceKey) == S_OK)
	{
		TCHAR ProgId[128];
		::DWORD ProgIdSize = sizeof(ProgId);
		::DWORD ProgIdType = 0;
		if(RegQueryValueEx(hUserChoiceKey, TEXT("Progid"), NULL, &ProgIdType, (BYTE*)ProgId, &ProgIdSize) == ERROR_SUCCESS && ProgIdType == REG_SZ)
		{
			bDeleteUserChoiceKey = (FCString::Strcmp(ProgId, TEXT("Unreal.ProjectFile")) != 0);
		}
		RegCloseKey(hUserChoiceKey);
	}
	if(bDeleteUserChoiceKey)
	{
		RootedKeys.Add(new FRegistryRootedKey(HKEY_CURRENT_USER, UserChoicePath));
	}
}

int32 FDesktopPlatformWindows::GetShellIntegrationVersion(const FString &FileName)
{
	::DWORD VersionInfoSize = GetFileVersionInfoSize(*FileName, NULL);
	if (VersionInfoSize != 0)
	{
		TArray<uint8> VersionInfo;
		VersionInfo.AddUninitialized(VersionInfoSize);
		if (GetFileVersionInfo(*FileName, NULL, VersionInfoSize, VersionInfo.GetData()))
		{
			TCHAR *ShellVersion;
			::UINT ShellVersionLen;
			if (VerQueryValue(VersionInfo.GetData(), TEXT("\\StringFileInfo\\040904b0\\ShellIntegrationVersion"), (LPVOID*)&ShellVersion, &ShellVersionLen))
			{
				TCHAR *ShellVersionEnd;
				int32 Version = FCString::Strtoi(ShellVersion, &ShellVersionEnd, 10);
				if(*ShellVersionEnd == 0)
				{
					return Version;
				}
			}
		}
	}
	return 0;
}

#undef LOCTEXT_NAMESPACE
