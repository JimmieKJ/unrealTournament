// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "ExcelImportUtil.h"
#include "LootTables.generated.h"

/** Structure representing a data row housing loot drop information */
USTRUCT()
struct LOOTTABLES_API FLootTierDataRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:
	/** Name of the loot tier */
	UPROPERTY(EditAnywhere, Category=LootTier)
	FString TierGroup;

	/** Weight to chance to pick this entry. Sum of weights are added and divided to find actual chance */
	UPROPERTY(EditAnywhere, Category=LootTier)
	float Weight;

	/** What tier the loot is. May be used to affect the look of the container. */
	UPROPERTY(EditAnywhere, Category=LootTier)
	int32 LootTier;

	/** World Level needs to be >= to this value or weight is treated as 0. -1 means don't check */
	UPROPERTY(EditAnywhere, Category=LootTier)
	int32 MinWorldLevel;

	/** World Level needs to be <= to this value or weight is treated as 0. -1 means don't check */
	UPROPERTY(EditAnywhere, Category=LootTier)
	int32 MaxWorldLevel;

	/** Specifies which streak breaker currency is used for this tier row */
	UPROPERTY(EditAnywhere, Category = LootTier)
	FString StreakBreakerCurrency;

	/** Minimum number of points added when this tier row is selected */
	UPROPERTY(EditAnywhere, Category=LootTier)
	int32 StreakBreakerPointsMin;

	/** Maximum number of points added when this tier row is selected */
	UPROPERTY(EditAnywhere, Category=LootTier)
	int32 StreakBreakerPointsMax;

	/** Number of points required to automatically select this tier row, weighted against other rows with a point spend */
	UPROPERTY(EditAnywhere, Category=LootTier)
	int32 StreakBreakerPointsSpend;

	/** Name of the loot package to spawn */
	UPROPERTY(EditAnywhere, Category=LootTier)
	FString LootPackage;

	/** The number of packages to drop. Fractional values will be treated as the probability of a drop */
	UPROPERTY(EditAnywhere, Category=LootTier)
	float NumLootPackageDrops;

	/** colon delimited list of loot package category weights */
	UPROPERTY(EditAnywhere, Category=LootTier)
	FString LootPackageCategoryWeight;

	/** colon delimited list of loot package minimum drops per category */
	UPROPERTY(EditAnywhere, Category=LootTier)
	FString LootPackageCategoryMin;

	/** colon delimited list of loot package maximum drops per category */
	UPROPERTY(EditAnywhere, Category=LootTier)
	FString LootPackageCategoryMax;

	/** Specifies an annotation, which triggers an analytics event */
	UPROPERTY(EditAnywhere, Category = LootTier)
	FString Annotation;

	/** If true, each row in the referenced package will be selected at-most once during this sub-roll */
	UPROPERTY(EditAnywhere, Category = LootTier)
	bool bPreventDuplicateRowSelection;

	FLootTierDataRow()
		: TierGroup()
		, Weight(1.f)
		, LootTier(0)
		, MinWorldLevel(-1)
		, MaxWorldLevel(-1)
		, StreakBreakerCurrency()
		, StreakBreakerPointsMin(0)
		, StreakBreakerPointsMax(0)
		, StreakBreakerPointsSpend(0)
		, LootPackage()
		, NumLootPackageDrops(0.f)
		, LootPackageCategoryWeight()
		, LootPackageCategoryMin()
		, LootPackageCategoryMax()
		, Annotation()
		, bPreventDuplicateRowSelection(false)
	{
	}
};

