// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "FrontendFilters.h"
#include "ISourceControlModule.h"
#include "ObjectTools.h"
#include "AssetToolsModule.h"


/////////////////////////////////////////
// FFrontendFilter_Text
/////////////////////////////////////////

void AssetDataToClassAndNameStrings(FAssetFilterType Asset, OUT TArray< FString >& Array)
{
	Array.Add(Asset.AssetClass.ToString());
	Array.Add(Asset.AssetName.ToString());
}

void AssetDataToNameString(FAssetFilterType Asset, OUT TArray< FString >& Array)
{
	Array.Add(Asset.AssetName.ToString());
}

FFrontendFilter_Text::FFrontendFilter_Text()
	: FFrontendFilter(nullptr)
	, TextFilter(TTextFilter<FAssetFilterType>::FItemToStringArray::CreateStatic(&AssetDataToClassAndNameStrings))
{
	SetIncludeClassName(true);
}

FFrontendFilter_Text::~FFrontendFilter_Text()
{
	TextFilter.OnChanged().RemoveAll(this);
}

bool FFrontendFilter_Text::PassesFilter(FAssetFilterType InItem) const
{
	return TextFilter.PassesFilter(InItem);
}

FText FFrontendFilter_Text::GetRawFilterText() const
{
	return TextFilter.GetRawFilterText();
}

void FFrontendFilter_Text::SetRawFilterText(const FText& InFilterText)
{
	return TextFilter.SetRawFilterText(InFilterText);
}

void FFrontendFilter_Text::SetIncludeClassName(bool bIncludeClassName)
{
	// Preserve the existing text. The filter is going to get recreated here.
	const FText ExistingText = TextFilter.GetRawFilterText();

	if ( bIncludeClassName )
	{
		TextFilter = TTextFilter<FAssetFilterType>::FItemToStringArray::CreateStatic(&AssetDataToClassAndNameStrings);
	}
	else
	{
		TextFilter = TTextFilter<FAssetFilterType>::FItemToStringArray::CreateStatic(&AssetDataToNameString);
	}

	// Apply the existing text before we re-assign the delegate since we want the text preservation to be opaque.
	TextFilter.SetRawFilterText(ExistingText);

	TextFilter.OnChanged().AddRaw(this, &FFrontendFilter_Text::HandleOnChangedEvent);
}

void FFrontendFilter_Text::HandleOnChangedEvent()
{
	BroadcastChangedEvent();
}


/////////////////////////////////////////
// FFrontendFilter_CheckedOut
/////////////////////////////////////////

FFrontendFilter_CheckedOut::FFrontendFilter_CheckedOut(TSharedPtr<FFrontendFilterCategory> InCategory) 
	: FFrontendFilter(InCategory)
{
	
}

void FFrontendFilter_CheckedOut::ActiveStateChanged(bool bActive)
{
	if(bActive)
	{
		RequestStatus();
	}
}

bool FFrontendFilter_CheckedOut::PassesFilter(FAssetFilterType InItem) const
{
	FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(InItem.PackageName.ToString()), EStateCacheUsage::Use);
	return SourceControlState.IsValid() && (SourceControlState->IsCheckedOut() || SourceControlState->IsAdded());
}

void FFrontendFilter_CheckedOut::RequestStatus()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( ISourceControlModule::Get().IsEnabled() )
	{
		// Request the opened files at filter construction time to make sure checked out files have the correct state for the filter
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
		UpdateStatusOperation->SetGetOpenedOnly(true);
		SourceControlProvider.Execute(UpdateStatusOperation, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &FFrontendFilter_CheckedOut::SourceControlOperationComplete) );
	}
}

void FFrontendFilter_CheckedOut::SourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	BroadcastChangedEvent();
}

/////////////////////////////////////////
// FFrontendFilter_Modified
/////////////////////////////////////////

