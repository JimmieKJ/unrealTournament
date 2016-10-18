// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "K2Node.h"
#include "PropertyEditorModule.h"
#include "AnimGraphNodeDetails.h"
#include "AnimGraphDefinitions.h"
#include "ObjectEditorUtils.h"
#include "AssetData.h"
#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "AssetSearchBoxUtilPersona.h"
#include "IDetailPropertyRow.h"
#include "IDetailCustomNodeBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "AnimGraphNode_Base.h"
#include "GraphEditor.h"
#include "Persona.h"
#include "BoneSelectionWidget.h"
#include "SExpandableArea.h"
#include "Animation/BlendProfile.h"
#include "SBlendProfilePicker.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "Animation/AnimInstance.h"

#define LOCTEXT_NAMESPACE "KismetNodeWithOptionalPinsDetails"

/////////////////////////////////////////////////////
// FAnimGraphNodeDetails 

TSharedRef<IDetailCustomization> FAnimGraphNodeDetails::MakeInstance()
{
	return MakeShareable(new FAnimGraphNodeDetails());
}

void FAnimGraphNodeDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	TArray< TWeakObjectPtr<UObject> > SelectedObjectsList = DetailBuilder.GetDetailsView().GetSelectedObjects();

	// Hide the pin options property; it's represented inline per-property instead
	IDetailCategoryBuilder& PinOptionsCategory = DetailBuilder.EditCategory("PinOptions");
	TSharedRef<IPropertyHandle> AvailablePins = DetailBuilder.GetProperty("ShowPinForProperties");
	DetailBuilder.HideProperty(AvailablePins);

	// Find the one (and only one) selected AnimGraphNode
	UAnimGraphNode_Base* AnimGraphNode = NULL;
	for (auto SelectionIt = SelectedObjectsList.CreateIterator(); SelectionIt; ++SelectionIt)
	{
		if (UAnimGraphNode_Base* TestNode = Cast<UAnimGraphNode_Base>(SelectionIt->Get()))
		{
			if (AnimGraphNode != NULL)
			{
				// We don't allow editing of multiple anim graph nodes at once
				AbortDisplayOfAllNodes(SelectedObjectsList, DetailBuilder);
				return;
			}
			else
			{
				AnimGraphNode = TestNode;
			}
		}
	}

	if (AnimGraphNode == NULL)
	{
		return;
	}

	TargetSkeleton = AnimGraphNode->GetAnimBlueprint()->TargetSkeleton;
	TargetSkeletonName = FString::Printf(TEXT("%s'%s'"), *TargetSkeleton->GetClass()->GetName(), *TargetSkeleton->GetPathName());

	// Get the node property
	const UStructProperty* NodeProperty = AnimGraphNode->GetFNodeProperty();
	if (NodeProperty == NULL)
	{
		return;
	}

	// customize anim graph node's own details if needed
	AnimGraphNode->CustomizeDetails(DetailBuilder);

	// Now customize each property in the pins array
	for (int CustomPinIndex = 0; CustomPinIndex < AnimGraphNode->ShowPinForProperties.Num(); ++CustomPinIndex)
	{
		const FOptionalPinFromProperty& OptionalPin = AnimGraphNode->ShowPinForProperties[CustomPinIndex];

		UProperty* TargetProperty = FindField<UProperty>(NodeProperty->Struct, OptionalPin.PropertyName);

		IDetailCategoryBuilder& CurrentCategory = DetailBuilder.EditCategory(FObjectEditorUtils::GetCategoryFName(TargetProperty));

		const FName TargetPropertyPath(*FString::Printf(TEXT("%s.%s"), *NodeProperty->GetName(), *TargetProperty->GetName()));
		TSharedRef<IPropertyHandle> TargetPropertyHandle = DetailBuilder.GetProperty(TargetPropertyPath, AnimGraphNode->GetClass() );

		// Not optional
		if (!OptionalPin.bCanToggleVisibility && OptionalPin.bShowPin)
		{
			// Always displayed as a pin, so hide the property
			DetailBuilder.HideProperty(TargetPropertyHandle);
			continue;
		}

		if(!TargetPropertyHandle->GetProperty())
		{
			continue;
		}

		// if customized, do not do anything
		if(TargetPropertyHandle->IsCustomized())
		{
			continue;
		}
		
		// sometimes because of order of customization
		// this gets called first for the node you'd like to customize
		// then the above statement won't work
		// so you can mark certain property to have meta data "CustomizeProperty"
		// which will trigger below statement
		if (OptionalPin.bPropertyIsCustomized)
		{
			continue;
		}

		IDetailPropertyRow& PropertyRow = CurrentCategory.AddProperty( TargetPropertyHandle );

		TSharedPtr<SWidget> NameWidget; 
		TSharedPtr<SWidget> ValueWidget;
		FDetailWidgetRow Row;
		PropertyRow.GetDefaultWidgets( NameWidget, ValueWidget, Row );

		TSharedRef<SWidget> TempWidget = CreatePropertyWidget(TargetProperty, TargetPropertyHandle, AnimGraphNode->GetClass());
		ValueWidget = (TempWidget == SNullWidget::NullWidget) ? ValueWidget : TempWidget;

		if (OptionalPin.bCanToggleVisibility)
		{
			const FName OptionalPinArrayEntryName(*FString::Printf(TEXT("ShowPinForProperties[%d].bShowPin"), CustomPinIndex));
			TSharedRef<IPropertyHandle> ShowHidePropertyHandle = DetailBuilder.GetProperty(OptionalPinArrayEntryName);

			ShowHidePropertyHandle->MarkHiddenByCustomization();

			const FText AsPinTooltip = LOCTEXT("AsPinTooltip", "Show this property as a pin on the node");

			TSharedRef<SWidget> ShowHidePropertyWidget = ShowHidePropertyHandle->CreatePropertyValueWidget();
			ShowHidePropertyWidget->SetToolTipText(AsPinTooltip);

			ValueWidget->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FAnimGraphNodeDetails::GetVisibilityOfProperty, ShowHidePropertyHandle)));

			NameWidget = SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.FillWidth(1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						ShowHidePropertyWidget
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AsPin", " (As pin) "))
						.Font(DetailBuilder.IDetailLayoutBuilder::GetDetailFont())
						.ToolTipText(AsPinTooltip)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSpacer)
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.FillWidth(1)
					.Padding(FMargin(0,0,4,0))
					[
						NameWidget.ToSharedRef()
					]
				];
		}

		const bool bShowChildren = true;
		PropertyRow.CustomWidget(bShowChildren)
		.NameContent()
		.MinDesiredWidth(Row.NameWidget.MinWidth)
		.MaxDesiredWidth(Row.NameWidget.MaxWidth)
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(Row.ValueWidget.MinWidth)
		.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
		[
			ValueWidget.ToSharedRef()
		];
	}
}

