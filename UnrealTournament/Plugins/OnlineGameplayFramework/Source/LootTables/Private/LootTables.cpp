// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LootTablesPCH.h"
#include "LootTables.h"
#include "Runtime/JsonUtilities/Public/JsonUtilities.h"

// this function assumes ExportedTemplates is fully populated
bool ULootTables::IsValidItemReference(FString& ItemRef, bool bAllowBlank) const
{
	// null template IDs are not dangling
	if (ItemRef.IsEmpty() || ItemRef == TEXT("None"))
	{
		return bAllowBlank;
	}

	// see if this is an item definition
	if (OnConvertItemDefToId.IsBound() && ItemRef.StartsWith(TEXT("/")))
	{
		if (!OnConvertItemDefToId.Execute(ItemRef))
		{
			return false;
		}
	}

	// see if it's in ExportedTemplates
	return OnIsValidTemplateId.IsBound() ? OnIsValidTemplateId.Execute(ItemRef) : false;
}

bool ULootTables::CheckDropList(const FString& DropListStr, const TCHAR* KeyName) const
{
	if (DropListStr.IsEmpty())
	{
		UE_LOG(LogLootTables, Error, TEXT("Invalid DropList %s: Empty drop list not allowed"), KeyName);
		return false;
	}

	int32 Index = -1;
	if (!DropListStr.FindChar(';', Index))
	{
		UE_LOG(LogLootTables, Error, TEXT("Invalid DropList %s: \"%s\" does not contain a ;"), KeyName, *DropListStr);
		return false;
	}

	return true;
}

