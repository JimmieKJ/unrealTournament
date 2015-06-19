// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DataTableFunctionLibrary.generated.h"

class UCurveTable;

/** Enum used to indicate success or failure of EvaluateCurveTableRow */
UENUM()
namespace EEvaluateCurveTableResult
{
    enum Type
    {
        /** Found the row successfully */
        RowFound,
        /** Failed to find row */
        RowNotFound,
    };
}

UCLASS()
class ENGINE_API UDataTableFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "DataTable", meta = (ExpandEnumAsExecs="OutResult", DataTablePin="CurveTable"))
	static void EvaluateCurveTableRow(UCurveTable* CurveTable, FName RowName, float InXY, TEnumAsByte<EEvaluateCurveTableResult::Type>& OutResult, float& OutXY);
    
	UFUNCTION(BlueprintCallable, Category = "DataTable")
	static void GetDataTableRowNames(UDataTable* Table, TArray<FName>& OutRowNames);

    /** Get a Row from a DataTable given a RowName */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "DataTable", meta=(CustomStructureParam = "OutRow", BlueprintInternalUseOnly="true"))
    static bool GetDataTableRowFromName(UDataTable* Table, FName RowName, FTableRowBase& OutRow);
    
	static bool Generic_GetDataTableRowFromName(UDataTable* Table, FName RowName, void* OutRowPtr);

    /** Based on UDataTableFunctionLibrary::GetDataTableRow */
    DECLARE_FUNCTION(execGetDataTableRowFromName)
    {
        P_GET_OBJECT(UDataTable, Table);
        P_GET_PROPERTY(UNameProperty, RowName);
        
        Stack.StepCompiledIn<UStructProperty>(NULL);
        void* OutRowPtr = Stack.MostRecentPropertyAddress;
        
        P_FINISH;
        
		*(bool*)RESULT_PARAM = Generic_GetDataTableRowFromName(Table, RowName, OutRowPtr);
    }
};