FFrontendFilter_Modified::FFrontendFilter_Modified(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
	, bIsCurrentlyActive(false)
{
	UPackage::PackageDirtyStateChangedEvent.AddRaw(this, &FFrontendFilter_Modified::OnPackageDirtyStateUpdated);
}

FFrontendFilter_Modified::~FFrontendFilter_Modified()
{
	UPackage::PackageDirtyStateChangedEvent.RemoveAll(this);
}

void FFrontendFilter_Modified::ActiveStateChanged(bool bActive)
{
	bIsCurrentlyActive = bActive;
}

bool FFrontendFilter_Modified::PassesFilter(FAssetFilterType InItem) const
{
	UPackage* Package = FindPackage(NULL, *InItem.PackageName.ToString());

	if ( Package != NULL )
	{
		return Package->IsDirty();
	}

	return false;
}

void FFrontendFilter_Modified::OnPackageDirtyStateUpdated(UPackage* Package)
{
	if (bIsCurrentlyActive)
	{
		BroadcastChangedEvent();
	}
}

/////////////////////////////////////////
// FFrontendFilter_ReplicatedBlueprint
/////////////////////////////////////////

bool FFrontendFilter_ReplicatedBlueprint::PassesFilter(FAssetFilterType InItem) const
{
	FString TagValue = InItem.TagsAndValues.FindRef("NumReplicatedProperties");
	return !TagValue.IsEmpty() && (FCString::Atoi(*TagValue) > 0);
}

/////////////////////////////////////////
// FFrontendFilter_ArbitraryComparisonOperation
/////////////////////////////////////////

#define LOCTEXT_NAMESPACE "ContentBrowser"

FFrontendFilter_ArbitraryComparisonOperation::FFrontendFilter_ArbitraryComparisonOperation(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
	, TagName(TEXT("TagName"))
	, TargetTagValue(TEXT("Value"))
	, ComparisonOp(ETextFilterComparisonOperation::NotEqual)
{
}

FString FFrontendFilter_ArbitraryComparisonOperation::GetName() const
{
	return TEXT("CompareTags");
}

FText FFrontendFilter_ArbitraryComparisonOperation::GetDisplayName() const
{
	return FText::Format(LOCTEXT("FFrontendFilter_CompareOperation", "Compare Tags ({0} {1} {2})"),
		FText::FromName(TagName),
		FText::AsCultureInvariant(ConvertOperationToString(ComparisonOp)),
		FText::AsCultureInvariant(TargetTagValue));
}

FText FFrontendFilter_ArbitraryComparisonOperation::GetToolTipText() const
{
	return LOCTEXT("FFrontendFilter_CompareOperation", "Compares AssetRegistrySearchable values on assets with a target value.");
}

bool FFrontendFilter_ArbitraryComparisonOperation::PassesFilter(FAssetFilterType InItem) const
{
	if (const FString* pTagValue = InItem.TagsAndValues.Find(TagName))
	{
		const FString& TestTagValue = *pTagValue;

		const bool bNumericComparison = TestTagValue.IsNumeric() && TargetTagValue.IsNumeric();

		int32 ComparisonSign = 0;
		if (bNumericComparison)
		{
			const double Difference = FCString::Atod(*TestTagValue) - FCString::Atod(*TargetTagValue);
			ComparisonSign = (int32)FMath::Sign(Difference);
		}
		else
		{
			ComparisonSign = TestTagValue.Compare(TargetTagValue, ESearchCase::IgnoreCase);
		}

		bool bComparisonPassed = false;
		switch (ComparisonOp)
		{
		case ETextFilterComparisonOperation::Equal:
			bComparisonPassed = ComparisonSign == 0;
			break;
		case ETextFilterComparisonOperation::NotEqual:
			bComparisonPassed = ComparisonSign != 0;
			break;
		case ETextFilterComparisonOperation::Less:
			bComparisonPassed = ComparisonSign < 0;
			break;
		case ETextFilterComparisonOperation::LessOrEqual:
			bComparisonPassed = ComparisonSign <= 0;
			break;
		case ETextFilterComparisonOperation::Greater:
			bComparisonPassed = ComparisonSign > 0;
			break;
		case ETextFilterComparisonOperation::GreaterOrEqual:
			bComparisonPassed = ComparisonSign >= 0;
			break;
		default:
			check(false);
		};

		return bComparisonPassed;
	}
	else
	{
		// Failed to find the tag, can't pass the filter
		//@TODO: Maybe we should succeed here if the operation is !=
		return false;
	}
}

void FFrontendFilter_ArbitraryComparisonOperation::ModifyContextMenu(FMenuBuilder& MenuBuilder)
{
	FUIAction Action;

	MenuBuilder.BeginSection(TEXT("ComparsionSection"), LOCTEXT("ComparisonSectionHeading", "AssetRegistrySearchable Comparison"));

	TSharedRef<SWidget> KeyWidget =
		SNew(SEditableTextBox)
		.Text_Raw(this, &FFrontendFilter_ArbitraryComparisonOperation::GetKeyValueAsText)
		.OnTextCommitted_Raw(this, &FFrontendFilter_ArbitraryComparisonOperation::OnKeyValueTextCommitted)
		.MinDesiredWidth(100.0f);
	TSharedRef<SWidget> ValueWidget = SNew(SEditableTextBox)
		.Text_Raw(this, &FFrontendFilter_ArbitraryComparisonOperation::GetTargetValueAsText)
		.OnTextCommitted_Raw(this, &FFrontendFilter_ArbitraryComparisonOperation::OnTargetValueTextCommitted)
		.MinDesiredWidth(100.0f);

	MenuBuilder.AddWidget(KeyWidget, LOCTEXT("KeyMenuDesc", "Tag"));
	MenuBuilder.AddWidget(ValueWidget, LOCTEXT("ValueMenuDesc", "Target Value"));

#define UE_SET_COMP_OP(Operation) \
	MenuBuilder.AddMenuEntry(FText::AsCultureInvariant(ConvertOperationToString(Operation)), \
		LOCTEXT("SwitchOpsTooltip", "Switch comparsion type"), \
		FSlateIcon(), \
		FUIAction(FExecuteAction::CreateRaw(this, &FFrontendFilter_ArbitraryComparisonOperation::SetComparisonOperation, Operation), FCanExecuteAction(), FIsActionChecked::CreateRaw(this, &FFrontendFilter_ArbitraryComparisonOperation::IsComparisonOperationEqualTo, Operation)), \
		NAME_None, \
		EUserInterfaceActionType::RadioButton);

	UE_SET_COMP_OP(ETextFilterComparisonOperation::Equal);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::NotEqual);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::Less);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::LessOrEqual);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::Greater);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::GreaterOrEqual);