bool ULootTables::ExportToJson(const FJsonWriter& Json, TSet<FString>* ValidLootTablesOut) const
{
	// check the sources for consistency
	for (const FLootTableSource& Source : Sources)
	{
		if (Source.LootTierData != nullptr || Source.LootPackages != nullptr)
		{
			if (Source.LootTierData == nullptr)
			{
				UE_LOG(LogLootTables, Error, TEXT("Source %s has invalid LootTierData table (null)"), *Source.SourceName);
				return false;
			}
			if (!Source.LootTierData->RowStruct || !Source.LootTierData->RowStruct->IsChildOf(FLootTierDataRow::StaticStruct()))
			{
				UE_LOG(LogLootTables, Error, TEXT("Source %s has invalid LootPackages table (not FLootTierDataRow)"), *Source.SourceName);
				return false;
			}
			if (Source.LootPackages == nullptr)
			{
				UE_LOG(LogLootTables, Error, TEXT("Source %s has invalid LootPackages table (null)"), *Source.SourceName);
				return false;
			}
			if (!Source.LootPackages->RowStruct || !Source.LootPackages->RowStruct->IsChildOf(FLootPackageRow::StaticStruct()))
			{
				UE_LOG(LogLootTables, Error, TEXT("Source %s has invalid LootPackages table (not FLootPackageRow)"), *Source.SourceName);
				return false;
			}
		}
		else
		{
			if (Source.LootTierRows.Num() <= 0)
			{
				UE_LOG(LogLootTables, Error, TEXT("Source %s defines no loot tiers"), *Source.SourceName);
				return false;
			}
			if (Source.LootPackageRows.Num() <= 0)
			{
				UE_LOG(LogLootTables, Error, TEXT("Source %s defines no loot packages"), *Source.SourceName);
				return false;
			}
		}
	}

	// start writing JSON
	bool bSuccess = true;
	Json->WriteObjectStart();
	{
		Json->WriteValue(TEXT("templateId"), TEXT("Data.LootTables"));
		Json->WriteObjectStart(TEXT("attributes"));
		{
			// set of defined packages
			TSet<FString> DefinedPackages;

			// write the loot package rows
			Json->WriteArrayStart(TEXT("lootPackage"));
			for (const FLootTableSource& Source : Sources)
			{
				UE_LOG(LogLootTables, Log, TEXT("Exporting loot packages for source %s"), *Source.SourceName);
				TArray<const FLootPackageRow*> PackageRows;
				Source.GetPackageRows(PackageRows);

				for (const FLootPackageRow* PkgStats : PackageRows)
				{
					if (!PkgStats->LootPackageID.IsEmpty())
					{
						// add this to the list of defined packages
						DefinedPackages.Add(PkgStats->LootPackageID);

						// see if this is a recursive entry
						FString ItemDef = PkgStats->ItemDefinition;
						if (PkgStats->LootPackageCall.IsEmpty())
						{
							// validate PkgStats ItemDefinition template ID
							if (!IsValidItemReference(ItemDef, false))
							{
								UE_LOG(LogLootTables, Error, TEXT("Loot Package %s references invalid def %s"), *PkgStats->LootPackageID, *ItemDef);
								bSuccess = false;
							}
						}
						else
						{
							// validate there is no item definition
							if (!ItemDef.IsEmpty() && ItemDef != TEXT("None"))
							{
								UE_LOG(LogLootTables, Error, TEXT("Loot Package %s references both a sub-table AND template ID %s (only one may be set)"), *PkgStats->LootPackageID, *ItemDef);
								bSuccess = false;
							}
						}

						// write out JSON
						Json->WriteObjectStart();
						{
							Json->WriteValue(TEXT("LootPackageID"), PkgStats->LootPackageID);
							Json->WriteValue(TEXT("Weight"), PkgStats->Weight);
							Json->WriteValue(TEXT("NamedWeightMult"), PkgStats->NamedWeightMult);
							Json->WriteValue(TEXT("MinCount"), PkgStats->MinCount);
							Json->WriteValue(TEXT("MaxCount"), PkgStats->MaxCount);
							Json->WriteValue(TEXT("LootPackageCategory"), PkgStats->LootPackageCategory);
							Json->WriteValue(TEXT("LootPackageCall"), PkgStats->LootPackageCall);
							Json->WriteValue(TEXT("ItemDefinition"), ItemDef);
							Json->WriteValue(TEXT("MinWorldLevel"), PkgStats->MinWorldLevel);
							Json->WriteValue(TEXT("MaxWorldLevel"), PkgStats->MaxWorldLevel);
							Json->WriteValue(TEXT("Annotation"), PkgStats->Annotation);
							Json->WriteValue(TEXT("bPreventDuplicateRowSelection"), PkgStats->bPreventDuplicateRowSelection);

							// write any ocean specific columns as item attributes
							if (!PkgStats->ItemAttributesJson.IsEmpty())
							{
								// validate the JSON
								TSharedPtr<FJsonObject> JsonObject;
								TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(PkgStats->ItemAttributesJson);
								if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
								{
									// write the json
									Json->WriteRawJSONValue(TEXT("ItemAttributes"), PkgStats->ItemAttributesJson);
								}
								else
								{
									UE_LOG(LogJson, Warning, TEXT("Unable to parse ItemAttributes on Package %s\n%s"), *PkgStats->LootPackageID, *PkgStats->ItemAttributesJson);
									bSuccess = false;
								}
							}
						}
						Json->WriteObjectEnd();
					}
				}
			}
			Json->WriteArrayEnd();

			// loop over packages again and make sure recursive ones are all defined
			for (const FLootTableSource& Source : Sources)
			{
				TArray<const FLootPackageRow*> PackageRows;
				Source.GetPackageRows(PackageRows);

				for (const FLootPackageRow* PkgStats : PackageRows)
				{
					if (!PkgStats->LootPackageCall.IsEmpty())
					{
						if (!DefinedPackages.Contains(PkgStats->LootPackageCall))
						{
							UE_LOG(LogLootTables, Error, TEXT("Loot Package %s references invalid package %s"), *PkgStats->LootPackageID, *PkgStats->LootPackageCall);
							bSuccess = false;
						}
					}
				}
			}

			// write the tier group rows
			Json->WriteArrayStart(TEXT("lootTier"));
			for (const FLootTableSource& Source : Sources)
			{
				UE_LOG(LogLootTables, Log, TEXT("Exporting loot tiers for source %s"), *Source.SourceName);
				TArray<const FLootTierDataRow*> TierDataRows;
				Source.GetTierRows(TierDataRows);

				for (const FLootTierDataRow* TierStats : TierDataRows)
				{
					if (!TierStats->TierGroup.IsEmpty())
					{
						// validate TierStats->StreakBreakerCurrency template ID
						FString TemplateId = TierStats->StreakBreakerCurrency;
						if (!IsValidItemReference(TemplateId, true))
						{
							UE_LOG(LogLootTables, Error, TEXT("Tier Group %s references invalid template ID %s"), *TierStats->TierGroup, *TemplateId);
							bSuccess = false;
						}

						// make sure the referenced package is defined
						if (!DefinedPackages.Contains(TierStats->LootPackage))
						{
							UE_LOG(LogLootTables, Error, TEXT("Tier Group %s references invalid loot package %s"), *TierStats->TierGroup, *TierStats->LootPackage);
							bSuccess = false;
						}

						// make sure this is in the list of valid loot tables
						if (ValidLootTablesOut != nullptr)
						{
							ValidLootTablesOut->Add(TierStats->TierGroup);
						}

						// check for deprecated dropCount style
						if (!CheckDropList(TierStats->LootPackageCategoryWeight, TEXT("LootPackageCategoryWeight")) ||
							!CheckDropList(TierStats->LootPackageCategoryMin, TEXT("LootPackageCategoryMin")) ||
							!CheckDropList(TierStats->LootPackageCategoryMax, TEXT("LootPackageCategoryMax")))
						{
							bSuccess = false;
						}

						// write out JSON
						Json->WriteObjectStart();
						{
							Json->WriteValue(TEXT("TierGroup"), TierStats->TierGroup);
							Json->WriteValue(TEXT("Weight"), TierStats->Weight);
							Json->WriteValue(TEXT("LootTier"), TierStats->LootTier);
							Json->WriteValue(TEXT("MinWorldLevel"), TierStats->MinWorldLevel);
							Json->WriteValue(TEXT("MaxWorldLevel"), TierStats->MaxWorldLevel);
							Json->WriteValue(TEXT("StreakBreakerCurrency"), TemplateId);
							Json->WriteValue(TEXT("StreakBreakerPointsMin"), TierStats->StreakBreakerPointsMin);
							Json->WriteValue(TEXT("StreakBreakerPointsMax"), TierStats->StreakBreakerPointsMax);
							Json->WriteValue(TEXT("StreakBreakerPointsSpend"), TierStats->StreakBreakerPointsSpend);
							Json->WriteValue(TEXT("LootPackage"), TierStats->LootPackage);
							Json->WriteValue(TEXT("NumLootPackageDrops"), TierStats->NumLootPackageDrops);
							Json->WriteValue(TEXT("LootPackageCategoryWeightRowName"), TierStats->LootPackageCategoryWeight);
							Json->WriteValue(TEXT("LootPackageCategoryMinRowName"), TierStats->LootPackageCategoryMin);
							Json->WriteValue(TEXT("LootPackageCategoryMaxRowName"), TierStats->LootPackageCategoryMax);
							Json->WriteValue(TEXT("Annotation"), TierStats->Annotation);
							Json->WriteValue(TEXT("bPreventDuplicateRowSelection"), TierStats->bPreventDuplicateRowSelection);
						}
						Json->WriteObjectEnd();
					}
				}
			}
			Json->WriteArrayEnd();
		}
		Json->WriteObjectEnd();
	}
	Json->WriteObjectEnd();

	return bSuccess;
}

