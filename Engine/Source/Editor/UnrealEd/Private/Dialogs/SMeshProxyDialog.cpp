// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Dialogs/SMeshProxyDialog.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "MeshUtilities.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "SNumericEntryBox.h"
#include "STextComboBox.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Selection.h"
#define LOCTEXT_NAMESPACE "SMeshProxyDialog"


/*-----------------------------------------------------------------------------
   SMeshProxyDialog
-----------------------------------------------------------------------------*/
class SMeshProxyDialog : public IMeshProxyDialog, public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMeshProxyDialog)
	{
	}

	SLATE_END_ARGS()

public:
	/** Destructor **/
	virtual ~SMeshProxyDialog();

	/** SMeshProxyDialog functions */
	virtual void AssignWindowContent(TSharedPtr<SWindow> Window) override;
	virtual TSharedPtr<SWindow> GetParentWindow() override;
	virtual void SetParentWindow(TSharedPtr<SWindow> Window) override;
	virtual void OnWindowClosed(const TSharedRef<SWindow>&) override;
	virtual void MarkDirty() override;

	/** SWidget functions */
	void Construct(const FArguments& InArgs);

protected:
	/** ScreenSize accessors */
	TOptional<int32> GetScreenSize() const;
	void ScreenSizeChanged(int32 NewValue);		//used with editable text block (Simplygon)

	/** Recalculate Normals accessors */
	ECheckBoxState GetRecalculateNormals() const;
	void SetRecalculateNormals(ECheckBoxState NewValue);

	/** Hard Angle Threshold accessors */
	TOptional<float> GetHardAngleThreshold() const;
	bool HardAngleThresholdEnabled() const;
	void HardAngleThresholdChanged(float NewValue);

	/** Hole filling accessors */
	TOptional<int32> GetMergeDistance() const;
	void MergeDistanceChanged(int32 NewValue);

	/** Clipping Plane accessors */
	ECheckBoxState GetUseClippingPlane() const;
	void SetUseClippingPlane(ECheckBoxState NewValue);
	bool UseClippingPlaneEnabled() const;
	void SetClippingAxis(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	TOptional<float> GetClippingLevel() const;
	void ClippingLevelChanged(float NewValue);
	
	/** TextureResolution accessors */
	void SetTextureResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Delegates for Merge button state */
	bool GetMergeButtonEnabledState() const;
	
	/** Called when the Cancel button is clicked */
	FReply OnCancelClicked();

	FReply OnMergeClicked();

	/** Called when actors selection is changed */
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection);
	
	/** Generates destination package name using currently selected actors */
	void GenerateNewProxyPackageName();

	/** Destination package name accessors */
	FText GetProxyPackagName() const;
	void OnPackageNameTextCommited(const FText& InText, ETextCommit::Type InCommitType);
	
	/** Called when the select package name  button is clicked. Brings asset path picker dialog */
	FReply OnSelectPackageNameClicked();

private:
	/** Creates the geometry mode controls */
	void CreateLayout();

	/** Clears all cached data */
	void Reset(const bool bRefresh = false);

private:
	/** Pointer to the parent window, so we know to destroy it when done */
	TSharedPtr<SWindow> ParentWindow;

	/**  */
	FMeshProxySettings ProxySettings;

	TArray< TSharedPtr<FString> >	CuttingPlaneOptions;
	TArray< TSharedPtr<FString> >	TextureResolutionOptions;

	/** Dictates the availability of the Merge button */
	bool bCanMerge;

	/** If true, the selected group needs to be re-evaluated */
	bool bDirty;
	
	/** Proxy mesh destination package name */
	FString ProxyPackageName;
};


TSharedPtr<IMeshProxyDialog> IMeshProxyDialog::MakeControls()
{
	return SNew(SMeshProxyDialog);
}

SMeshProxyDialog::~SMeshProxyDialog()
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor" );
	LevelEditor.OnActorSelectionChanged().RemoveAll(this);
}

void SMeshProxyDialog::AssignWindowContent(TSharedPtr<SWindow> Window)
{
	Window->SetContent(this->AsShared());
}

TSharedPtr<SWindow> SMeshProxyDialog::GetParentWindow()
{
	return ParentWindow;
}

void SMeshProxyDialog::SetParentWindow(TSharedPtr<SWindow> Window)
{
	ParentWindow = Window;
}

void SMeshProxyDialog::OnWindowClosed(const TSharedRef<SWindow>&)
{
	ParentWindow.Reset();
}

void SMeshProxyDialog::MarkDirty()
{
	bDirty = true;
}