#undef UE_SET_COMP_OP

	MenuBuilder.EndSection();
}

void FFrontendFilter_ArbitraryComparisonOperation::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".Key")), *TagName.ToString(), IniFilename);
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".Value")), *TargetTagValue, IniFilename);
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".Op")), *FString::FromInt((int32)ComparisonOp), IniFilename);
}

void FFrontendFilter_ArbitraryComparisonOperation::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	FString TagNameAsString;
	if (GConfig->GetString(*IniSection, *(SettingsString + TEXT(".Key")), TagNameAsString, IniFilename))
	{
		TagName = *TagNameAsString;
	}

	GConfig->GetString(*IniSection, *(SettingsString + TEXT(".Value")), TargetTagValue, IniFilename);

	int32 OpAsInteger;
	if (GConfig->GetInt(*IniSection, *(SettingsString + TEXT(".Op")), OpAsInteger, IniFilename))
	{
		ComparisonOp = (ETextFilterComparisonOperation)OpAsInteger;
	}
}

void FFrontendFilter_ArbitraryComparisonOperation::SetComparisonOperation(ETextFilterComparisonOperation NewOp)
{
	ComparisonOp = NewOp;
	BroadcastChangedEvent();
}

bool FFrontendFilter_ArbitraryComparisonOperation::IsComparisonOperationEqualTo(ETextFilterComparisonOperation TestOp) const
{
	return (ComparisonOp == TestOp);
}

