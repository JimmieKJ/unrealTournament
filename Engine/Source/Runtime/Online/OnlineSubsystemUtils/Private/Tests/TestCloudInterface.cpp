// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "TestCloudInterface.h"
#include "OnlineIdentityInterface.h"
#include "ModuleManager.h"

void FTestCloudInterface::Test(UWorld* InWorld)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(InWorld, FName(*Subsystem));
	check(OnlineSub); 

	if (OnlineSub->GetIdentityInterface().IsValid())
	{
		UserId = OnlineSub->GetIdentityInterface()->GetUniquePlayerId(0);
	}
	if (UserId.IsValid())
	{
		// Cache interfaces
		UserCloud = OnlineSub->GetUserCloudInterface();
		if (UserCloud.IsValid())
		{
			// Setup delegates
			EnumerationDelegate = FOnEnumerateUserFilesCompleteDelegate::CreateRaw(this, &FTestCloudInterface::OnEnumerateUserFilesComplete);
			OnWriteUserCloudFileCompleteDelegate = FOnWriteUserFileCompleteDelegate::CreateRaw(this, &FTestCloudInterface::OnWriteUserCloudFileComplete);
			OnReadEnumeratedUserFilesCompleteDelegate = FOnReadUserFileCompleteDelegate::CreateRaw(this, &FTestCloudInterface::OnReadEnumeratedUserFilesComplete);
			OnDeleteEnumeratedUserFilesCompleteDelegate = FOnDeleteUserFileCompleteDelegate::CreateRaw(this, &FTestCloudInterface::OnDeleteEnumeratedUserFilesComplete);

			SharedCloud = OnlineSub->GetSharedCloudInterface();
			if (SharedCloud.IsValid())
			{
				OnWriteSharedCloudFileCompleteDelegate = FOnWriteSharedFileCompleteDelegate::CreateRaw(this, &FTestCloudInterface::OnWriteSharedCloudFileComplete);
				OnReadEnumerateSharedFileCompleteDelegate = FOnReadSharedFileCompleteDelegate::CreateRaw(this, &FTestCloudInterface::OnReadEnumeratedSharedFileCompleteDelegate);

				// Pre-generated content
				SharedCloud->GetDummySharedHandlesForTest(RandomSharedFileHandles);
			}
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Cloud test failed.  Cloud API not supported."));
			delete this;
		}
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT("Cloud test failed.  No logged in user."));
		delete this;
	}
}

bool FTestCloudInterface::Tick( float DeltaTime )
{
	if (TestPhase != LastTestPhase)
	{
		if (!bOverallSuccess)
		{
			UE_LOG(LogOnline, Log, TEXT("Testing failed in phase %d"), LastTestPhase);
			TestPhase = 10;
		}
		if (!SharedCloud.IsValid() && 
			TestPhase > 4)
		{
			UE_LOG(LogOnline, Log, TEXT("Skipping shared cloud tests"));
			TestPhase = 10;
		}

		switch(TestPhase)
		{
		case 0:
			// Enumerate all files at the beginning
			EnumerateUserFiles();
			break;
		case 1:
			// Write out N files for the user of various sizes/names and enumerate
			// @todo: make the file count configurable on a per-platform basis, since different platforms have different file count limits
			WriteNUserCloudFiles(*UserId, FString(TEXT("UserCloud.bin")), 15, OnWriteUserCloudFileCompleteDelegate);
			break;
		case 2:
			// Read in N files for the user and enumerate
			ReadEnumeratedUserFiles(OnReadEnumeratedUserFilesCompleteDelegate);
			break;
		case 3: 
			// Forget the N files for the user and enumerate (should still remain locally)
			DeleteEnumeratedUserFiles(true, false, OnDeleteEnumeratedUserFilesCompleteDelegate);
			break;
		case 4:
			// Delete the N files for the user and enumerate (should be removed permanently)
			DeleteEnumeratedUserFiles(false, true, OnDeleteEnumeratedUserFilesCompleteDelegate);
			break;
		case 5:
			// Write out N files for the user and share with the cloud and enumerate
			WriteNSharedCloudFiles(*UserId, FString(TEXT("SharedCloud.bin")), 15, OnWriteSharedCloudFileCompleteDelegate);
			break;
		case 6:
			// Read in N files given the shared handles that this user generated
			ReadEnumeratedSharedFiles(false, OnReadEnumerateSharedFileCompleteDelegate);
			break;
		case 7:
			// Delete, marking all the files as only forgotten
			DeleteEnumeratedUserFiles(true, false, OnDeleteEnumeratedUserFilesCompleteDelegate);
			break;
		case 8:
			// Try to read in the N random files from their shared handles
			ReadEnumeratedSharedFiles(true, OnReadEnumerateSharedFileCompleteDelegate);
			break;
		case 9:
			// Delete, marking all the files for permanent removal
			DeleteEnumeratedUserFiles(true, true, OnDeleteEnumeratedUserFilesCompleteDelegate);
			break;	
		case 10:
			bOverallSuccess = bOverallSuccess && Cleanup();
			UE_LOG(LogOnline, Log, TEXT("TESTING COMPLETE Success:%s!"), bOverallSuccess ? TEXT("true") : TEXT("false"));
			delete this;
			return false;
		}
		LastTestPhase = TestPhase;
	}
	return true;
}

