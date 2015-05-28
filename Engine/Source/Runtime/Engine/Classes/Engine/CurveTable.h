// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveTable.generated.h"


ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogCurveTable, Log, All);


// forward declare JSON writer
template <class CharType>
struct TPrettyJsonPrintPolicy;
template <class CharType, class PrintPolicy>
class TJsonWriter;


/**
 * Imported spreadsheet table as curves.
 */
UCLASS(MinimalAPI)
class UCurveTable
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** The filename that was used to create this object. Relative to the object's package, BaseDir() or absolute */
	UPROPERTY()
	FString ImportPath;


	/** Map of name of row to row data structure. */
	TMap<FName, FRichCurve*>	RowMap;

	// Begin UObject interface.
	virtual void FinishDestroy() override;
	virtual void Serialize( FArchive& Ar ) override;
#if WITH_EDITORONLY_DATA
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif
	// End  UObject interface

	// Begin UCurveTable interface

	/** Function to find the row of a table given its name. */
	FRichCurve* FindCurve(FName RowName, const FString& ContextString, bool WarnIfNotFound=true) const
	{
		if(RowName == NAME_None)
		{
			UE_CLOG(WarnIfNotFound, LogCurveTable, Warning, TEXT("UCurveTable::FindRow : NAME_None is invalid row name for CurveTable '%s' (%s)."), *GetPathName(), *ContextString);
			return nullptr;
		}

		FRichCurve* const* FoundCurve = RowMap.Find(RowName);

		if(FoundCurve == nullptr)
		{
			UE_CLOG(WarnIfNotFound, LogCurveTable, Warning, TEXT("UCurveTable::FindRow : Row '%s' not found in CurveTable '%s' (%s)."), *RowName.ToString(), *GetPathName(), *ContextString);
			return nullptr;
		}

		return (FRichCurve*)*FoundCurve;
	}

	/** Output entire contents of table as a string */
	ENGINE_API FString GetTableAsString() const;

	/** Output entire contents of table as CSV */
	ENGINE_API FString GetTableAsCSV() const;

	/** Output entire contents of table as JSON */
	ENGINE_API FString GetTableAsJSON() const;

	/** Output entire contents of table as JSON */
	ENGINE_API bool WriteTableAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter) const;

	/** 
	 *	Create table from CSV style comma-separated string. 
	 *	RowCurve must be defined before calling this function. 
	 *  @param InString The string representing the CurveTable
	 *  @param InterpMode The mode of interpolation to use for the curves
	 *	@return	Set of problems encountered while processing input
	 */
	ENGINE_API TArray<FString> CreateTableFromCSVString(const FString& InString, ERichCurveInterpMode InterpMode = RCIM_Linear);

	/** 
	 *	Create table from JSON string. 
	 *	RowCurve must be defined before calling this function. 
	 *  @param InString The string representing the CurveTable
	 *  @param InterpMode The mode of interpolation to use for the curves
	 *	@return	Set of problems encountered while processing input
	 */
	ENGINE_API TArray<FString> CreateTableFromJSONString(const FString& InString, ERichCurveInterpMode InterpMode = RCIM_Linear);

	/** Empty the table info (will not clear RowCurve) */
	ENGINE_API void EmptyTable();

protected:
	/** Util that removes invalid chars and then make an FName
	 * @param InString The string to create a valid name from
	 * @return A valid name from InString
	 */
	static FName MakeValidName(const FString& InString);

};


/**
 * Handle to a particular row in a table.
 */
USTRUCT(BlueprintType)
struct ENGINE_API FCurveTableRowHandle
{
	GENERATED_USTRUCT_BODY()

	FCurveTableRowHandle()
		: CurveTable(nullptr)
		, RowName(NAME_None)
	{ }

	/** Pointer to table we want a row from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CurveTableRowHandle)
	const class UCurveTable*	CurveTable;

	/** Name of row in the table that we want */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CurveTableRowHandle)
	FName				RowName;

	/** Returns true if the curve is valid */
	bool IsValid() const
	{
		return (GetCurve() != nullptr);
	}

	static const FString Unknown;

	/** Get the curve straight from the row handle */
	FRichCurve* GetCurve(const FString& ContextString = Unknown) const;

	/** Evaluate the curve if it is valid
	 * @param XValue The input X value to the curve
	 * @return The value of the curve if valid, 0 if not
	 */
	float Eval(float XValue) const;

	/** Evaluate the curve if it is valid
	 * @param XValue The input X value to the curve
	 * @param YValue The output Y value from the curve
	 * @return True if it filled out YValue with a valid number, false otherwise
	 */
	bool Eval(float XValue, float* YValue) const;

	bool operator==(const FCurveTableRowHandle& Other) const;
	bool operator!=(const FCurveTableRowHandle& Other) const;
};


/** Macro to call GetCurve with a correct error info. Assumed to be called within a UObject */
#define GETCURVE_REPORTERROR(Handle) Handle.GetCurve(FString::Printf(TEXT("%s.%s"), *GetPathName(), TEXT(#Handle)))
