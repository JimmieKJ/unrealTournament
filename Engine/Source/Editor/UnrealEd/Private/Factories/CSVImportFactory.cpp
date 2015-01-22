// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "MainFrame.h"
#include "ModuleManager.h"
#include "DirectoryWatcherModule.h"
#include "../../../DataTableEditor/Public/IDataTableEditor.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "Engine/UserDefinedStruct.h"
#include "DataTableEditorUtils.h"
DEFINE_LOG_CATEGORY(LogCSVImportFactory);

#define LOCTEXT_NAMESPACE "CSVImportFactory"

/** Enum to indicate what to import CSV as */
enum ECSVImportType
{
	/** Import as UDataTable */
	ECSV_DataTable,
	/** Import as UCurveTable */
	ECSV_CurveTable,
	/** Import as a UCurveFloat */
	ECSV_CurveFloat,
	/** Import as a UCurveVector */
	ECSV_CurveVector,
	/** Import as a UCurveLinearColor */
	ECSV_CurveLinearColor,
};

/** UI to pick options when importing data table */
class SCSVImportOptions : public SCompoundWidget
{
private:
	/** Whether we should go ahead with import */
	bool										bImport;

	/** Window that owns us */
	TWeakPtr< SWindow >							WidgetWindow;

	// Import type

	/** List of import types to pick from, drives combo box */
	TArray< TSharedPtr<ECSVImportType> >						ImportTypes;

	/** The combo box */
	TSharedPtr< SComboBox< TSharedPtr<ECSVImportType> > >		ImportTypeCombo;

	/** Indicates what kind of asset we want to make from the CSV file */
	ECSVImportType												SelectedImportType;


	// Row type

	/** Array of row struct options */
	TArray< UScriptStruct* >						RowStructs;

	/** The row struct combo box */
	TSharedPtr< SComboBox<UScriptStruct*> >			RowStructCombo;

	/** The selected row struct */
	UScriptStruct*									SelectedStruct;

	/** Typedef for curve enum pointers */
	typedef TSharedPtr<ERichCurveInterpMode>		CurveInterpModePtr;

	/** The curve interpolation combo box */
	TSharedPtr< SComboBox<CurveInterpModePtr> >		CurveInterpCombo;

	/** All available curve interpolation modes */
	TArray< CurveInterpModePtr >					CurveInterpModes;

	/** The selected curve interpolation type */
	ERichCurveInterpMode							SelectedCurveInterpMode;

public:
	SLATE_BEGIN_ARGS( SCSVImportOptions ) 
		: _WidgetWindow()
		{}

		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
	SLATE_END_ARGS()

	SCSVImportOptions()
	: bImport(false)
	, SelectedImportType(ECSV_DataTable)
	, SelectedStruct(NULL)
	{}