TSharedRef<SWidget> FAnimGraphNodeDetails::CreatePropertyWidget(UProperty* TargetProperty, TSharedRef<IPropertyHandle> TargetPropertyHandle, const UClass* NodeClass)
{
	if(const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>( TargetProperty ))
	{
		if(ObjectProperty->PropertyClass->IsChildOf(UAnimationAsset::StaticClass()))
		{
			bool bAllowClear = !(ObjectProperty->PropertyFlags & CPF_NoClear);

			return SNew(SObjectPropertyEntryBox)
				.PropertyHandle(TargetPropertyHandle)
				.AllowedClass(ObjectProperty->PropertyClass)
				.AllowClear(bAllowClear)
				.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FAnimGraphNodeDetails::OnShouldFilterAnimAsset, NodeClass));
		}
		else if(ObjectProperty->PropertyClass->IsChildOf(UBlendProfile::StaticClass()) && TargetSkeleton)
		{
			TSharedPtr<IPropertyHandle> PropertyPtr(TargetPropertyHandle);

			UObject* PropertyValue = nullptr;
			TargetPropertyHandle->GetValue(PropertyValue);

			UBlendProfile* CurrentProfile = Cast<UBlendProfile>(PropertyValue);

			return SNew(SBlendProfilePicker)
				.TargetSkeleton(this->TargetSkeleton)
				.AllowNew(false)
				.OnBlendProfileSelected(this, &FAnimGraphNodeDetails::OnBlendProfileChanged, PropertyPtr)
				.InitialProfile(CurrentProfile);
		}
	}

	return SNullWidget::NullWidget;
}