bool FTestCloudInterface::Cleanup()
{
	bool bSuccess = true;
	if (UserCloud.IsValid() &&
		!UserCloud->ClearFiles(*UserId))
	{
		UE_LOG(LogOnline, Log, TEXT("Failed to cleanup user files"));
		bSuccess = false;
	}
	if (SharedCloud.IsValid() &&
		!SharedCloud->ClearSharedFiles())
	{
		UE_LOG(LogOnline, Log, TEXT("Failed to cleanup shared files"));
		bSuccess = false;
	}

	return bSuccess;
}

void FTestCloudInterface::EnumerateUserFiles()
{
	UserCloud->AddOnEnumerateUserFilesCompleteDelegate(EnumerationDelegate);
	UserCloud->EnumerateUserFiles(*UserId);
}

void FTestCloudInterface::OnEnumerateUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& InUserId)
{
	UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
	UE_LOG(LogOnline, Log, TEXT("OnEnumerateUserFilesComplete Success:%d UserId:%s"), bWasSuccessful, *InUserId.ToDebugString());
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	TArray<FCloudFileHeader> UserFiles;
	UserCloud->GetUserFileList(InUserId, UserFiles);
	UserCloud->ClearOnEnumerateUserFilesCompleteDelegate(EnumerationDelegate);

	for (int32 Idx=0; Idx<UserFiles.Num(); Idx++)
	{
		UE_LOG(LogOnline, Log, TEXT("\t%d FileName:%s DLName:%s Hash:%s Size:%d"),
			Idx, *UserFiles[Idx].FileName, *UserFiles[Idx].DLName, *UserFiles[Idx].Hash, UserFiles[Idx].FileSize);
	}

	// Enumeration always advances the test phase
	TestPhase++;
}

void FTestCloudInterface::WriteRandomFile(TArray<uint8>& Buffer, int32 Size)
{
	Buffer.Empty(Size);
	Buffer.AddUninitialized(Size);
	for (int32 i=0; i<Size; i++)
	{
		Buffer[i] = i % 255;
	}
}

void FTestCloudInterface::WriteNUserCloudFiles(const FUniqueNetId& InUserId, const FString& FileNameBase, int32 FileCount, FOnWriteUserFileCompleteDelegate& Delegate)
{
	UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
	UE_LOG(LogOnline, Log, TEXT("Writing %d files to the cloud for user %s"), FileCount, *InUserId.ToDebugString());

	FString FileName(FileNameBase);

	WriteUserCloudFileCount = FileCount;
	UserCloud->AddOnWriteUserFileCompleteDelegate(Delegate);

	TArray<uint8> DummyData;
	for (int32 FileIdx=0; FileIdx<FileCount; FileIdx++)
	{
		// @todo: make the dummy data size configurable on a per-platform basis, since different platforms (e.g. PS4) have different size limits
		//WriteRandomFile(DummyData, FMath::TruncToInt(FMath::FRandRange(1024, 100*1024)));
		WriteRandomFile(DummyData, FMath::TruncToInt(FMath::FRandRange(256, 1024)));
		UserCloud->WriteUserFile(InUserId, FString::Printf(TEXT("%s%d.%s"), *FPaths::GetBaseFilename(FileName), FileIdx, *FPaths::GetExtension(FileName)), DummyData);
	}
}