	void Construct( const FArguments& InArgs )
	{
		WidgetWindow = InArgs._WidgetWindow;

		// Make array of enum pointers
		TSharedPtr<ECSVImportType> DataTableTypePtr = MakeShareable(new ECSVImportType(ECSV_DataTable));
		ImportTypes.Add( DataTableTypePtr );
		ImportTypes.Add( MakeShareable(new ECSVImportType(ECSV_CurveTable)) );
		ImportTypes.Add( MakeShareable(new ECSVImportType(ECSV_CurveFloat)) );
		ImportTypes.Add( MakeShareable(new ECSVImportType(ECSV_CurveVector)) );

		// Find table row struct info
		RowStructs = FDataTableEditorUtils::GetPossibleStructs();

		// Create widget
		this->ChildSlot
		[
			SNew(SBorder)
			. BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
			. Padding(10)
			[
				SNew(SVerticalBox)
				// Import type
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text( LOCTEXT("ChooseAssetType", "Import As:") )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(ImportTypeCombo, SComboBox< TSharedPtr<ECSVImportType> >)
					.OptionsSource( &ImportTypes )
					.OnGenerateWidget( this, &SCSVImportOptions::MakeImportTypeItemWidget )
					[
						SNew(STextBlock)
						.Text(this, &SCSVImportOptions::GetSelectedItemText)
					]
				]
				// Data row struct
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text( LOCTEXT("ChooseRowType", "Choose DataTable Row Type:") )
					.Visibility( this, &SCSVImportOptions::GetTableRowOptionVis )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(RowStructCombo, SComboBox<UScriptStruct*>)
					.OptionsSource( &RowStructs )
					.OnGenerateWidget( this, &SCSVImportOptions::MakeRowStructItemWidget )
					.Visibility( this, &SCSVImportOptions::GetTableRowOptionVis )
					[
						SNew(STextBlock)
						.Text(this, &SCSVImportOptions::GetSelectedRowOptionText)
					]
				]
				// Curve interpolation
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text( LOCTEXT("ChooseCurveType", "Choose Curve Interpolation Type:") )
					.Visibility( this, &SCSVImportOptions::GetCurveTypeVis )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(CurveInterpCombo, SComboBox<CurveInterpModePtr>)
					.OptionsSource( &CurveInterpModes )
					.OnGenerateWidget( this, &SCSVImportOptions::MakeCurveTypeWidget )
					.Visibility( this, &SCSVImportOptions::GetCurveTypeVis )
					[
						SNew(STextBlock)
						.Text(this, &SCSVImportOptions::GetSelectedCurveTypeText)
					]
				]
				// Ok/Cancel
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("OK", "OK"))
						.OnClicked( this, &SCSVImportOptions::OnImport )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked( this, &SCSVImportOptions::OnCancel )
					]
				]
			]
		];

		// set-up selection
		ImportTypeCombo->SetSelectedItem(DataTableTypePtr);

		// Populate the valid interploation modes
		{
			CurveInterpModes.Add( MakeShareable( new ERichCurveInterpMode(ERichCurveInterpMode::RCIM_Constant) ) );
			CurveInterpModes.Add( MakeShareable( new ERichCurveInterpMode(ERichCurveInterpMode::RCIM_Linear) ) );
			CurveInterpModes.Add( MakeShareable( new ERichCurveInterpMode(ERichCurveInterpMode::RCIM_Cubic) ) );
		}

		// NB: Both combo boxes default to first item in their options lists as initially selected item
	}

	/** If we should import */
	bool ShouldImport()
	{
		return ((SelectedStruct != NULL) || GetSelectedImportType() != ECSV_DataTable) && bImport;
	}

	/** Get the row struct we selected */
	UScriptStruct* GetSelectedRowStruct()
	{
		return SelectedStruct;
	}

	/** Get the import type we selected */
	ECSVImportType GetSelectedImportType()
	{
		return SelectedImportType;
	}

	/** Get the interpolation mode we selected */
	ERichCurveInterpMode GetSelectedCurveIterpMode()
	{
		return SelectedCurveInterpMode;
	}
	
	/** Whether to show table row options */
	EVisibility GetTableRowOptionVis() const
	{
		return (ImportTypeCombo.IsValid() && *ImportTypeCombo->GetSelectedItem() == ECSV_DataTable) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	/** Whether to show table row options */
	EVisibility GetCurveTypeVis() const
	{
		return (ImportTypeCombo.IsValid() && *ImportTypeCombo->GetSelectedItem() == ECSV_CurveTable) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FString GetImportTypeText(TSharedPtr<ECSVImportType> Type ) const
	{
		FString EnumString;
		if(*Type == ECSV_DataTable)
		{
			EnumString = TEXT("DataTable");
		}
		else if(*Type == ECSV_CurveTable)
		{
			EnumString = TEXT("CurveTable");
		}
		else if(*Type == ECSV_CurveFloat)
		{
			EnumString = TEXT("Float Curve");
		}
		else if(*Type == ECSV_CurveVector)
		{
			EnumString = TEXT("Vector Curve");
		}
		return EnumString;
	}

	/** Called to create a widget for each struct */
	TSharedRef<SWidget> MakeImportTypeItemWidget( TSharedPtr<ECSVImportType> Type )
	{
		return	SNew(STextBlock)
				.Text(GetImportTypeText(Type));
	}

	/** Called to create a widget for each struct */
	TSharedRef<SWidget> MakeRowStructItemWidget( UScriptStruct* Struct )
	{
		check( Struct != NULL );
		return	SNew(STextBlock)
				.Text(Struct->GetName());
	}

	FString GetCurveTypeText (CurveInterpModePtr InterpMode) const
	{
		FString EnumString;

		switch(*InterpMode)
		{
			case ERichCurveInterpMode::RCIM_Constant : 
				EnumString = TEXT("Constant");
				break;

			case ERichCurveInterpMode::RCIM_Linear : 
				EnumString = TEXT("Linear");
				break;

			case ERichCurveInterpMode::RCIM_Cubic : 
				EnumString = TEXT("Cubic");
				break;
		}
		return EnumString;
	}

	/** Called to create a widget for each curve interpolation enum */
	TSharedRef<SWidget> MakeCurveTypeWidget( CurveInterpModePtr InterpMode )
	{
		FString Label = GetCurveTypeText(InterpMode);
		return SNew(STextBlock) .Text( Label );
	}

	/** Called when 'OK' button is pressed */
	FReply OnImport()
	{
		SelectedStruct = RowStructCombo->GetSelectedItem();
		SelectedImportType = *ImportTypeCombo->GetSelectedItem();
		if(CurveInterpCombo->GetSelectedItem().IsValid())
		{
			SelectedCurveInterpMode = *CurveInterpCombo->GetSelectedItem();
		}
		bImport = true;
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	/** Called when 'Cancel' button is pressed */
	FReply OnCancel()
	{
		bImport = false;
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FString GetSelectedItemText() const
	{
		TSharedPtr<ECSVImportType> SelectedType = ImportTypeCombo->GetSelectedItem();

		return (SelectedType.IsValid())
			? GetImportTypeText(SelectedType)
			: FString();
	}

	FString GetSelectedRowOptionText() const
	{
		UScriptStruct* SelectedScript = RowStructCombo->GetSelectedItem();
		return (SelectedScript)
			? SelectedScript->GetName()
			: FString();
	}

	FString GetSelectedCurveTypeText() const
	{
		CurveInterpModePtr CurveModePtr = CurveInterpCombo->GetSelectedItem();
		return (CurveModePtr.IsValid())
			? GetCurveTypeText(CurveModePtr)
			: FString();
	}
};

//////////////////////////////////////////////////////////////////////////

static UClass* GetCurveClass( ECSVImportType ImportType )
{
	switch( ImportType )
	{
	case ECSV_CurveFloat:
		return UCurveFloat::StaticClass();
		break;
	case ECSV_CurveVector:
		return UCurveVector::StaticClass();
		break;
	case ECSV_CurveLinearColor:
		return UCurveLinearColor::StaticClass();
		break;
	default:
		return UCurveVector::StaticClass();
		break;
	}
}


UCSVImportFactory::UCSVImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UDataTable::StaticClass();

	bEditorImport = true;
	bText = true;

	Formats.Add(TEXT("csv;Comma-separated values"));
}

FText UCSVImportFactory::GetDisplayName() const
{
	return LOCTEXT("CSVImportFactoryDescription", "Comma Separated Values");
}


bool UCSVImportFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UDataTable::StaticClass() || Class == UCurveTable::StaticClass() || Class == UCurveFloat::StaticClass() || Class == UCurveVector::StaticClass() || Class == UCurveLinearColor::StaticClass() );
}

