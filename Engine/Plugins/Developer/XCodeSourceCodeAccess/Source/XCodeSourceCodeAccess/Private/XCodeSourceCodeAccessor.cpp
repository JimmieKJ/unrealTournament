// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "XCodeSourceCodeAccessPrivatePCH.h"
#include "XCodeSourceCodeAccessor.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "XCodeSourceCodeAccessor"

DEFINE_LOG_CATEGORY_STATIC(LogXcodeAccessor, Log, All);

/** Applescript we use to open XCode */
static const char* OpenXCodeAtFileAndLineAppleScript =
	"on OpenXcodeAtFileAndLine(filepath, linenumber)\n"
	"	set theOffset to offset of \"/\" in filepath\n"
	"	tell application \"Xcode\"\n"
	"		activate\n"
	"		if theOffset is 1 then\n"
	"			open filepath\n"
	"		end if\n"
	"		tell application \"System Events\"\n"
	"			tell process \"Xcode\"\n"
	"				\n"
	"				if theOffset is not 1 then\n"
	"					set bActivated to false\n"
	"					repeat until window \"Open Quickly\" exists\n"
	"						tell application \"Xcode\"\n"
	"							if application \"Xcode\" is not frontmost then\n"
	"								activate\n"
	"							end if\n"
	"						end tell\n"
	"						if application \"Xcode\" is frontmost and bActivated is false then\n"
	"							keystroke \"o\" using {command down, shift down}\n"
	"							set bActivated to true\n"
	"						end if\n"
	"					end repeat\n"
	"					click text field 1 of window \"Open Quickly\"\n"
	"					set value of text field 1 of window \"Open Quickly\" to filepath\n"
	"					keystroke return\n"
	"				end if\n"
	"				\n"
	"				set bActivated to false\n"
	"				repeat until window \"Open Quickly\" exists\n"
	"					tell application \"Xcode\"\n"
	"						if application \"Xcode\" is not frontmost then\n"
	"							activate\n"
	"						end if\n"
	"					end tell\n"
	"					if application \"Xcode\" is frontmost and bActivated is false then\n"
	"						keystroke \"l\" using command down\n"
	"						set bActivated to true\n"
	"					end if\n"
	"				end repeat\n"
	"				\n"
	"				click text field 1 of window \"Open Quickly\"\n"
	"				set value of text field 1 of window \"Open Quickly\" to linenumber\n"
	"				keystroke return\n"
	"				keystroke return\n"
	"			end tell\n"
	"		end tell\n"
	"	end tell\n"
	"end OpenXcodeAtFileAndLine\n"
;

static const char* SaveAllXcodeDocuments =
	"	on SaveAllXcodeDocuments()\n"
	"		tell application \"Xcode\"\n"
	"			save documents\n"
	"		end tell\n"
	"	end SaveAllXcodeDocuments\n"
;

bool FXCodeSourceCodeAccessor::CanAccessSourceCode() const
{
	return IFileManager::Get().DirectoryExists(TEXT("/Applications/Xcode.app"));
}

FName FXCodeSourceCodeAccessor::GetFName() const
{
	return FName("XCodeSourceCodeAccessor");
}

FText FXCodeSourceCodeAccessor::GetNameText() const
{
	return LOCTEXT("XCodeDisplayName", "Xcode");
}

FText FXCodeSourceCodeAccessor::GetDescriptionText() const
{
	return LOCTEXT("XCodeDisplayDesc", "Open source code files in XCode");
}

bool FXCodeSourceCodeAccessor::OpenSolution()
{
	FString SolutionPath;
	if(FDesktopPlatformModule::Get()->GetSolutionPath(SolutionPath))
	{
		const FString FullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead( *SolutionPath );
		if ( FPaths::FileExists( FullPath ) )
		{
			FPlatformProcess::LaunchFileInDefaultExternalApplication( *FullPath );
			return true;
		}
	}
	return false;
}