FText FFrontendFilter_ArbitraryComparisonOperation::GetKeyValueAsText() const
{
	return FText::FromName(TagName);
}

FText FFrontendFilter_ArbitraryComparisonOperation::GetTargetValueAsText() const
{
	return FText::AsCultureInvariant(TargetTagValue);
}

void FFrontendFilter_ArbitraryComparisonOperation::OnKeyValueTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if (!InText.IsEmpty())
	{
		TagName = *InText.ToString();
		BroadcastChangedEvent();
	}
}

void FFrontendFilter_ArbitraryComparisonOperation::OnTargetValueTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	TargetTagValue = InText.ToString();
	BroadcastChangedEvent();
}

FString FFrontendFilter_ArbitraryComparisonOperation::ConvertOperationToString(ETextFilterComparisonOperation Op)
{
	switch (Op)
	{
	case ETextFilterComparisonOperation::Equal:
		return TEXT("==");
	case ETextFilterComparisonOperation::NotEqual:
		return TEXT("!=");
	case ETextFilterComparisonOperation::Less:
		return TEXT("<");
	case ETextFilterComparisonOperation::LessOrEqual:
		return TEXT("<=");
	case ETextFilterComparisonOperation::Greater:
		return TEXT(">");
	case ETextFilterComparisonOperation::GreaterOrEqual:
		return TEXT(">=");
	default:
		check(false);
		return TEXT("op");
	};
}

#undef LOCTEXT_NAMESPACE

/////////////////////////////////////////
// FFrontendFilter_ShowOtherDevelopers
/////////////////////////////////////////