bool FAnimGraphNodeDetails::OnShouldFilterAnimAsset( const FAssetData& AssetData, const UClass* NodeToFilterFor ) const
{
	const FString* SkeletonName = AssetData.TagsAndValues.Find(TEXT("Skeleton"));
	if ((SkeletonName != nullptr) && (*SkeletonName == TargetSkeletonName))
	{
		const UClass* AssetClass = AssetData.GetClass();
		// If node is an 'asset player', only let you select the right kind of asset for it
		if (!NodeToFilterFor->IsChildOf(UAnimGraphNode_AssetPlayerBase::StaticClass()) || SupportNodeClassForAsset(AssetClass, NodeToFilterFor))
		{
			return false;
		}
	}
	return true;
}

EVisibility FAnimGraphNodeDetails::GetVisibilityOfProperty(TSharedRef<IPropertyHandle> Handle) const
{
	bool bShowAsPin;
	if (FPropertyAccess::Success == Handle->GetValue(/*out*/ bShowAsPin))
	{
		return bShowAsPin ? EVisibility::Hidden : EVisibility::Visible;
	}
	else
	{
		return EVisibility::Visible;
	}
}

// Hide any anim graph node properties; used when multiple are selected
void FAnimGraphNodeDetails::AbortDisplayOfAllNodes(TArray< TWeakObjectPtr<UObject> >& SelectedObjectsList, class IDetailLayoutBuilder& DetailBuilder)
{
	// Display a warning message
	IDetailCategoryBuilder& ErrorCategory = DetailBuilder.EditCategory("Animation Nodes");
	ErrorCategory.AddCustomRow( LOCTEXT("ErrorRow", "Error") )
	[
		SNew(STextBlock)
		.Text(LOCTEXT("MultiSelectNotSupported", "Multiple nodes selected"))
		.Font(DetailBuilder.GetDetailFontBold())
	];

	// Hide all node properties
	for (auto SelectionIt = SelectedObjectsList.CreateIterator(); SelectionIt; ++SelectionIt)
	{
		if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(SelectionIt->Get()))
		{
			if (const UStructProperty* NodeProperty = AnimNode->GetFNodeProperty())
			{
				DetailBuilder.HideProperty(NodeProperty->GetFName(), AnimNode->GetClass());
			}
		}
	}
}

void FAnimGraphNodeDetails::OnBlendProfileChanged(UBlendProfile* NewProfile, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	if(PropertyHandle.IsValid())
	{
		PropertyHandle->SetValue(NewProfile);
	}
}