void SMeshProxyDialog::Construct(const FArguments& InArgs)
{
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+X"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+Y"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+Z"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-X"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-Y"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-Z"))));

	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("64"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("128"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("256"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("512"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("1024"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("2048"))));

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor" );
	LevelEditor.OnActorSelectionChanged().AddSP(this, &SMeshProxyDialog::OnActorSelectionChanged);

	Reset();
	GenerateNewProxyPackageName();
	CreateLayout();
}

void SMeshProxyDialog::CreateLayout()
{
	this->ChildSlot
	[
		SNew(SVerticalBox)
			
		+SVerticalBox::Slot()
		.FillHeight(0.2f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			// Simplygon logo
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("MeshProxy.SimplygonLogo"))
		]
			
		+SVerticalBox::Slot()
		.FillHeight(0.65f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				// Proxy options
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(0.4f)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("OnScreenSizeLabel", "On Screen Size (pixels)"))
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("RecalcNormalsLabel", "Recalculate Normals"))
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("MergeDistanceLabel", "Merge Distance (pixels)"))
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("ClippingPlaneLabel", "Clipping Plane"))
					]
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("TextureResolutionLabel", "Texture Resolution"))
					]

					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("DestinationPackageLabel", "Base Asset Name"))
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SVerticalBox)
					//Screen Size
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SNumericEntryBox<int32>)
						.MinValue(40)
						.MaxValue(1200)
						.MinSliderValue(40)
						.MaxSliderValue(1200)
						.AllowSpin(true)
						.Value(this, &SMeshProxyDialog::GetScreenSize)
						.OnValueChanged(this, &SMeshProxyDialog::ScreenSizeChanged)
					]
					//Recalculate normals
					+SVerticalBox::Slot()
					.FillHeight(1.0)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						/*
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(0.0, 0.0, 10.0, 0.0)
						[
							SNew(SCheckBox)
							.Type(ESlateCheckBoxType::CheckBox)
							.IsChecked(this, &SMeshProxyDialog::GetRecalculateNormals)
							.OnCheckStateChanged(this, &SMeshProxyDialog::SetRecalculateNormals)
						]
						*/
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(0.0, 0.0, 3.0, 0.0)
						[
							SNew(STextBlock).Text(LOCTEXT("HardAngleLabel", "Hard Angle:"))
						]
						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						.VAlign(VAlign_Center)
						[
							SNew(SNumericEntryBox<float>)
							.MinValue(0.f)
							.MaxValue(180.f)
							.MinSliderValue(0.f)
							.MaxSliderValue(180.f)
							.AllowSpin(true)
							.Value(this, &SMeshProxyDialog::GetHardAngleThreshold)
							.OnValueChanged(this, &SMeshProxyDialog::HardAngleThresholdChanged)
							.IsEnabled(this, &SMeshProxyDialog::HardAngleThresholdEnabled)
						]
					]
					//Merge Distance
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SNumericEntryBox<int32>)
						.MinValue(0)
						.MaxValue(300)
						.MinSliderValue(0)
						.MaxSliderValue(300)
						.AllowSpin(true)
						.Value(this, &SMeshProxyDialog::GetMergeDistance)
						.OnValueChanged(this, &SMeshProxyDialog::MergeDistanceChanged)
					]
					//Clipping plane
					+SVerticalBox::Slot()
					.FillHeight(1.0)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(0.0, 0.0, 10.0, 0.0)
						[
							SNew(SCheckBox)
							.Type(ESlateCheckBoxType::CheckBox)
							.IsChecked(this, &SMeshProxyDialog::GetUseClippingPlane)
							.OnCheckStateChanged(this, &SMeshProxyDialog::SetUseClippingPlane)
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(0.0, 0.0, 8.0, 0.0)
						[
							SNew(STextComboBox)
							.OptionsSource(&CuttingPlaneOptions)
							.InitiallySelectedItem(CuttingPlaneOptions[0])
							.OnSelectionChanged(this, &SMeshProxyDialog::SetClippingAxis)
							.IsEnabled(this, &SMeshProxyDialog::UseClippingPlaneEnabled)
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0, 0.0, 3.0, 0.0)
						[
							SNew(STextBlock).Text(LOCTEXT("PlaneLevelLabel", "Plane level:"))
							.IsEnabled(this, &SMeshProxyDialog::UseClippingPlaneEnabled)
						]
						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							SNew(SNumericEntryBox<float>)
							.Value(this, &SMeshProxyDialog::GetClippingLevel)
							.OnValueChanged(this, &SMeshProxyDialog::ClippingLevelChanged)
							.IsEnabled(this, &SMeshProxyDialog::UseClippingPlaneEnabled)
						]
					]

					//Texture Resolution
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextComboBox)
						.OptionsSource(&TextureResolutionOptions)
						.InitiallySelectedItem(TextureResolutionOptions[3]) //512
						.OnSelectionChanged(this, &SMeshProxyDialog::SetTextureResolution)
					]

					// Package Name
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						[
							SNew(SEditableTextBox)
							.Text(this, &SMeshProxyDialog::GetProxyPackagName)
							.OnTextCommitted(this, &SMeshProxyDialog::OnPackageNameTextCommited)
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							//.HAlign(HAlign_Center)
							.OnClicked(this, &SMeshProxyDialog::OnSelectPackageNameClicked)
							.Text(LOCTEXT("SelectPackageButton", "..."))
						]
					]
				]
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.AutoHeight()
		.Padding(0,4,0,8)
		[
			// Merge, Cancel buttons
			SNew(SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+SUniformGridPanel::Slot(0,0)
			[
				SNew(SButton)
				.Text(LOCTEXT("MergeLabel", "Merge"))
				.HAlign(HAlign_Center)
				.IsEnabled(this, &SMeshProxyDialog::GetMergeButtonEnabledState)
				.OnClicked(this,&SMeshProxyDialog::OnMergeClicked)

			]
			+SUniformGridPanel::Slot(1,0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SMeshProxyDialog::OnCancelClicked)
			]
		]
	];
}

