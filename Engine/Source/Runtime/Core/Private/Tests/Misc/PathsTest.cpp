// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AutomationTest.h"


FString FPaths::GameProjectFilePath;


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathTests, "System.Core.Misc.Paths", EAutomationTestFlags::ATF_SmokeTest)

bool FPathTests::RunTest( const FString& Parameters )
{
	struct FCollapseRelativeDirectoriesTest
	{
		static void Run(FString Path, const TCHAR* Result)
		{
			// Run test
			bool bValid = FPaths::CollapseRelativeDirectories(Path);

			if (Result)
			{
				// If we're looking for a result, make sure it was returned correctly
				check(bValid);
				check(Path == Result);
			}
			else
			{
				// Otherwise, make sure it failed
				check(!bValid);
			}
		}
	};

	FCollapseRelativeDirectoriesTest::Run(TEXT(".."),                                                   NULL);
	FCollapseRelativeDirectoriesTest::Run(TEXT("/.."),                                                  NULL);
	FCollapseRelativeDirectoriesTest::Run(TEXT("./"),                                                   TEXT(""));
	FCollapseRelativeDirectoriesTest::Run(TEXT("./file.txt"),                                           TEXT("file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("/."),                                                   TEXT("/."));
	FCollapseRelativeDirectoriesTest::Run(TEXT("Folder"),                                               TEXT("Folder"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("/Folder"),                                              TEXT("/Folder"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder"),                                            TEXT("C:/Folder"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder/.."),                                         TEXT("C:"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder/../"),                                        TEXT("C:/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder/../file.txt"),                                TEXT("C:/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("Folder/.."),                                            TEXT(""));
	FCollapseRelativeDirectoriesTest::Run(TEXT("Folder/../"),                                           TEXT("/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("Folder/../file.txt"),                                   TEXT("/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("/Folder/.."),                                           TEXT(""));
	FCollapseRelativeDirectoriesTest::Run(TEXT("/Folder/../"),                                          TEXT("/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("/Folder/../file.txt"),                                  TEXT("/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("Folder/../.."),                                         NULL);
	FCollapseRelativeDirectoriesTest::Run(TEXT("Folder/../../"),                                        NULL);
	FCollapseRelativeDirectoriesTest::Run(TEXT("Folder/../../file.txt"),                                NULL);
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/.."),                                                NULL);
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/."),                                                 TEXT("C:/."));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/./"),                                                TEXT("C:/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/./file.txt"),                                        TEXT("C:/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/../Folder2"),                                TEXT("C:/Folder2"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/../Folder2/"),                               TEXT("C:/Folder2/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/../Folder2/file.txt"),                       TEXT("C:/Folder2/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/../Folder2/../.."),                          NULL);
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/../Folder2/../Folder3"),                     TEXT("C:/Folder3"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/../Folder2/../Folder3/"),                    TEXT("C:/Folder3/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/../Folder2/../Folder3/file.txt"),            TEXT("C:/Folder3/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../../Folder3"),                     TEXT("C:/Folder3"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../../Folder3/"),                    TEXT("C:/Folder3/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../../Folder3/file.txt"),            TEXT("C:/Folder3/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4"),          TEXT("C:/Folder4"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4/"),         TEXT("C:/Folder4/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4/file.txt"), TEXT("C:/Folder4/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4"),          TEXT("C:/Folder4"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4/"),         TEXT("C:/Folder4/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4/file.txt"), TEXT("C:/Folder4/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/.././../Folder4"),                   TEXT("C:/Folder4"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/.././../Folder4/"),                  TEXT("C:/Folder4/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/Folder1/Folder2/.././../Folder4/file.txt"),          TEXT("C:/Folder4/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/A/B/.././../C"),                                     TEXT("C:/C"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/A/B/.././../C/"),                                    TEXT("C:/C/"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("C:/A/B/.././../C/file.txt"),                            TEXT("C:/C/file.txt"));
	FCollapseRelativeDirectoriesTest::Run(TEXT(".svn"),                                                 TEXT(".svn"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("/.svn"),                                                TEXT("/.svn"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("./Folder/.svn"),                                        TEXT("Folder/.svn"));
	FCollapseRelativeDirectoriesTest::Run(TEXT("./.svn/../.svn"),                                       TEXT(".svn"));
	FCollapseRelativeDirectoriesTest::Run(TEXT(".svn/./.svn/.././../.svn"),                             TEXT("/.svn"));

	return true;
}