TSharedRef<IPropertyTypeCustomization> FInputScaleBiasCustomization::MakeInstance() 
{
	return MakeShareable(new FInputScaleBiasCustomization());
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FInputScaleBiasCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()
	[
		SNew(STextBlock)
		.Text(StructPropertyHandle->GetPropertyDisplayName())
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("InputScaleBias", "Input scaling"))
		.InitiallyCollapsed(true)
		.BodyContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.FillWidth(0.5f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MinInputScaleBias", "Minimal input value"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(0.5f)
				.Padding(FMargin(5.0f,1.0f,5.0f,1.0f))
				[
					SNew(SEditableTextBox)
					.MinDesiredWidth(64.0f)
					.Text(this, &FInputScaleBiasCustomization::GetMinValue, StructPropertyHandle)
					.SelectAllTextWhenFocused(true)
					.RevertTextOnEscape(true)
					.OnTextCommitted(this, &FInputScaleBiasCustomization::OnMinValueCommitted, StructPropertyHandle)
				]
			]
			+SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.FillWidth(0.5f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaxInputScaleBias", "Maximal input value"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(0.5f)
				.Padding(FMargin(5.0f,1.0f,5.0f,1.0f))
				[
					SNew(SEditableTextBox)
					.MinDesiredWidth(64.0f)
					.Text(this, &FInputScaleBiasCustomization::GetMaxValue, StructPropertyHandle)
					.SelectAllTextWhenFocused(true)
					.RevertTextOnEscape(true)
					.OnTextCommitted(this, &FInputScaleBiasCustomization::OnMaxValueCommitted, StructPropertyHandle)
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

/////////////////////////////////////////////////////
void FInputScaleBiasCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// nothing here
}

void UpdateInputScaleBiasWith(float MinValue, float MaxValue, TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	TSharedRef<class IPropertyHandle> BiasProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Bias").ToSharedRef();
	TSharedRef<class IPropertyHandle> ScaleProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Scale").ToSharedRef();
	const float Difference = MaxValue - MinValue;
	const float Scale = Difference != 0.0f? 1.0f / Difference : 0.0f;
	const float Bias = -MinValue * Scale;
	ScaleProperty->SetValue(Scale);
	BiasProperty->SetValue(Bias);
}

float GetMinValueInputScaleBias(TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	TSharedRef<class IPropertyHandle> BiasProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Bias").ToSharedRef();
	TSharedRef<class IPropertyHandle> ScaleProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Scale").ToSharedRef();
	float Scale = 1.0f;
	float Bias = 0.0f;
	ScaleProperty->GetValue(Scale);
	BiasProperty->GetValue(Bias);
	return Scale != 0.0f? (FMath::Abs(Bias) < SMALL_NUMBER? 0.0f : -Bias) / Scale : 0.0f; // to avoid displaying of - in front of 0
}

float GetMaxValueInputScaleBias(TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	TSharedRef<class IPropertyHandle> BiasProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Bias").ToSharedRef();
	TSharedRef<class IPropertyHandle> ScaleProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Scale").ToSharedRef();
	float Scale = 1.0f;
	float Bias = 0.0f;
	ScaleProperty->GetValue(Scale);
	BiasProperty->GetValue(Bias);
	return Scale != 0.0f? (1.0f - Bias) / Scale : 0.0f;
}

FText FInputScaleBiasCustomization::GetMinValue(TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle) const
{
	return FText::FromString(FString::Printf(TEXT("%.6f"), GetMinValueInputScaleBias(InputBiasScaleStructPropertyHandle)));
}

FText FInputScaleBiasCustomization::GetMaxValue(TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle) const
{
	return FText::FromString(FString::Printf(TEXT("%.6f"), GetMaxValueInputScaleBias(InputBiasScaleStructPropertyHandle)));
}

void FInputScaleBiasCustomization::OnMinValueCommitted(const FText& NewText, ETextCommit::Type CommitInfo, TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		UpdateInputScaleBiasWith(FCString::Atof(*NewText.ToString()), GetMaxValueInputScaleBias(InputBiasScaleStructPropertyHandle), InputBiasScaleStructPropertyHandle);
	}
}

void FInputScaleBiasCustomization::OnMaxValueCommitted(const FText& NewText, ETextCommit::Type CommitInfo, TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		UpdateInputScaleBiasWith(GetMinValueInputScaleBias(InputBiasScaleStructPropertyHandle), FCString::Atof(*NewText.ToString()), InputBiasScaleStructPropertyHandle);
	}
}