FFrontendFilter_ShowOtherDevelopers::FFrontendFilter_ShowOtherDevelopers(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
	, BaseDeveloperPath(FPackageName::FilenameToLongPackageName(FPaths::GameDevelopersDir()))
	, UserDeveloperPath(FPackageName::FilenameToLongPackageName(FPaths::GameUserDeveloperDir()))
	, bIsOnlyOneDeveloperPathSelected(false)
	, bShowOtherDeveloperAssets(false)
{
}

void FFrontendFilter_ShowOtherDevelopers::SetCurrentFilter(const FARFilter& InFilter)
{
	if ( InFilter.PackagePaths.Num() == 1 )
	{
		const FString PackagePath = InFilter.PackagePaths[0].ToString() + TEXT("/");
		
		// If the path starts with the base developer path, and is not the path itself then only one developer path is selected
		bIsOnlyOneDeveloperPathSelected = PackagePath.StartsWith(BaseDeveloperPath) && PackagePath.Len() != BaseDeveloperPath.Len();
	}
	else
	{
		// More or less than one path is selected
		bIsOnlyOneDeveloperPathSelected = false;
	}
}

bool FFrontendFilter_ShowOtherDevelopers::PassesFilter(FAssetFilterType InItem) const
{
	// Pass all assets if other developer assets are allowed
	if ( !bShowOtherDeveloperAssets )
	{
		// Never hide developer assets when a single developer folder is selected.
		if ( !bIsOnlyOneDeveloperPathSelected )
		{
			// If selecting multiple folders, the Developers folder/parent folder, or "All Assets", hide assets which are found in the development folder unless they are in the current user's folder
			const FString PackagePath = InItem.PackagePath.ToString() + TEXT("/");
			const bool bPackageInDeveloperFolder = PackagePath.StartsWith(BaseDeveloperPath) && PackagePath.Len() != BaseDeveloperPath.Len();

			if ( bPackageInDeveloperFolder )
			{
				const bool bPackageInUserDeveloperFolder = PackagePath.StartsWith(UserDeveloperPath);
				if ( !bPackageInUserDeveloperFolder )
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FFrontendFilter_ShowOtherDevelopers::SetShowOtherDeveloperAssets(bool bValue)
{
	if ( bShowOtherDeveloperAssets != bValue )
	{
		bShowOtherDeveloperAssets = bValue;
		BroadcastChangedEvent();
	}
}

bool FFrontendFilter_ShowOtherDevelopers::GetShowOtherDeveloperAssets() const
{
	return bShowOtherDeveloperAssets;
}

/////////////////////////////////////////
// FFrontendFilter_ShowRedirectors
/////////////////////////////////////////

FFrontendFilter_ShowRedirectors::FFrontendFilter_ShowRedirectors(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
{
	bAreRedirectorsInBaseFilter = false;
	RedirectorClassName = UObjectRedirector::StaticClass()->GetFName();
}

void FFrontendFilter_ShowRedirectors::SetCurrentFilter(const FARFilter& InFilter)
{
	bAreRedirectorsInBaseFilter = InFilter.ClassNames.Contains(RedirectorClassName);
}

bool FFrontendFilter_ShowRedirectors::PassesFilter(FAssetFilterType InItem) const
{
	// Never hide redirectors if they are explicitly searched for
	if ( !bAreRedirectorsInBaseFilter )
	{
		return InItem.AssetClass != RedirectorClassName;
	}

	return true;
}

/////////////////////////////////////////
// FFrontendFilter_InUseByLoadedLevels
/////////////////////////////////////////

FFrontendFilter_InUseByLoadedLevels::FFrontendFilter_InUseByLoadedLevels(TSharedPtr<FFrontendFilterCategory> InCategory) 
	: FFrontendFilter(InCategory)
	, bIsCurrentlyActive(false)
{
	FEditorDelegates::MapChange.AddRaw(this, &FFrontendFilter_InUseByLoadedLevels::OnEditorMapChange);

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.OnAssetPostRename().AddRaw(this, &FFrontendFilter_InUseByLoadedLevels::OnAssetPostRename);
}

FFrontendFilter_InUseByLoadedLevels::~FFrontendFilter_InUseByLoadedLevels()
{
	FEditorDelegates::MapChange.RemoveAll(this);

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.OnAssetPostRename().RemoveAll(this);
}

void FFrontendFilter_InUseByLoadedLevels::ActiveStateChanged( bool bActive )
{
	bIsCurrentlyActive = bActive;

	if ( bActive )
	{
		ObjectTools::TagInUseObjects(ObjectTools::SO_LoadedLevels);
	}
}

void FFrontendFilter_InUseByLoadedLevels::OnAssetPostRename(const TArray<FAssetRenameData>& AssetsAndNames)
{
	// Update the tags identifying objects currently used by loaded levels
	ObjectTools::TagInUseObjects(ObjectTools::SO_LoadedLevels);
}

bool FFrontendFilter_InUseByLoadedLevels::PassesFilter(FAssetFilterType InItem) const
{
	bool bObjectInUse = false;
	
	if ( InItem.IsAssetLoaded() )
	{
		UObject* Asset = InItem.GetAsset();

		const bool bUnreferenced = !Asset->HasAnyMarks( OBJECTMARK_TagExp );
		const bool bIndirectlyReferencedObject = Asset->HasAnyMarks( OBJECTMARK_TagImp );
		const bool bRejectObject =
			Asset->GetOuter() == NULL || // Skip objects with null outers
			Asset->HasAnyFlags( RF_Transient ) || // Skip transient objects (these shouldn't show up in the CB anyway)
			Asset->IsPendingKill() || // Objects that will be garbage collected 
			bUnreferenced || // Unreferenced objects 
			bIndirectlyReferencedObject; // Indirectly referenced objects

		if( !bRejectObject && Asset->HasAnyFlags( RF_Public ) )
		{
			// The object is in use 
			bObjectInUse = true;
		}
	}

	return bObjectInUse;
}

void FFrontendFilter_InUseByLoadedLevels::OnEditorMapChange( uint32 MapChangeFlags )
{
	if ( MapChangeFlags == MapChangeEventFlags::NewMap && bIsCurrentlyActive )
	{
		ObjectTools::TagInUseObjects(ObjectTools::SO_LoadedLevels);
		BroadcastChangedEvent();
	}
}