void FTestCloudInterface::WriteNSharedCloudFiles(const FUniqueNetId& InUserId, const FString& FileNameBase, int32 FileCount, FOnWriteSharedFileCompleteDelegate& Delegate)
{
	UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
	UE_LOG(LogOnline, Log, TEXT("Writing %d files to the cloud and sharing for user %s"), FileCount, *InUserId.ToDebugString());

	FString FileName(FileNameBase);

	WriteSharedCloudFileCount = FileCount;
	SharedCloud->AddOnWriteSharedFileCompleteDelegate(Delegate);

	TArray<uint8> DummyData;
	for (int32 FileIdx=0; FileIdx<FileCount; FileIdx++)
	{
		WriteRandomFile(DummyData, FMath::TruncToInt(FMath::FRandRange(1024, 100*1024)));
		SharedCloud->WriteSharedFile(InUserId, FString::Printf(TEXT("%s%d.%s"), *FPaths::GetBaseFilename(FileName), FileIdx, *FPaths::GetExtension(FileName)), DummyData);
	}
}

void FTestCloudInterface::OnWriteUserCloudFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	UE_LOG(LogOnline, Log, TEXT("Write user file complete Success:%d UserId:%s FileName:%s"), bWasSuccessful, *InUserId.ToDebugString(), *FileName);
	
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	static int32 NumWrittenCount = 0;
	NumWrittenCount++;
	if (NumWrittenCount == WriteUserCloudFileCount)
	{
		UE_LOG(LogOnline, Log, TEXT("Write %d User Files Complete!"), NumWrittenCount);
		UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
		UserCloud->ClearOnWriteUserFileCompleteDelegate(OnWriteUserCloudFileCompleteDelegate);
		NumWrittenCount = 0;
		EnumerateUserFiles();
	}
}

void FTestCloudInterface::ReadEnumeratedUserFiles(FOnReadUserFileCompleteDelegate& Delegate)
{
	TArray<FCloudFileHeader> UserFiles;
	UserCloud->GetUserFileList(*UserId, UserFiles);

	ReadUserFileCount = UserFiles.Num();

	UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
	UE_LOG(LogOnline, Log, TEXT("Reading %d enumerated files for user %s"), ReadUserFileCount, *UserId->ToDebugString());
	if (ReadUserFileCount > 0)
	{
		UserCloud->AddOnReadUserFileCompleteDelegate(Delegate);
		for (int32 Idx=0; Idx < ReadUserFileCount; Idx++)
		{
			UE_LOG(LogOnline, Log, TEXT("\tFileName:%s Size:%d"), *UserFiles[Idx].FileName, UserFiles[Idx].FileSize);
			UserCloud->ReadUserFile(*UserId, UserFiles[Idx].FileName);
		}
	}
	else
	{
		EnumerateUserFiles();
	}
}

void FTestCloudInterface::OnReadEnumeratedUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	int32 FileSize = 0;
	char *buffer = NULL;
	if (bWasSuccessful)
	{
		TArray<uint8> FileContents;
		bWasSuccessful = UserCloud->GetFileContents(InUserId, FileName, FileContents);
		FileSize = FileContents.Num();
		buffer = (char *) FileContents.GetData();
	}

	UE_LOG(LogOnline, Log, TEXT("Read user file complete Success:%d UserId:%s FileName:%s Size:%d"), bWasSuccessful, *InUserId.ToDebugString(), *FileName, FileSize);
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	static int32 NumReadCount = 0;
	NumReadCount++;
	if (NumReadCount == ReadUserFileCount)
	{
		UE_LOG(LogOnline, Log, TEXT("Read %d User Files Complete!"), NumReadCount);
		UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
		UserCloud->ClearOnReadUserFileCompleteDelegate(OnReadEnumeratedUserFilesCompleteDelegate);
		NumReadCount = 0;
		EnumerateUserFiles();
	}
}

void FTestCloudInterface::DeleteEnumeratedUserFiles(bool bCloudDelete, bool bLocalDelete, FOnDeleteUserFileCompleteDelegate& Delegate)
{
	TArray<FCloudFileHeader> UserFiles;
	UserCloud->GetUserFileList(*UserId, UserFiles);

	DeleteUserFileCount = UserFiles.Num();

	UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
	UE_LOG(LogOnline, Log, TEXT("Deleting %d files from the cloud for user %s CLOUD: %s LOCAL: %s"), DeleteUserFileCount, *UserId->ToDebugString(), bCloudDelete ? TEXT("true") : TEXT("false"), bLocalDelete ? TEXT("true") : TEXT("false"));
	if (DeleteUserFileCount > 0)
	{
		UserCloud->AddOnDeleteUserFileCompleteDelegate(Delegate);
		for (int32 Idx=0; Idx < DeleteUserFileCount; Idx++)
		{
			UE_LOG(LogOnline, Log, TEXT("\tFileName:%s Size:%d"), *UserFiles[Idx].FileName, UserFiles[Idx].FileSize);
			UserCloud->DeleteUserFile(*UserId, UserFiles[Idx].FileName, bCloudDelete, bLocalDelete);
		}
	}
	else
	{
		EnumerateUserFiles();
	}
}

