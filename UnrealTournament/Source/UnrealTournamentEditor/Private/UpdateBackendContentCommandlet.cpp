// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournamentEditor.h"
#include "AssetRegistryModule.h"
#include "Runtime/JsonUtilities/Public/JsonUtilities.h"
#include "UpdateBackendContentCommandlet.h"
#if WITH_PROFILE
#include "CatalogDefinition.h"
#include "MultiLocHelper.h"
#include "LootTables.h"
#endif

DEFINE_LOG_CATEGORY(LogTemplates);

int32 UUpdateBackendContentCommandlet::Main(const FString& FullCommandLine)
{
	UE_LOG(LogTemplates, Log, TEXT("--------------------------------------------------------------------------------------------"));
	UE_LOG(LogTemplates, Log, TEXT("Running BackendTemplateUpdate Commandlet"));
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;
	ParseCommandLine(*FullCommandLine, Tokens, Switches, Params);

	// get the base directory
	FString BaseDirectory = FPaths::GameContentDir() + TEXT("Backend/");
	FString* OutDir = Params.Find(TEXT("OUT"));
	if (OutDir)
	{
		BaseDirectory = *OutDir;
	}

	// export templates
	if (!ExportTemplates(BaseDirectory))
	{
		UE_LOG(LogTemplates, Warning, TEXT("Errors occurred while generating JSON files"));
		return 2; // return something other than 1 for error since the engine will return 1 if any other system (possibly unrelated) logged errors during execution.
	}

	UE_LOG(LogTemplates, Log, TEXT("Successfully finished running BackendTemplateUpdate Commandlet"));
	UE_LOG(LogTemplates, Log, TEXT("--------------------------------------------------------------------------------------------"));
	return 0;
}

typedef TPrettyJsonPrintPolicy<TCHAR> JsonPrintPolicy; // use pretty printing since we're checking these in (results in better diffs)
typedef TSharedRef< TJsonWriter<TCHAR, JsonPrintPolicy > > JsonWriter;
#if WITH_PROFILE
struct FJsonExporter : public TSharedFromThis<FJsonExporter>
{
	FJsonExporter()
	{
	}

	bool Export(const FString& ExportDirectory)
	{
		if (Initialize(ExportDirectory))
		{
			// write everything
			bool bSuccess = true;
			
			// TODO:
			//bSuccess = ExportLootTables() && bSuccess;
			// TODO:
			//bSuccess = WriteCatalogJson() && bSuccess;
			bSuccess = ExportProfileItems() && bSuccess;
			bSuccess = ExportXPTable() && bSuccess;

			UE_LOG(LogTemplates, Log, TEXT("Export Complete"));
			return bSuccess;
		}

		return false;
	}
protected:

	/** Keep track of what loot tables are referenced by what */
	//TMap<FString, TArray<const UUtMcpDefinition*> > LootTableRefs;

	FString BaseDirectory;
	TSet<FString> ExportedTemplates;
	TSet<FString> ValidLootTables;

	FCatalogExporter CatalogExporter;

	bool Initialize(const FString& ExportDirectory)
	{
		// figure out the base directory
		BaseDirectory = ExportDirectory;
		if (!BaseDirectory.EndsWith(TEXT("\\")) && !BaseDirectory.EndsWith(TEXT("/")))
		{
			BaseDirectory.AppendChar(TCHAR('/'));
		}

		// prime localization helper
		CatalogExporter.MultiLocHelper = MakeShareable(new FMultiLocHelper());
		if (!CatalogExporter.MultiLocHelper->LoadAllArchives())
		{
			UE_LOG(LogTemplates, Error, TEXT("Unable to initialize multi-lingual export"));
			return false;
		}
		CatalogExporter.OnIsValidLootTable.BindLambda([this](const FString& LootTableId) {
			return ValidLootTables.Contains(LootTableId);
		});
		CatalogExporter.OnIsValidTemplateId.BindLambda([this](const FString& TemplateId) {
			for (const FString& MtxId : UMcpProfile::MTX_TEMPLATE_IDS)
			{
				if (MtxId.Equals(TemplateId))
				{
					return true;
				}
			}
			return ExportedTemplates.Contains(TemplateId);
		});

		ValidLootTables.Empty();
		return true;
	}
		
	/** String used as cache for Json write. */
	FString JsonOutString;

	/** Current file writing out */
	FString OutFilePath;

	/* Current writer */
	TSharedPtr< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > Json;

