// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../GenericErrorReport.h"

/**
 * Helper that works with Linux Error Reports
 */
class FLinuxErrorReport : public FGenericErrorReport
{
public:
	/**
	 * Default constructor: creates a report with no files
	 */
	FLinuxErrorReport()
	{
	}

	/**
	 * Discover all files in the crash report directory
	 * @param Directory Full path to directory containing the report
	 */
	explicit FLinuxErrorReport(const FString& Directory)
		: FGenericErrorReport(Directory)
	{
	}

	/**
	 * Do nothing - shouldn't be called on Linux
	 * @return Dummy text
	 */
	FText DiagnoseReport() const	
	{
		return FText::FromString("No local diagnosis on Linux");
	}

	/**
	 * Do nothing - shouldn't be called on Linux
	 * @return Empty string
	 */
	static FString FindMostRecentErrorReport()
	{
		// The report folder is currently always sent on the command-line on Linux
		return TEXT("");
	}
};