void FTestCloudInterface::OnDeleteEnumeratedUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	UE_LOG(LogOnline, Log, TEXT("Delete user file complete Success:%d UserId:%s FileName:%s"), bWasSuccessful, *InUserId.ToDebugString(), *FileName);
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	static int32 NumDeletedCount = 0;
	NumDeletedCount++;
	if (NumDeletedCount == DeleteUserFileCount)
	{
		UE_LOG(LogOnline, Log, TEXT("Delete %d User Files Complete!"), NumDeletedCount);
		UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
		UserCloud->ClearOnDeleteUserFileCompleteDelegate(OnDeleteEnumeratedUserFilesCompleteDelegate);
		NumDeletedCount = 0;
		EnumerateUserFiles();
	}
}

void FTestCloudInterface::OnWriteSharedCloudFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName, const TSharedRef<FSharedContentHandle>& SharedHandle)
{
	UE_LOG(LogOnline, Log, TEXT("Write shared file complete Success:%d UserId:%s FileName:%s SharedHandle:%s"), bWasSuccessful, *InUserId.ToDebugString(), *FileName, *SharedHandle->ToDebugString());
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	CloudFileHandles.Add(SharedHandle);

	static int32 NumWrittenCount = 0;
	NumWrittenCount++;
	if (NumWrittenCount == WriteSharedCloudFileCount)
	{
		UE_LOG(LogOnline, Log, TEXT("Write %d Shared Files Complete!"), NumWrittenCount);
		UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
		SharedCloud->ClearOnWriteSharedFileCompleteDelegate(OnWriteSharedCloudFileCompleteDelegate);
		NumWrittenCount = 0;
		// Enumeration will put the newly shared files into this user's list
		EnumerateUserFiles();
	}
}

void FTestCloudInterface::ReadEnumeratedSharedFiles(bool bUseRandom, FOnReadSharedFileCompleteDelegate& Delegate)
{
	if (bUseRandom)
	{
		ReadSharedFileCount = RandomSharedFileHandles.Num();
	}
	else
	{
		ReadSharedFileCount = CloudFileHandles.Num();
	}
	
	UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
	UE_LOG(LogOnline, Log, TEXT("Reading %d enumerated shared files"), ReadSharedFileCount);
	if (ReadSharedFileCount > 0)
	{
		SharedCloud->AddOnReadSharedFileCompleteDelegate(Delegate);
		for (int32 Idx=0; Idx < ReadSharedFileCount; Idx++)
		{
			if (bUseRandom)
			{
				UE_LOG(LogOnline, Log, TEXT("\tHandle:%s"), *RandomSharedFileHandles[Idx]->ToDebugString());
				SharedCloud->ReadSharedFile(*RandomSharedFileHandles[Idx]);
			}
			else
			{
				UE_LOG(LogOnline, Log, TEXT("\tHandle:%s"), *CloudFileHandles[Idx]->ToDebugString());
				SharedCloud->ReadSharedFile(*CloudFileHandles[Idx]);
			}
		}
	}
	else
	{
		EnumerateUserFiles();
	}
}

void FTestCloudInterface::OnReadEnumeratedSharedFileCompleteDelegate(bool bWasSuccessful, const FSharedContentHandle& SharedHandle)
{
	UE_LOG(LogOnline, Log, TEXT("Read shared file complete Success:%d SharedHandle:%s"), bWasSuccessful, *SharedHandle.ToDebugString());
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	static int32 NumReadCount = 0;
	NumReadCount++;
	if (NumReadCount == ReadSharedFileCount)
	{
		UE_LOG(LogOnline, Log, TEXT("Read %d Shared Files Complete!"), NumReadCount);
		UE_LOG(LogOnline, Log, TEXT("------------------------------------------------"));
		SharedCloud->ClearOnReadSharedFileCompleteDelegate(OnReadEnumerateSharedFileCompleteDelegate);
		NumReadCount = 0;
		EnumerateUserFiles();
	}
}