void SMeshProxyDialog::Reset(const bool bRefresh)
{
	bCanMerge = true;
	if (bRefresh)
	{
		FEditorDelegates::RefreshEditor.Broadcast();
	}
}

//Screen size
TOptional<int32> SMeshProxyDialog::GetScreenSize() const
{
	return ProxySettings.ScreenSize;
}

void SMeshProxyDialog::ScreenSizeChanged(int32 NewValue)
{
	ProxySettings.ScreenSize = NewValue;
}

//Recalculate normals
ECheckBoxState SMeshProxyDialog::GetRecalculateNormals() const
{
	return ProxySettings.bRecalculateNormals ? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetRecalculateNormals(ECheckBoxState NewValue)
{
	ProxySettings.bRecalculateNormals = (NewValue == ECheckBoxState::Checked);
}

//Hard Angle Threshold
bool SMeshProxyDialog::HardAngleThresholdEnabled() const
{
	if(ProxySettings.bRecalculateNormals)
	{
		return true;
	}

	return false;
}

TOptional<float> SMeshProxyDialog::GetHardAngleThreshold() const
{
	return ProxySettings.HardAngleThreshold;
}

void SMeshProxyDialog::HardAngleThresholdChanged(float NewValue)
{
	ProxySettings.HardAngleThreshold = NewValue;
}

//Merge Distance
TOptional<int32> SMeshProxyDialog::GetMergeDistance() const
{
	return ProxySettings.MergeDistance;
}

void SMeshProxyDialog::MergeDistanceChanged(int32 NewValue)
{
	ProxySettings.MergeDistance = NewValue;
}

//Clipping Plane
ECheckBoxState SMeshProxyDialog::GetUseClippingPlane() const
{
	return ProxySettings.bUseClippingPlane ? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetUseClippingPlane(ECheckBoxState NewValue)
{
	ProxySettings.bUseClippingPlane = (NewValue == ECheckBoxState::Checked);
}

bool SMeshProxyDialog::UseClippingPlaneEnabled() const
{
	if(ProxySettings.bUseClippingPlane)
	{
		return true;
	}
	return false;
}

void SMeshProxyDialog::SetClippingAxis(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo )
{
	//This is a ugly hack, but it solves the problem for now
	int32 AxisIndex = CuttingPlaneOptions.Find(NewSelection);
	//FMessageDialog::Debugf(*FString::Printf(TEXT("%d"), AxisIndex ));
	if(AxisIndex < 3)
	{
		ProxySettings.bPlaneNegativeHalfspace = false;
		ProxySettings.AxisIndex = AxisIndex;
		return;
	}
	else
	{
		ProxySettings.bPlaneNegativeHalfspace = true;
		ProxySettings.AxisIndex = AxisIndex - 3;
		return;
	}
}

TOptional<float> SMeshProxyDialog::GetClippingLevel() const
{
	return ProxySettings.ClippingLevel;
}

void SMeshProxyDialog::ClippingLevelChanged(float NewValue)
{
	ProxySettings.ClippingLevel = NewValue;
}

//Texture Resolution
void SMeshProxyDialog::SetTextureResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Resolution = 512;
	TTypeFromString<int32>::FromString(Resolution, **NewSelection);
	
	ProxySettings.TextureWidth	= Resolution;
	ProxySettings.TextureHeight = Resolution;
}


bool SMeshProxyDialog::GetMergeButtonEnabledState() const
{
	return bCanMerge;
}

FReply SMeshProxyDialog::OnCancelClicked()
{
	if (ParentWindow.IsValid())
	{
		ParentWindow->HideWindow();
	}

	return FReply::Handled();
}

FReply SMeshProxyDialog::OnMergeClicked()
{
	TArray<AActor*> Actors;
	TArray<UObject*> AssetsToSync;

	//Get the module for the proxy mesh utilities
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
			
	USelection* SelectedActors = GEditor->GetSelectedActors();
	for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
		}
	}

	if (Actors.Num())
	{
		GWarn->BeginSlowTask( LOCTEXT("MeshProxy_CreatingProxy", "Creating Mesh Proxy"), true);
		GEditor->BeginTransaction( LOCTEXT( "MeshProxy_Create", "Creating Mesh Proxy" ) );
		
		FVector ProxyLocation = FVector::ZeroVector;
		MeshUtilities.CreateProxyMesh(Actors, ProxySettings, NULL, ProxyPackageName, AssetsToSync, ProxyLocation);

		GEditor->EndTransaction();
		GWarn->EndSlowTask();

		//Create a new world actor and place it at the same location as the original assets
		if (Actors[0] != nullptr)
		{
			UStaticMesh* ProxyMesh = nullptr;
			if (AssetsToSync.FindItemByClass(&ProxyMesh))
			{
				UWorld* World = Actors[0]->GetWorld();
				AStaticMeshActor* WorldActor = Cast<AStaticMeshActor>(World->SpawnActor(AStaticMeshActor::StaticClass(), &ProxyLocation));
				WorldActor->GetStaticMeshComponent()->StaticMesh = ProxyMesh;
				WorldActor->SetActorLabel(ProxyMesh->GetName());
			}
		}

		//Update the asset registry that a new static mash and material has been created
		if (AssetsToSync.Num())
		{
			FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			int32 AssetCount = AssetsToSync.Num();
			for(int32 AssetIndex = 0; AssetIndex < AssetCount; AssetIndex++ )
			{
				AssetRegistry.AssetCreated(AssetsToSync[AssetIndex]);
				GEditor->BroadcastObjectReimported(AssetsToSync[AssetIndex]);
			}

			//Also notify the content browser that the new assets exists
			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets( AssetsToSync, true );
		}
	}
	
	return FReply::Handled();
	
}

