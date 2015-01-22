// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DataTable.h"
#include "ListenerManager.h"

struct UNREALED_API FDataTableEditorUtils
{
	enum EDataTableChangeInfo
	{
		RowDataPostChangeOnly,
		RowList,
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

	static void BroadcastPreChange(UDataTable* DataTable, EDataTableChangeInfo Info);
	static void BroadcastPostChange(UDataTable* DataTable, EDataTableChangeInfo Info);

	static TArray<UScriptStruct*> GetPossibleStructs();
};