	void BeginJson(const FString& FilePath)
	{
		OutFilePath = BaseDirectory + FilePath;
		JsonOutString.Empty();
		Json = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&JsonOutString);
	}
	bool EndJson()
	{
		if (Json->Close())
		{
			FArchive* ItemTemplatesFile = IFileManager::Get().CreateFileWriter(*OutFilePath);
			if (ItemTemplatesFile)
			{
				// serialize the file contents
				TStringConversion<FTCHARToUTF8_Convert> Convert(*JsonOutString);
				ItemTemplatesFile->Serialize(const_cast<ANSICHAR*>(Convert.Get()), Convert.Length());
				ItemTemplatesFile->Close();
				bool bSuccess = !ItemTemplatesFile->IsError();
				delete ItemTemplatesFile;
				if (bSuccess)
				{
					//UE_LOG(LogTemplates, Display, TEXT("Wrote file: %s"), *OutFilePath);
					return true;
				}
				else
				{
					UE_LOG(LogTemplates, Error, TEXT("Unable to write to %s"), *OutFilePath);
				}
			}
			else
			{
				UE_LOG(LogTemplates, Error, TEXT("Unable to open %s for writing."), *OutFilePath);
			}
		}
		else
		{
			UE_LOG(LogTemplates, Error, TEXT("Error closing Json Writer"));
		}
		Json = nullptr;
		return false;
	}

	bool WriteCatalogJson()
	{
		// load the store catalog asset
		static const TCHAR* CATALOG_PATH = TEXT("/Game/Backend/StoreCatalog.StoreCatalog");
		UCatalogDefinition* StoreCatalog = LoadObject<UCatalogDefinition>(nullptr, CATALOG_PATH);
		if (StoreCatalog == nullptr)
		{
			UE_LOG(LogTemplates, Error, TEXT("Missing StoreCatalog object at %s"), CATALOG_PATH);
			return false;
		}

		// load the fulfillments directory
		UObjectLibrary* FulfillmentsLib = UObjectLibrary::CreateLibrary(UCatalogFulfillment::StaticClass(), false, false);
		static const TCHAR* FULFILLMENTS_PATH = TEXT("/Game/GameObjects/GameData");
		TArray<FString> Paths;
		Paths.Add(FULFILLMENTS_PATH);
		FulfillmentsLib->LoadAssetsFromPaths(Paths);
		TArray<const UCatalogFulfillment*> Fulfillments;
		FulfillmentsLib->GetObjects(Fulfillments);

		// export
		UE_LOG(LogTemplates, Display, TEXT("BEGIN EXPORT - Catalog"));
		BeginJson(TEXT("DataTemplates/Catalog.json"));
		FCatalogExporter::JsonWriter JsonRef = Json.ToSharedRef();
		bool bSuccess = CatalogExporter.ExportCatalog(JsonRef, *StoreCatalog, Fulfillments);
		return EndJson() && bSuccess;
	}

	bool IsValidTemplateId(const FString& TemplateId)
	{
		for (const FString& MtxId : UMcpProfile::MTX_TEMPLATE_IDS)
		{
			if (MtxId.Equals(TemplateId))
			{
				return true;
			}
		}
		return ExportedTemplates.Contains(TemplateId);
	}

	bool IsValidTemplateIdOrPath(const FString& TemplateIdOrPath)
	{
		if (TemplateIdOrPath.StartsWith(TEXT("/")))
		{
			return true;
		}

		if (TemplateIdOrPath.StartsWith(TEXT("S.")))
		{
			return true;
		}

		return IsValidTemplateId(TemplateIdOrPath);
	}

	bool ExportLootTables()
	{
		UE_LOG(LogTemplates, Display, TEXT("BEGIN EXPORT - LootTables"));

		// get the loot tables root object
		static const TCHAR* LOOT_TABLES_PATH = TEXT("/Game/GameObjects/GameData/DataTables/Loot/LootTables.LootTables");
		ULootTables* LootTables = LoadObject<ULootTables>(nullptr, LOOT_TABLES_PATH);
		if (LootTables == nullptr)
		{
			UE_LOG(LogTemplates, Error, TEXT("Unable to find ULootTables object at %s"), LOOT_TABLES_PATH);
			return false;
		}

		// bind the template ID check function
		LootTables->OnIsValidTemplateId.BindSP(AsShared(), &FJsonExporter::IsValidTemplateIdOrPath);

		// export
		BeginJson(TEXT("DataTemplates/LootTables.json"));
		ULootTables::FJsonWriter JsonRef = Json.ToSharedRef();
		if (!LootTables->ExportToJson(JsonRef, &ValidLootTables))
		{
			UE_LOG(LogTemplates, Error, TEXT("Unable to export loot tables"));
			return false;
		}
		/*
		// check the table references against the table list we just exported
		bool bSuccess = true;
		for (const auto& ReferencePair : LootTableRefs)
		{
			// see if this is a valid loot table
			if (!ValidLootTables.Contains(ReferencePair.Key))
			{
				for (const auto& RefItem : ReferencePair.Value)
				{
					UE_LOG(LogTemplates, Error, TEXT("Invalid Tier Group %s referenced by %s"), *ReferencePair.Key, *RefItem->GetPathName());
					bSuccess = false;
				}
			}
		}
		*/
		return EndJson();
	}

	bool ExportProfileItems()
	{
		TArray<FAssetData> Assets;
		GetAllAssetData(UUTProfileItem::StaticClass(), Assets, false);

		BeginJson(TEXT("ItemTemplates/ProfileItems.json"));
		Json->WriteArrayStart();
		for (const FAssetData& Asset : Assets)
		{
			UUTProfileItem* Item = Cast<UUTProfileItem>(Asset.GetAsset());
			if (Item != NULL)
			{
				Item->ExportBackendJson(Json.ToSharedRef());
			}
		}
		Json->WriteArrayEnd();
		return EndJson();
	}

	bool ExportXPTable()
	{
		// TODO: currently copied from old UTGameMode code, needs to be moved to an asset
		// note: one based
		TArray<FString> LevelUpRewards;
		LevelUpRewards.AddZeroed(51);
		LevelUpRewards[2] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/BeanieBlack.BeanieBlack"));
		LevelUpRewards[3] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/Sunglasses.Sunglasses"));
		LevelUpRewards[4] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/HockeyMask.HockeyMask"));
		LevelUpRewards[5] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/ThundercrashMale05.ThundercrashMale05"));
		LevelUpRewards[7] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/NecrisMale01.NecrisMale01"));
		LevelUpRewards[8] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/ThundercrashMale03.ThundercrashMale03"));
		LevelUpRewards[10] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/NecrisHelm01.NecrisHelm01"));
		LevelUpRewards[12] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/ThundercrashBeanieGreen.ThundercrashBeanieGreen"));
		LevelUpRewards[14] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/HockeyMask02.HockeyMask02"));
		LevelUpRewards[17] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/ThundercrashMale02.ThundercrashMale02"));
		LevelUpRewards[20] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/NecrisFemale02.NecrisFemale02"));
		LevelUpRewards[23] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/BeanieWhite.BeanieWhite"));
		LevelUpRewards[26] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/NecrisHelm02.NecrisHelm02"));
		LevelUpRewards[30] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/SkaarjMale01.SkaarjMale01"));
		LevelUpRewards[34] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/BeanieGrey.BeanieGrey"));
		LevelUpRewards[39] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/ThundercrashBeanieRed.ThundercrashBeanieRed"));
		LevelUpRewards[40] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/SkaarjMale02.SkaarjMale02"));
		LevelUpRewards[45] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/ThundercrashBeret.ThundercrashBeret"));
		LevelUpRewards[50] = FString(TEXT("/Game/RestrictedAssets/ProfileItems/NecrisMale04.NecrisMale04"));

		TArray<int32> XPTable = GetLevelTable();
		XPTable.Add(-1);
		BeginJson(TEXT("DataTemplates/XPTable.json"));
		Json->WriteArrayStart();
		Json->WriteObjectStart();
		Json->WriteValue(TEXT("templateId"), TEXT("XPTable.Main"));
		Json->WriteObjectStart(TEXT("attributes"));
		Json->WriteArrayStart(TEXT("table"));
		for (int32 i = 0; i < XPTable.Num(); i++)
		{
			Json->WriteObjectStart();
			UUTProfileItem* RewardItem = NULL;
			if (LevelUpRewards.IsValidIndex(i) && !LevelUpRewards[i].IsEmpty())
			{
				RewardItem = LoadObject<UUTProfileItem>(NULL, *LevelUpRewards[i]);
			}
			Json->WriteValue(TEXT("rewardTemplateID"), (RewardItem != NULL) ? RewardItem->GetTemplateID() : TEXT(""));
			Json->WriteValue(TEXT("xpToNext"), XPTable[i]);
			Json->WriteObjectEnd();
		}
		Json->WriteArrayEnd();
		Json->WriteObjectEnd();
		Json->WriteObjectEnd();
		Json->WriteArrayEnd();
		return EndJson();
	}
};
#endif
bool UUpdateBackendContentCommandlet::ExportTemplates(const FString& ExportDir)
{
#if WITH_PROFILE
	FJsonExporter Exporter;
	return Exporter.Export(ExportDir);
#else
	return false;
#endif
}