#if WITH_EDITOR

void ULootTables::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FixupSourceParentLinks();
}

#endif

void ULootTables::FixupSourceParentLinks()
{
	for (FLootTableSource& LootSource : Sources)
	{
		LootSource.Parent = this;
	}
}

void ULootTables::PostLoad()
{
	Super::PostLoad();
	FixupSourceParentLinks();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<typename ROW_TYPE>
static bool DoMappingFromTable(TArray<ROW_TYPE>& OutputArray, const TSharedRef<FTableRecord>& Table)
{
	bool bAllSuccess = true;
	OutputArray.SetNum(Table->Rows.Num());
	TArray<const UProperty*> TierDataColumnMap = Table->MapColumnsToProperties(ROW_TYPE::StaticStruct(), nullptr);
	for (int i = 0; i < Table->Rows.Num(); ++i)
	{
		if (!Table->RowToProperties(i, TierDataColumnMap, &OutputArray[i]))
		{
			bAllSuccess = false;
		}
	}
	return bAllSuccess;
}

FLootTableSource::FLootTableSource()
: LootTierData(nullptr)
, LootPackages(nullptr)
, Parent(nullptr)
{
	ExcelImport.RequiredTables.Add(TEXT("TABLE_EXPORT_LootTierData"));
	ExcelImport.RequiredTables.Add(TEXT("TABLE_EXPORT_LootPackages"));
	ExcelImport.OnImportTables.BindLambda([this](const FExcelImportUtil::TableMap& Tables) {
		// clear out the old datatable pointers
		LootTierData = nullptr;
		LootPackages = nullptr;

		// make the editor mark us as dirty
		if (Parent)
		{
			Parent->PreEditChange(nullptr);
		}

		// load this table data into our array
		TArray<FLootTierDataRow> NewLootTierRows;
		TArray<FLootPackageRow> NewLootPackageRows;
		if (!DoMappingFromTable(NewLootTierRows, Tables.FindChecked(ExcelImport.RequiredTables[0])) ||
			!DoMappingFromTable(NewLootPackageRows, Tables.FindChecked(ExcelImport.RequiredTables[1])))
		{
			return false;
		}

		// apply the change
		LootTierRows = NewLootTierRows;
		LootPackageRows = NewLootPackageRows;

		// get the containing UObject and call PostEditChange
		if (Parent)
		{
			Parent->PostEditChange();
		}
		return true;
	});
}

void FLootTableSource::GetTierRows(TArray<const FLootTierDataRow*>& Rows) const
{
	if (LootTierData != nullptr)
	{
		Rows.Reserve(LootTierData->RowMap.Num());
		for (const auto& Row : LootTierData->RowMap)
		{
			Rows.Add(reinterpret_cast<const FLootTierDataRow*>(Row.Value));
		}
	}
	else
	{
		Rows.Reserve(LootTierRows.Num());
		for (const auto& Row : LootTierRows)
		{
			Rows.Add(&Row);
		}
	}
}

void FLootTableSource::GetPackageRows(TArray<const FLootPackageRow*>& Rows) const
{
	if (LootPackages != nullptr)
	{
		Rows.Reserve(LootPackages->RowMap.Num());
		for (const auto& Row : LootPackages->RowMap)
		{
			Rows.Add(reinterpret_cast<const FLootPackageRow*>(Row.Value));
		}
	}
	else
	{
		Rows.Reserve(LootPackageRows.Num());
		for (const auto& Row : LootPackageRows)
		{
			Rows.Add(&Row);
		}
	}
}
