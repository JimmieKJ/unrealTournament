// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Commandlets/Commandlet.h"
#include "GenerateTextLocalizationReportCommandlet.generated.h"


UCLASS()
class UGenerateTextLocalizationReportCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()



public:
	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface

private:

	/**
	 * Calculates the number of words found in a given string.
	 * @param Text - The string to examine.
	 * @return Number of words found.
	 */
	int32 CountWords( const FString& Text ) const;

	/**
	 * Collects word count info and writes out a report.
	 * @param SourcePath - Source path to localization files.
	 * @param DestinationPath - The destination folder the report will be write to.
	 * @return True if successful; false oterwise.
	 */
	bool ProcessWordCountReport(const FString& SourcePath, const FString& DestinationPath);

	/**
	 * Writes out a report representing localization conflicts.  These are conflicts where the namespace
	 * and key match but the source text differs.
	 * @param DestinationPath - The destination folder the report will be write to.
	 * @return True if successful; false oterwise.
	 */
	bool ProcessConflictReport(const FString& DestinationPath);

private:
	FString GatherTextConfigPath;
	FString SectionName;
	FString CmdlineTimeStamp;

};