UObject* UCSVImportFactory::FactoryCreateText( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn )
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	// See if table/curve already exists
	UDataTable* ExistingTable = FindObject<UDataTable>(InParent, *InName.ToString());
	UCurveTable* ExistingCurveTable = FindObject<UCurveTable>(InParent, *InName.ToString());
	UCurveBase* ExistingCurve = FindObject<UCurveBase>(InParent, *InName.ToString());

	// Save off information if so
	bool bHaveInfo = false;
	UScriptStruct* ImportRowStruct = NULL;
	ERichCurveInterpMode ImportCurveInterpMode = RCIM_Linear;

	ECSVImportType ImportType = ECSV_DataTable;
	if(ExistingTable != NULL)
	{
		ImportRowStruct = ExistingTable->RowStruct;
		bHaveInfo = true;
	}
	else if(ExistingCurveTable != NULL)
	{
		ImportType = ECSV_CurveTable;
		bHaveInfo = true;
	}
	else if(ExistingCurve != NULL)
	{
		ImportType = ExistingCurve->IsA(UCurveFloat::StaticClass()) ? ECSV_CurveFloat : ECSV_CurveVector;
		bHaveInfo = true;
	}

	bool bDoImport = true;

	// If we do not have the info we need, pop up window to ask for things
	if(!bHaveInfo)
	{
		TSharedPtr<SWindow> ParentWindow;
		// Check if the main frame is loaded.  When using the old main frame it may not be.
		if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
			ParentWindow = MainFrame.GetParentWindow();
		}

		TSharedPtr<SCSVImportOptions> ImportOptionsWindow;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title( LOCTEXT("DataTableOptionsWindowTitle", "DataTable Options" ))
			.SizingRule( ESizingRule::Autosized );
		
		Window->SetContent
		(
			SAssignNew(ImportOptionsWindow, SCSVImportOptions)
			.WidgetWindow(Window)
		);

		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

		ImportType = ImportOptionsWindow->GetSelectedImportType();
		ImportRowStruct = ImportOptionsWindow->GetSelectedRowStruct();
		ImportCurveInterpMode = ImportOptionsWindow->GetSelectedCurveIterpMode();
		bDoImport = ImportOptionsWindow->ShouldImport();
	}

	UObject* NewAsset = NULL;
	if(bDoImport)
	{
		// Convert buffer to an FString (will this be slow with big tables?)
		FString String;
		//const int32 BufferSize = BufferEnd - Buffer;
		//appBufferToString( String, Buffer, BufferSize );
		int32 NumChars = (BufferEnd - Buffer);
		TArray<TCHAR>& StringChars = String.GetCharArray();
		StringChars.AddUninitialized(NumChars+1);
		FMemory::Memcpy(StringChars.GetData(), Buffer, NumChars*sizeof(TCHAR));
		StringChars.Last() = 0;

		TArray<FString> Problems;

		if(ImportType == ECSV_DataTable)
		{
			// If there is an existing table, need to call this to free data memory before recreating object
			if(ExistingTable != NULL)
			{
				ExistingTable->EmptyTable();
			}

			// Create/reset table
			UDataTable* NewTable = CastChecked<UDataTable>(StaticConstructObject(UDataTable::StaticClass(), InParent, InName, Flags));
			NewTable->RowStruct = ImportRowStruct;
			NewTable->ImportPath = FReimportManager::SanitizeImportFilename(CurrentFilename, NewTable);
			// Go ahead and create table from string
			Problems = NewTable->CreateTableFromCSVString(String);

			// Print out
			UE_LOG(LogCSVImportFactory, Log, TEXT("Imported DataTable '%s' - %d Problems"), *InName.ToString(), Problems.Num());
			NewAsset = NewTable;
		}
		else if(ImportType == ECSV_CurveTable)
		{
			// If there is an existing table, need to call this to free data memory before recreating object
			if(ExistingCurveTable != NULL)
			{
				ExistingCurveTable->EmptyTable();
			}

			// Create/reset table
			UCurveTable* NewTable = CastChecked<UCurveTable>(StaticConstructObject(UCurveTable::StaticClass(), InParent, InName, Flags));
			NewTable->ImportPath = FReimportManager::SanitizeImportFilename(CurrentFilename, NewTable);

			// Go ahead and create table from string
			Problems = NewTable->CreateTableFromCSVString(String, ImportCurveInterpMode);

			// Print out
			UE_LOG(LogCSVImportFactory, Log, TEXT("Imported CurveTable '%s' - %d Problems"), *InName.ToString(), Problems.Num());
			NewAsset = NewTable;
		}
		else if(ImportType == ECSV_CurveFloat || ImportType == ECSV_CurveVector || ImportType == ECSV_CurveLinearColor)
		{
			UClass* CurveClass = GetCurveClass( ImportType );

			// Create/reset curve
			UCurveBase* NewCurve = CastChecked<UCurveBase>(StaticConstructObject(CurveClass, InParent, InName, Flags));

			Problems = NewCurve->CreateCurveFromCSVString(String);

			UE_LOG(LogCSVImportFactory, Log, TEXT("Imported Curve '%s' - %d Problems"), *InName.ToString(), Problems.Num());
			NewCurve->ImportPath = FReimportManager::SanitizeImportFilename(CurrentFilename, NewCurve);
			NewAsset = NewCurve;
		}
		
		if(Problems.Num() > 0)
		{
			FString AllProblems;

			for(int32 ProbIdx=0; ProbIdx<Problems.Num(); ProbIdx++)
			{
				// Output problems to log
				UE_LOG(LogCSVImportFactory, Log, TEXT("%d:%s"), ProbIdx, *Problems[ProbIdx]);
				AllProblems += Problems[ProbIdx];
				AllProblems += TEXT("\n");
			}

			// Pop up any problems for user
			FMessageDialog::Open( EAppMsgType::Ok, FText::FromString( AllProblems ) );
		}
	}

	FEditorDelegates::OnAssetPostImport.Broadcast(this, NewAsset);

	return NewAsset;
}