/////////////////////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FBoneReferenceCustomization::MakeInstance()
{
	return MakeShareable(new FBoneReferenceCustomization());
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBoneReferenceCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIdx);
		if(ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FBoneReference, BoneName))
		{
			BoneRefProperty = ChildHandle;
			break;
		}
	}

	check(BoneRefProperty->IsValidHandle());

	TArray<UObject*> Objects;
	StructPropertyHandle->GetOuterObjects(Objects);
	UAnimGraphNode_Base* AnimGraphNode = NULL;
	USkeletalMesh* SkeletalMesh = NULL;
	UAnimationAsset * AnimationAsset = NULL;
	USkeleton* TargetSkeleton = NULL;

	for (auto OuterIter = Objects.CreateIterator() ; OuterIter ; ++OuterIter)
	{
		AnimGraphNode = Cast<UAnimGraphNode_Base>(*OuterIter);
		if (AnimGraphNode)
		{
			TargetSkeleton = AnimGraphNode->GetAnimBlueprint()->TargetSkeleton;
			break;
		}
		SkeletalMesh = Cast<USkeletalMesh>(*OuterIter);
		if (SkeletalMesh)
		{
			TargetSkeleton = SkeletalMesh->Skeleton;
			break;
		}
		AnimationAsset = Cast<UAnimationAsset>(*OuterIter);
		if(AnimationAsset)
		{
			TargetSkeleton = AnimationAsset->GetSkeleton();
			break;
		}

		if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(*OuterIter))
		{
			if (AnimInstance->CurrentSkeleton)
			{
				TargetSkeleton = AnimInstance->CurrentSkeleton;
				break;
			}
			else if (UAnimBlueprintGeneratedClass* AnimBPClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstance->GetClass()))
			{
				TargetSkeleton = AnimBPClass->TargetSkeleton;
				break;
			}
		}
	}

	if (TargetSkeleton)
	{
		HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];

		HeaderRow.ValueContent()
		[
			SNew(SBoneSelectionWidget, TargetSkeleton)
			.Tooltip(StructPropertyHandle->GetToolTipText())
			.OnBoneSelectionChanged(this, &FBoneReferenceCustomization::OnBoneSelectionChanged)
			.OnGetSelectedBone(this, &FBoneReferenceCustomization::GetSelectedBone)
		];
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FBoneReferenceCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// No child customizations as the properties are shown in the header
}

void FBoneReferenceCustomization::OnBoneSelectionChanged(FName Name)
{
	BoneRefProperty->SetValue(Name);
}

FName FBoneReferenceCustomization::GetSelectedBone() const
{
	FString OutText;
	
	BoneRefProperty->GetValueAsFormattedString(OutText);

	return FName(*OutText);
}

TSharedRef<IDetailCustomization> FAnimGraphParentPlayerDetails::MakeInstance(TWeakPtr<FPersona> InPersona)
{
	return MakeShareable(new FAnimGraphParentPlayerDetails(InPersona));
}

void FAnimGraphParentPlayerDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	check(SelectedObjects.Num() == 1);

	EditorObject = Cast<UEditorParentPlayerListObj>(SelectedObjects[0].Get());
	check(EditorObject);
	
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("AnimGraphOverrides");
	DetailBuilder.HideProperty("Overrides");

	// Build a hierarchy of entires for a tree view in the form of Blueprint->Graph->Node
	for(FAnimParentNodeAssetOverride& Override : EditorObject->Overrides)
	{
		UAnimGraphNode_Base* Node = EditorObject->GetVisualNodeFromGuid(Override.ParentNodeGuid);
		TSharedPtr<FPlayerTreeViewEntry> NodeEntry = MakeShareable(new FPlayerTreeViewEntry(Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), EPlayerTreeViewEntryType::Node, &Override));
		
		// Process blueprint entry
		TSharedPtr<FPlayerTreeViewEntry>* ExistingBPEntry = ListEntries.FindByPredicate([Node](const TSharedPtr<FPlayerTreeViewEntry>& Other)
		{
			return Node->GetBlueprint()->GetName() == Other->EntryName;
		});

		if(!ExistingBPEntry)
		{
			ListEntries.Add(MakeShareable(new FPlayerTreeViewEntry(Node->GetBlueprint()->GetName(), EPlayerTreeViewEntryType::Blueprint)));
			ExistingBPEntry = &ListEntries.Last();
		}

		// Process graph entry
		TSharedPtr<FPlayerTreeViewEntry>* ExistingGraphEntry = (*ExistingBPEntry)->Children.FindByPredicate([Node](const TSharedPtr<FPlayerTreeViewEntry>& Other)
		{
			return Node->GetGraph()->GetName() == Other->EntryName;
		});

		if(!ExistingGraphEntry)
		{
			(*ExistingBPEntry)->Children.Add(MakeShareable(new FPlayerTreeViewEntry(Node->GetGraph()->GetName(), EPlayerTreeViewEntryType::Graph)));
			ExistingGraphEntry = &(*ExistingBPEntry)->Children.Last();
		}

		// Process node entry
		(*ExistingGraphEntry)->Children.Add(NodeEntry);
	}

	FDetailWidgetRow& Row = Category.AddCustomRow(FText::GetEmpty());
	TSharedRef<STreeView<TSharedPtr<FPlayerTreeViewEntry>>> TreeView = SNew(STreeView<TSharedPtr<FPlayerTreeViewEntry>>)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &FAnimGraphParentPlayerDetails::OnGenerateRow)
		.OnGetChildren(this, &FAnimGraphParentPlayerDetails::OnGetChildren)
		.TreeItemsSource(&ListEntries)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+SHeaderRow::Column(FName("Name"))
			.FillWidth(0.5f)
			.DefaultLabel(LOCTEXT("ParentPlayer_NameCol", "Name"))

			+SHeaderRow::Column(FName("Asset"))
			.FillWidth(0.5f)
			.DefaultLabel(LOCTEXT("ParentPlayer_AssetCol", "Asset"))
		);

	// Expand top level (blueprint) entries so the panel seems less empty
	for(TSharedPtr<FPlayerTreeViewEntry> Entry : ListEntries)
	{
		TreeView->SetItemExpansion(Entry, true);
	}

	Row
	[
		TreeView->AsShared()
	];
}