void SMeshProxyDialog::OnActorSelectionChanged(const TArray<UObject*>& NewSelection)
{
	GenerateNewProxyPackageName();
}

void SMeshProxyDialog::GenerateNewProxyPackageName()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	ProxyPackageName.Empty();

	// Iterate through selected actors and find first static mesh asset
	// Use this static mesh path as destination package name for a merged mesh
	for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			TInlineComponentArray<UStaticMeshComponent*> SMComponets; 
			Actor->GetComponents<UStaticMeshComponent>(SMComponets);
			for (UStaticMeshComponent* Component : SMComponets)
			{
				if (Component->StaticMesh)
				{
					ProxyPackageName = FPackageName::GetLongPackagePath(Component->StaticMesh->GetOutermost()->GetName());
					ProxyPackageName+= FString(TEXT("/PROXY_")) + Component->StaticMesh->GetName();
					break;
				}
			}
		}

		if (!ProxyPackageName.IsEmpty())
		{
			break;
		}
	}

	if (ProxyPackageName.IsEmpty())
	{
		ProxyPackageName = FPackageName::FilenameToLongPackageName(FPaths::GameContentDir() + TEXT("PROXY"));
		ProxyPackageName = MakeUniqueObjectName(NULL, UPackage::StaticClass(), *ProxyPackageName).ToString();
	}
}

FText SMeshProxyDialog::GetProxyPackagName() const
{
	return FText::FromString(ProxyPackageName);
}

void SMeshProxyDialog::OnPackageNameTextCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	ProxyPackageName = InText.ToString();

	if (FPackageName::IsShortPackageName(ProxyPackageName))
	{
		ProxyPackageName = FPackageName::FilenameToLongPackageName(FPaths::GameContentDir()) + ProxyPackageName;
	}
}

FReply SMeshProxyDialog::OnSelectPackageNameClicked()
{
	TSharedRef<SDlgPickAssetPath> NewLayerDlg = 
		SNew(SDlgPickAssetPath)
		.Title(LOCTEXT("SelectProxyPackage", "Base Asset Name"))
		.DefaultAssetPath(FText::FromString(ProxyPackageName));

	if (NewLayerDlg->ShowModal() != EAppReturnType::Cancel)
	{
		ProxyPackageName = NewLayerDlg->GetFullAssetPath().ToString();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