bool UCSVImportFactory::ReimportCSV( UObject* Obj )
{
	bool bHandled = false;
	if(UCurveBase* Curve = Cast<UCurveBase>(Obj))
	{
		bHandled = Reimport(Curve, FReimportManager::ResolveImportFilename(Curve->ImportPath, Curve));
	}
	else if(UCurveTable* CurveTable = Cast<UCurveTable>(Obj))
	{
		bHandled = Reimport(CurveTable, FReimportManager::ResolveImportFilename(CurveTable->ImportPath, CurveTable));
	}
	else if(UDataTable* DataTable = Cast<UDataTable>(Obj))
	{
		bHandled = Reimport(DataTable, FReimportManager::ResolveImportFilename(DataTable->ImportPath, DataTable));
	}
	return bHandled;
}

bool UCSVImportFactory::Reimport( UObject* Obj, const FString& Path )
{
	if(Path.IsEmpty() == false)
	{
		FString FilePath = IFileManager::Get().ConvertToRelativePath(*Path);

		FString Data;
		if( FFileHelper::LoadFileToString( Data, *FilePath) )
		{
			const TCHAR* Ptr = *Data;
			CurrentFilename = FilePath; //not thread safe but seems to be how it is done..
			auto Result = FactoryCreateText( Obj->GetClass(), Obj->GetOuter(), Obj->GetFName(), Obj->GetFlags(), NULL, *FPaths::GetExtension(FilePath), Ptr, Ptr+Data.Len(), NULL );
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

UReimportDataTableFactory::UReimportDataTableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UReimportDataTableFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UDataTable* DataTable = Cast<UDataTable>(Obj);
	if(DataTable)
	{
		OutFilenames.Add(FReimportManager::ResolveImportFilename(DataTable->ImportPath, DataTable));
		return true;
	}
	return false;
}

void UReimportDataTableFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UDataTable* DataTable = Cast<UDataTable>(Obj);
	if(DataTable && ensure(NewReimportPaths.Num() == 1))
	{
		DataTable->ImportPath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], DataTable);
	}
}