bool FXCodeSourceCodeAccessor::OpenFileAtLine(const FString& FullPath, int32 LineNumber, int32 ColumnNumber)
{
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>(TEXT("SourceCodeAccess"));

	// column & line numbers are 1-based, so dont allow zero
	if(LineNumber == 0)
	{
		LineNumber++;
	}
	if(ColumnNumber == 0)
	{
		ColumnNumber++;
	}
		 
	bool ExecutionSucceeded = false;
	
	FString SolutionPath;
	if(FDesktopPlatformModule::Get()->GetSolutionPath(SolutionPath))
	{
		const FString ProjPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead( *SolutionPath );
		if ( FPaths::FileExists( ProjPath ) )
		{
			NSString* ProjectPath = [ProjPath.GetNSString() stringByDeletingLastPathComponent];
			[[NSWorkspace sharedWorkspace] openFile:ProjectPath withApplication:@"Xcode" andDeactivate:YES];
			
			NSAppleScript* AppleScript = nil;
			
			NSString* AppleScriptString = [NSString stringWithCString:OpenXCodeAtFileAndLineAppleScript encoding:NSUTF8StringEncoding];
			AppleScript = [[NSAppleScript alloc] initWithSource:AppleScriptString];
			
			int PID = [[NSProcessInfo processInfo] processIdentifier];
			NSAppleEventDescriptor* ThisApplication = [NSAppleEventDescriptor descriptorWithDescriptorType:typeKernelProcessID bytes:&PID length:sizeof(PID)];
			
			NSAppleEventDescriptor* ContainerEvent = [NSAppleEventDescriptor appleEventWithEventClass:'ascr' eventID:'psbr' targetDescriptor:ThisApplication returnID:kAutoGenerateReturnID transactionID:kAnyTransactionID];
			
			[ContainerEvent setParamDescriptor:[NSAppleEventDescriptor descriptorWithString:@"OpenXcodeAtFileAndLine"] forKeyword:'snam'];
			
			{
				NSAppleEventDescriptor* Arguments = [[NSAppleEventDescriptor alloc] initListDescriptor];
				
				CFStringRef FileString = FPlatformString::TCHARToCFString(*FullPath);
				NSString* Path = (NSString*)FileString;
				
				if([Path isAbsolutePath] == NO)
				{
					NSString* CurDir = [[NSFileManager defaultManager] currentDirectoryPath];
					NSString* ResolvedPath = [[NSString stringWithFormat:@"%@/%@", CurDir, Path] stringByResolvingSymlinksInPath];
					if([[NSFileManager defaultManager] fileExistsAtPath:ResolvedPath])
					{
						Path = ResolvedPath;
					}
					else // If it doesn't exist, supply only the filename, we'll use Open Quickly to try and find it from Xcode
					{
						Path = [Path lastPathComponent];
					}
				}
				
				[Arguments insertDescriptor:[NSAppleEventDescriptor descriptorWithString:Path] atIndex:([Arguments numberOfItems] + 1)];
				CFRelease(FileString);
				
				CFStringRef LineString = FPlatformString::TCHARToCFString(*FString::FromInt(LineNumber));
				if(LineString)
				{
					[Arguments insertDescriptor:[NSAppleEventDescriptor descriptorWithString:(NSString*)LineString] atIndex:([Arguments numberOfItems] + 1)];
					CFRelease(LineString);
				}
				else
				{
					[Arguments insertDescriptor:[NSAppleEventDescriptor descriptorWithString:@"1"] atIndex:([Arguments numberOfItems] + 1)];
				}
				
				[ContainerEvent setParamDescriptor:Arguments forKeyword:keyDirectObject];
				[Arguments release];
			}
			
			NSDictionary* ExecutionError = nil;
			[AppleScript executeAppleEvent:ContainerEvent error:&ExecutionError];
			if(ExecutionError == nil)
			{
				ExecutionSucceeded = true;
			}
			else
			{
				UE_LOG(LogXcodeAccessor, Error, TEXT("%s"), *FString([ExecutionError description]));
			}
			
			[AppleScript release];
			
			// Fallback to trivial implementation when something goes wrong (like not having permission for UI scripting)
			if(ExecutionSucceeded == false)
			{
				FPlatformProcess::LaunchFileInDefaultExternalApplication(*FullPath);
				ExecutionSucceeded = true;
			}
		}
	}
		
	return ExecutionSucceeded;
}

bool FXCodeSourceCodeAccessor::OpenSourceFiles(const TArray<FString>& AbsoluteSourcePaths)
{
	for ( const FString& SourcePath : AbsoluteSourcePaths )
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*SourcePath);
	}

	return true;
}

bool FXCodeSourceCodeAccessor::AddSourceFiles(const TArray<FString>& AbsoluteSourcePaths, const TArray<FString>& AvailableModules)
{
	return false;
}

bool FXCodeSourceCodeAccessor::SaveAllOpenDocuments() const
{
	bool ExecutionSucceeded = false;
	
	NSAppleScript* AppleScript = nil;
	
	NSString* AppleScriptString = [NSString stringWithCString:SaveAllXcodeDocuments encoding:NSUTF8StringEncoding];
	AppleScript = [[NSAppleScript alloc] initWithSource:AppleScriptString];
	
	int PID = [[NSProcessInfo processInfo] processIdentifier];
	NSAppleEventDescriptor* ThisApplication = [NSAppleEventDescriptor descriptorWithDescriptorType:typeKernelProcessID bytes:&PID length:sizeof(PID)];
	
	NSAppleEventDescriptor* ContainerEvent = [NSAppleEventDescriptor appleEventWithEventClass:'ascr' eventID:'psbr' targetDescriptor:ThisApplication returnID:kAutoGenerateReturnID transactionID:kAnyTransactionID];
	
	[ContainerEvent setParamDescriptor:[NSAppleEventDescriptor descriptorWithString:@"SaveAllXcodeDocuments"] forKeyword:'snam'];
	
	NSDictionary* ExecutionError = nil;
	[AppleScript executeAppleEvent:ContainerEvent error:&ExecutionError];
	if(ExecutionError == nil)
	{
		ExecutionSucceeded = true;
	}
	else
	{
		UE_LOG(LogXcodeAccessor, Error, TEXT("%s"), *FString([ExecutionError description]));
	}
	
	[AppleScript release];
	return ExecutionSucceeded;
}

void FXCodeSourceCodeAccessor::Tick(const float DeltaTime)
{
}

#undef LOCTEXT_NAMESPACE