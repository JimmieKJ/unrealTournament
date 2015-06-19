// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DataTable.h"
#include "ListenerManager.h"

struct FDataTableEditorColumnHeaderData
{
	/** Unique ID used to identify this column */
	FName ColumnId;

	/** Display name of this column */
	FText DisplayName;

	/** The calculated width of this column taking into account the cell data for each row */
	float DesiredColumnWidth;
};

struct FDataTableEditorRowListViewData
{
	/** Unique ID used to identify this row */
	FName RowId;

	/** Display name of this row */
	FText DisplayName;

	/** Array corresponding to each cell in this row */
	TArray<FText> CellData;
};

typedef TSharedPtr<FDataTableEditorColumnHeaderData> FDataTableEditorColumnHeaderDataPtr;
typedef TSharedPtr<FDataTableEditorRowListViewData>  FDataTableEditorRowListViewDataPtr;

struct UNREALED_API FDataTableEditorUtils
{
	enum class EDataTableChangeInfo
	{
		/** The data corresponding to a single row has been changed */
		RowData,
		/** The data corresponding to the entire list of rows has been changed */
		RowList,
	};

	enum class ERowMoveDirection
	{
		Up,
		Down,
	};

	class FDataTableEditorManager : public FListenerManager < UDataTable, EDataTableChangeInfo >
	{
		FDataTableEditorManager() {}
	public:
		UNREALED_API static FDataTableEditorManager& Get();

		class UNREALED_API ListenerType : public InnerListenerType<FDataTableEditorManager>
		{
		};
	};

	typedef FDataTableEditorManager::ListenerType INotifyOnDataTableChanged;

	static bool RemoveRow(UDataTable* DataTable, FName Name);
	static uint8* AddRow(UDataTable* DataTable, FName RowName);
	static bool RenameRow(UDataTable* DataTable, FName OldName, FName NewName);
	static bool MoveRow(UDataTable* DataTable, FName RowName, ERowMoveDirection Direction, int32 NumRowsToMoveBy = 1);

	static void BroadcastPreChange(UDataTable* DataTable, EDataTableChangeInfo Info);
	static void BroadcastPostChange(UDataTable* DataTable, EDataTableChangeInfo Info);

	static void CacheDataTableForEditing(const UDataTable* DataTable, TArray<FDataTableEditorColumnHeaderDataPtr>& OutAvailableColumns, TArray<FDataTableEditorRowListViewDataPtr>& OutAvailableRows);

	static TArray<UScriptStruct*> GetPossibleStructs();
};