TSharedRef<ITableRow> FAnimGraphParentPlayerDetails::OnGenerateRow(TSharedPtr<FPlayerTreeViewEntry> EntryPtr, const TSharedRef< STableViewBase >& OwnerTable)
{
	return SNew(SParentPlayerTreeRow, OwnerTable).Item(EntryPtr).OverrideObject(EditorObject).Persona(PersonaPtr);
}

void FAnimGraphParentPlayerDetails::OnGetChildren(TSharedPtr<FPlayerTreeViewEntry> InParent, TArray< TSharedPtr<FPlayerTreeViewEntry> >& OutChildren)
{
	OutChildren.Append(InParent->Children);
}

void SParentPlayerTreeRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	EditorObject = InArgs._OverrideObject;
	Persona = InArgs._Persona;

	if(Item->Override)
	{
		GraphNode = EditorObject->GetVisualNodeFromGuid(Item->Override->ParentNodeGuid);
	}
	else
	{
		GraphNode = NULL;
	}

	SMultiColumnTableRow<TSharedPtr<FAnimGraphParentPlayerDetails>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SParentPlayerTreeRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	TSharedPtr<SHorizontalBox> HorizBox;
	SAssignNew(HorizBox, SHorizontalBox);

	if(ColumnName == "Name")
	{
		HorizBox->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			];

		Item->GenerateNameWidget(HorizBox);
	}
	else if(Item->Override)
	{
		HorizBox->AddSlot()
			.Padding(2)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("FocusNodeButtonTip", "Open the graph that contains this node in read-only mode and focus on the node"), NULL, "Shared/Editors/Persona", "FocusNodeButton"))
				.OnClicked(FOnClicked::CreateSP(this, &SParentPlayerTreeRow::OnFocusNodeButtonClicked))
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("GenericViewButton"))
				]
				
			];
		
		TArray<const UClass*> AllowedClasses;
		AllowedClasses.Add(UAnimationAsset::StaticClass());
		HorizBox->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SObjectPropertyEntryBox)
				.ObjectPath(this, &SParentPlayerTreeRow::GetCurrentAssetPath)
				.OnShouldFilterAsset(this, &SParentPlayerTreeRow::OnShouldFilterAsset)
				.OnObjectChanged(this, &SParentPlayerTreeRow::OnAssetSelected)
				.AllowedClass(GetCurrentAssetToUse()->GetClass())
			];

		HorizBox->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.Visibility(this, &SParentPlayerTreeRow::GetResetToDefaultVisibility)
				.OnClicked(this, &SParentPlayerTreeRow::OnResetButtonClicked)
				.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("ResetToParentButtonTip", "Undo the override, returning to the default asset for this node"), NULL, "Shared/Editors/Persona", "ResetToParentButton"))
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			];
	}

	return HorizBox.ToSharedRef();
}