/** Structure representing a data row housing loot drop information */
USTRUCT()
struct LOOTTABLES_API FLootPackageRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:
	/** Name of the loot package */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	FString LootPackageID;

	/** Weight to chance to pick this entry. Sum of weights are added and divided to find actual chance */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	float Weight;

	/** Named multiplier to skew weights for some package sampling */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	FString NamedWeightMult;

	/** Minimum number of stacks of this item/subpackage to drop. */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	int32 MinCount;

	/** Maximum number of stacks of this item/subpackage to drop. */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	int32 MaxCount;

	/** Category assigned to this loot package. My be used to distinguish quality/rarity/etc. */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	int32 LootPackageCategory;

	/** Name of the loot sub package to drop */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	FString LootPackageCall;

	/** Specific item to drop (Template ID) */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	FString ItemDefinition;

	/** attributes to set on the item dropped by this row */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	FString ItemAttributesJson;

	/** World Level needs to be >= to this value or weight is treated as 0. -1 means don't check */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	int32 MinWorldLevel;

	/** World Level needs to be <= to this value or weight is treated as 0. -1 means don't check */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	int32 MaxWorldLevel;

	/** Specifies an annotation, which triggers an analytics event */
	UPROPERTY(EditAnywhere, Category=LootPackage)
	FString Annotation;

	/** If true, each row in the referenced package will be selected at-most once during this sub-roll */
	UPROPERTY(EditAnywhere, Category = LootPackage)
	bool bPreventDuplicateRowSelection;

	FLootPackageRow()
		: LootPackageID()
		, Weight(1.f)
		, MinCount(1)
		, MaxCount(1)
		, LootPackageCategory(-1)
		, LootPackageCall()
		, ItemDefinition()
		, MinWorldLevel(-1)
		, MaxWorldLevel(-1)
		, Annotation()
		, bPreventDuplicateRowSelection(false)
	{
	}
};

/**
 * Eventually this will represent the raw loot table data from a single xls source.
 */
USTRUCT()
struct LOOTTABLES_API FLootTableSource
{
	GENERATED_USTRUCT_BODY()
public:
	FLootTableSource();

	void GetTierRows(TArray<const FLootTierDataRow*>& Rows) const;

	void GetPackageRows(TArray<const FLootPackageRow*>& Rows) const;

public:

	/** Human tag for identifying where this source comes from */
	UPROPERTY(EditAnywhere)
	FString SourceName;

	/** Import helper */
	UPROPERTY(EditAnywhere)
	FExcelImportUtil ExcelImport;

	/** Loot tiers from this source. DEPRECATED (use Excel import directly) */
	UPROPERTY(EditAnywhere)
	const UDataTable* LootTierData;

	/** Loot packages from this source. DEPRECATED (use Excel import directly) */
	UPROPERTY(EditAnywhere)
	const UDataTable* LootPackages;

	UPROPERTY(VisibleAnywhere)
	TArray<FLootTierDataRow> LootTierRows;

	UPROPERTY(VisibleAnywhere)
	TArray<FLootPackageRow> LootPackageRows;

	UPROPERTY(Transient)
	class ULootTables* Parent;
};

/**
 * Central point of reference for all loot table information for this app.
 */
UCLASS()
class LOOTTABLES_API ULootTables : public UDataAsset
{
	GENERATED_BODY()
public:
	typedef TPrettyJsonPrintPolicy<TCHAR> JsonPrintPolicy; // use pretty printing since we're checking these in (results in better diffs)
	typedef TSharedRef< TJsonWriter<TCHAR, JsonPrintPolicy > > FJsonWriter;

	/** Export loot tables to JSON (for MCP) */
	bool ExportToJson(const FJsonWriter& Json, TSet<FString>* ValidLootTablesOut) const;

	/** Provide bindings for template validation */
	DECLARE_DELEGATE_RetVal_OneParam(bool, ValidateIdCallback, const FString& /* TemplateId */);

	/** Bind this to provide template validation */
	ValidateIdCallback OnIsValidTemplateId;

	/** Provide bindings for item definition (path) translation */
	DECLARE_DELEGATE_RetVal_OneParam(bool, ConvertItemDefToIdCallback, FString& /* ItemRefInOut */);

	/** Bind this to provide item definition translation to templateID */
	ConvertItemDefToIdCallback OnConvertItemDefToId;

public: // UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;

public: // make sure this is not cooked or loaded anywhere other than the editor
	virtual bool NeedsLoadForClient() const override { return false; }
	virtual bool NeedsLoadForEditorGame() const override { return false; }
	virtual bool NeedsLoadForServer() const override { return false; }

private:
	bool IsValidItemReference(FString& ItemRef, bool bAllowBlank) const;
	bool CheckDropList(const FString& DropListStr, const TCHAR* KeyName) const;
	void FixupSourceParentLinks();

public:
	/** Multiple sources may be composed to define loot tables */
	UPROPERTY(EditAnywhere)
	TArray<FLootTableSource> Sources;
};
