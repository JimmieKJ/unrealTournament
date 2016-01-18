// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FExcelBridge
{
public:
	struct Table
	{
		/** Null-separated cell strings */
		const wchar_t* Data;

		/** Total size of cell data in wide chars */
		unsigned long long DataSize;

		/** Number of columns in the table */
		int Width;

		/** Number of rows in the table */
		int Height;
	};

	virtual bool OpenActiveWorkbook() = 0;
	virtual bool OpenWorkbook(const wchar_t* FileName) = 0;
	virtual Table GetTableList() = 0;
	virtual Table ExportTable(const wchar_t* TableName) = 0;
	virtual const wchar_t* GetError() = 0;

protected:
	FExcelBridge() {}
	~FExcelBridge() {}
};