EReimportResult::Type UReimportDataTableFactory::Reimport( UObject* Obj )
{	
	auto Result = EReimportResult::Failed;
	if(auto DataTable = Cast<UDataTable>(Obj))
	{
		FDataTableEditorUtils::BroadcastPreChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
		Result = UCSVImportFactory::ReimportCSV(DataTable) ? EReimportResult::Succeeded : EReimportResult::Failed;
		FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	}
	return Result;
}

////////////////////////////////////////////////////////////////////////////
//
UReimportCurveTableFactory::UReimportCurveTableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UReimportCurveTableFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UCurveTable* CurveTable = Cast<UCurveTable>(Obj);
	if(CurveTable)
	{
		OutFilenames.Add(FReimportManager::ResolveImportFilename(CurveTable->ImportPath, CurveTable));
		return true;
	}
	return false;
}

void UReimportCurveTableFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UCurveTable* CurveTable = Cast<UCurveTable>(Obj);
	if(CurveTable && ensure(NewReimportPaths.Num() == 1))
	{
		CurveTable->ImportPath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], CurveTable);
	}
}

EReimportResult::Type UReimportCurveTableFactory::Reimport( UObject* Obj )
{	
	if(Cast<UCurveTable>(Obj))
	{
		return UCSVImportFactory::ReimportCSV(Obj) ? EReimportResult::Succeeded : EReimportResult::Failed;
	}
	return EReimportResult::Failed;
}

////////////////////////////////////////////////////////////////////////////
//
UReimportCurveFactory::UReimportCurveFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UReimportCurveFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UCurveBase* CurveBase = Cast<UCurveBase>(Obj);
	if(CurveBase)
	{
		OutFilenames.Add(FReimportManager::ResolveImportFilename(CurveBase->ImportPath, CurveBase));
		return true;
	}
	return false;
}

void UReimportCurveFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UCurveBase* CurveBase = Cast<UCurveBase>(Obj);
	if(CurveBase && ensure(NewReimportPaths.Num() == 1))
	{
		CurveBase->ImportPath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], CurveBase);
	}
}

EReimportResult::Type UReimportCurveFactory::Reimport( UObject* Obj )
{	
	if(Cast<UCurveBase>(Obj))
	{
		return UCSVImportFactory::ReimportCSV(Obj) ? EReimportResult::Succeeded : EReimportResult::Failed;
	}
	return EReimportResult::Failed;
}


#undef LOCTEXT_NAMESPACE