bool SParentPlayerTreeRow::OnShouldFilterAsset(const FAssetData& AssetData)
{
	const FString SkeletonName = AssetData.GetTagValueRef<FString>("Skeleton");

	if(!SkeletonName.IsEmpty())
	{
		USkeleton* CurrentSkeleton = GraphNode->GetAnimBlueprint()->TargetSkeleton;
		if(SkeletonName == FString::Printf(TEXT("%s'%s'"), *CurrentSkeleton->GetClass()->GetName(), *CurrentSkeleton->GetPathName()))
		{
			return false;
		}
	}

	return true;
}

void SParentPlayerTreeRow::OnAssetSelected(const FAssetData& AssetData)
{
	Item->Override->NewAsset = Cast<UAnimationAsset>(AssetData.GetAsset());
	EditorObject->ApplyOverrideToBlueprint(*Item->Override);
}

FReply SParentPlayerTreeRow::OnFocusNodeButtonClicked()
{
	TSharedPtr<FPersona> SharedPersona = Persona.Pin();
	if(SharedPersona.IsValid())
	{
		if(GraphNode)
		{
			UEdGraph* EdGraph = GraphNode->GetGraph();
			TSharedPtr<SGraphEditor> GraphEditor = SharedPersona->OpenGraphAndBringToFront(EdGraph);
			if (GraphEditor.IsValid())
			{
				GraphEditor->JumpToNode(GraphNode, false);
			}
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

const UAnimationAsset* SParentPlayerTreeRow::GetCurrentAssetToUse() const
{
	if(Item->Override->NewAsset)
	{
		return Item->Override->NewAsset;
	}
	
	if(GraphNode)
	{
		return GraphNode->GetAnimationAsset();
	}

	return NULL;
}

EVisibility SParentPlayerTreeRow::GetResetToDefaultVisibility() const
{
	FAnimParentNodeAssetOverride* HierarchyOverride = EditorObject->GetBlueprint()->GetAssetOverrideForNode(Item->Override->ParentNodeGuid, true);

	if(HierarchyOverride)
	{
		return Item->Override->NewAsset != HierarchyOverride->NewAsset ? EVisibility::Visible : EVisibility::Hidden;
	}

	return Item->Override->NewAsset != GraphNode->GetAnimationAsset() ? EVisibility::Visible : EVisibility::Hidden;
}

FReply SParentPlayerTreeRow::OnResetButtonClicked()
{
	FAnimParentNodeAssetOverride* HierarchyOverride = EditorObject->GetBlueprint()->GetAssetOverrideForNode(Item->Override->ParentNodeGuid, true);
	
	Item->Override->NewAsset = HierarchyOverride ? HierarchyOverride->NewAsset : GraphNode->GetAnimationAsset();

	// Apply will remove the override from the object
	EditorObject->ApplyOverrideToBlueprint(*Item->Override);
	return FReply::Handled();
}

FString SParentPlayerTreeRow::GetCurrentAssetPath() const
{
	const UAnimationAsset* Asset = GetCurrentAssetToUse();
	return Asset ? Asset->GetPathName() : FString("");
}

FORCENOINLINE bool FPlayerTreeViewEntry::operator==(const FPlayerTreeViewEntry& Other)
{
	return EntryName == Other.EntryName;
}

void FPlayerTreeViewEntry::GenerateNameWidget(TSharedPtr<SHorizontalBox> Box)
{
	// Get an appropriate image icon for the row
	const FSlateBrush* EntryImageBrush = NULL;
	switch(EntryType)
	{
		case EPlayerTreeViewEntryType::Blueprint:
			EntryImageBrush = FEditorStyle::GetBrush("ClassIcon.Blueprint");
			break;
		case EPlayerTreeViewEntryType::Graph:
			EntryImageBrush = FEditorStyle::GetBrush("GraphEditor.EventGraph_16x");
			break;
		case EPlayerTreeViewEntryType::Node:
			EntryImageBrush = FEditorStyle::GetBrush("GraphEditor.Default_16x");
			break;
		default:
			break;
	}

	Box->AddSlot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(EntryImageBrush)
		];

	Box->AddSlot()
		.VAlign(VAlign_Center)
		.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10))
			.Text(FText::FromString(EntryName))
		];
}

#undef LOCTEXT_NAMESPACE
