// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "PhysicsSettingsDetails.h"
#include "ScopedTransaction.h"
#include "ObjectEditorUtils.h"
#include "BodyInstanceCustomization.h"
#include "IDocumentation.h"
#include "STableViewBase.h"

#define LOCTEXT_NAMESPACE "PhysicalSurfaceDetails"

//====================================================================================
// SPhysicalSurfaceListItem 
//=====================================================================================

void SPhysicalSurfaceListItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	PhysicalSurface = InArgs._PhysicalSurface;
	PhysicalSurfaceEnum = InArgs._PhysicalSurfaceEnum;
	OnCommitChange = InArgs._OnCommitChange;
	check(PhysicalSurface.IsValid() && PhysicalSurfaceEnum);
	SMultiColumnTableRow< TSharedPtr<FPhysicalSurfaceListItem> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

FString SPhysicalSurfaceListItem::GetTypeString() const
{
	// i need to convert to enum name
	int32 EnumIndex = (int32)PhysicalSurface->Type;
	return PhysicalSurfaceEnum->GetEnumName(EnumIndex);
}

TSharedRef<SWidget> SPhysicalSurfaceListItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == TEXT("Type"))
	{
		return	SNew(SBox)
			.HeightOverride(20)
			.Padding(FMargin(5, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(GetTypeString()))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}
	else if (ColumnName == TEXT("Name"))
	{
		return	SNew(SBox)
			.HeightOverride(20)
			.Padding(FMargin(5, 2))
			.VAlign(VAlign_Center)
			[
				SAssignNew(NameBox, SEditableTextBox)
				.MinDesiredWidth(128.0f)
				.Text(this, &SPhysicalSurfaceListItem::GetName)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.OnTextCommitted(this, &SPhysicalSurfaceListItem::NewNameEntered)
				.OnTextChanged(this, &SPhysicalSurfaceListItem::OnTextChanged)
				.IsReadOnly(PhysicalSurface->Type == SurfaceType_Default)
			];
	}

	return SNullWidget::NullWidget;
}

void SPhysicalSurfaceListItem::OnTextChanged(const FText& NewText)
{
	FString NewName = NewText.ToString();

	if(NewName.Find(TEXT(" "))!=INDEX_NONE)
	{
		// no white space
		NameBox->SetError(TEXT("No white space is allowed"));
	}
	else
	{
		NameBox->SetError(TEXT(""));
	}
}

void SPhysicalSurfaceListItem::NewNameEntered(const FText& NewText, ETextCommit::Type CommitInfo)
{
	// Don't digest the number if we just clicked away from the pop-up
	if((CommitInfo == ETextCommit::OnEnter) || (CommitInfo == ETextCommit::OnUserMovedFocus))
	{
		FString NewName = NewText.ToString();
		if(NewName.Find(TEXT(" "))==INDEX_NONE)
		{
			FName NewSurfaceName(*NewName);
			if(PhysicalSurface->Name!=NAME_None && NewSurfaceName == NAME_None)
			{
				if(FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("SPhysicalSurfaceListItem_DeleteConfirm", "Would you like to delete the name? If this type is used, it will invalidate the usage.")) == EAppReturnType::No)
				{
					return;
				}
			}
			if(NewSurfaceName != PhysicalSurface->Name)
			{
				PhysicalSurface->Name = NewSurfaceName;
				OnCommitChange.ExecuteIfBound();
			}
		}
		else
		{
			// clear error
			NameBox->SetError(TEXT(""));
		}
	}
}

FText SPhysicalSurfaceListItem::GetName() const
{
	return FText::FromName(PhysicalSurface->Name);
}

//====================================================================================
// FPhysicsSettingsDetails
//=====================================================================================
TSharedRef<IDetailCustomization> FPhysicsSettingsDetails::MakeInstance()
{
	return MakeShareable( new FPhysicsSettingsDetails );
}

void FPhysicsSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
  	IDetailCategoryBuilder& PhysicalSurfaceCategory = DetailBuilder.EditCategory("Physical Surface", FText::GetEmpty(), ECategoryPriority::Uncommon);

	PhysicsSettings= UPhysicsSettings::Get();
	check(PhysicsSettings);

	PhysicalSurfaceEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPhysicalSurface"), true);
	check(PhysicalSurfaceEnum);

	RefreshPhysicalSurfaceList();

	const FString PhysicalSurfaceDocLink = TEXT("Shared/Physics");
	TSharedPtr<SToolTip> PhysicalSurfaceTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("PhysicalSurface", "Edit physical surface."), NULL, PhysicalSurfaceDocLink, TEXT("PhysicalSurface"));

	// Customize collision section
	PhysicalSurfaceCategory.AddCustomRow(LOCTEXT("FPhysicsSettingsDetails_PhysicalSurface", "Physical Surface"))
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.Padding(5)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTip(PhysicalSurfaceTooltip)
			.AutoWrapText(true)
			.Text(LOCTEXT("PhysicalSurface_Menu_Description", " You can have up to 62 custom surface types for your project. \nOnce you name each type, they will show up as surface type in the physical material."))
		]

		+SVerticalBox::Slot()
		.Padding(5)
		.FillHeight(1)
		[
			SAssignNew(PhysicalSurfaceListView, SPhysicalSurfaceListView)
			.ItemHeight(15.f)
			.ListItemsSource(&PhysicalSurfaceList)
			.OnGenerateRow(this, &FPhysicsSettingsDetails::HandleGenerateListWidget)
			.SelectionMode(ESelectionMode::None)

			.HeaderRow(
				SNew(SHeaderRow)
				+ SHeaderRow::Column("Type")
				.HAlignCell(HAlign_Left)
				.FillWidth(1)
				.HeaderContentPadding(FMargin(0, 3))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CPhysicalSurfaceListHeader_Type", "Type"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				// Name
				+ SHeaderRow::Column("Name")
				.HAlignCell(HAlign_Left)
				.FillWidth(1)
				.HeaderContentPadding(FMargin(0, 3))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PhysicalSurfaceListHeader_Name", "Name"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			)
		]
	];
}

void FPhysicsSettingsDetails::UpdatePhysicalSurfaceList()
{
	RefreshPhysicalSurfaceList();
	PhysicalSurfaceListView->RequestListRefresh();
	PhysicsSettings->LoadSurfaceType();
	PhysicsSettings->UpdateDefaultConfigFile();
}

void FPhysicsSettingsDetails::RefreshPhysicalSurfaceList()
{
	// make sure no duplicate exists
	// if exists, use the last one
	for(auto Iter = PhysicsSettings->PhysicalSurfaces.CreateIterator(); Iter; ++Iter)
	{
		for(auto InnerIter = Iter+1; InnerIter; ++InnerIter)
		{
			// see if same type
			if(Iter->Type == InnerIter->Type)
			{
				// remove the current one
				PhysicsSettings->PhysicalSurfaces.RemoveAt(Iter.GetIndex());
				--Iter;
				break;
			}
		}
	}

	bool bCreatedItem[SurfaceType_Max];
	FGenericPlatformMemory::Memzero(bCreatedItem, sizeof(bCreatedItem));

	PhysicalSurfaceList.Empty();

	// I'm listing all of these because it is easier for users to understand how does this work. 
	// I can't just link behind of scene because internally it will only save the enum
	// for example if you name SurfaceType5 to be Water and later changed to Sand, everything that used
	// SurfaceType5 will changed to Sand
	// I think what might be better is to show what options they have, and it's for them to choose how to name

	// add the first one by default
	{
		bCreatedItem[0] = true;
		PhysicalSurfaceList.Add(MakeShareable(new FPhysicalSurfaceListItem(MakeShareable(new FPhysicalSurfaceName(SurfaceType_Default, TEXT("Default"))))));
	}

	// we don't create the first one. First one is always default. 
	for(auto Iter = PhysicsSettings->PhysicalSurfaces.CreateIterator(); Iter; ++Iter)
	{
		bCreatedItem[Iter->Type] = true;
		PhysicalSurfaceList.Add(MakeShareable(new FPhysicalSurfaceListItem(MakeShareable(new FPhysicalSurfaceName(*Iter)))));
	}

	for(int32 Index = (int32)SurfaceType1; Index < SurfaceType_Max; ++Index)
	{
		if(bCreatedItem[Index] == false)
		{
			FPhysicalSurfaceName NeweElement((EPhysicalSurface)Index, TEXT(""));
			PhysicalSurfaceList.Add(MakeShareable(new FPhysicalSurfaceListItem(MakeShareable(new FPhysicalSurfaceName(NeweElement)))));
		}
	}

	// sort PhysicalSurfaceList by Type

	struct FComparePhysicalSurface
	{
		FORCEINLINE bool operator()(const TSharedPtr<FPhysicalSurfaceListItem> A, const TSharedPtr<FPhysicalSurfaceListItem> B) const
		{
			check(A.IsValid());
			check(B.IsValid());
			return A->PhysicalSurface->Type < B->PhysicalSurface->Type;
		}
	};

	PhysicalSurfaceList.Sort(FComparePhysicalSurface());
}

TSharedRef<ITableRow> FPhysicsSettingsDetails::HandleGenerateListWidget(TSharedPtr< FPhysicalSurfaceListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPhysicalSurfaceListItem, OwnerTable)
		.PhysicalSurface(InItem->PhysicalSurface)
		.PhysicalSurfaceEnum(PhysicalSurfaceEnum)
		.OnCommitChange(this, &FPhysicsSettingsDetails::OnCommitChange);
}

void FPhysicsSettingsDetails::OnCommitChange()
{
	bool bDoCommit=true;
	// make sure it verifies all data is correct
	// skip the first one
	for(auto Iter = PhysicalSurfaceList.CreateConstIterator()+1; Iter; ++Iter)
	{
		TSharedPtr<FPhysicalSurfaceListItem> ListItem = *Iter;
		if(ListItem->PhysicalSurface->Name != NAME_None)
		{
			// make sure no same name exists
			for(auto InnerIter= Iter+1; InnerIter; ++InnerIter)
			{
				TSharedPtr<FPhysicalSurfaceListItem> InnerItem = *InnerIter;
				if(ListItem->PhysicalSurface->Name == InnerItem->PhysicalSurface->Name)
				{
					// duplicate name, warn uer and get out
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FPhysicsSettingsDetails_InvalidName", "Duplicate name found."));
					bDoCommit = false;
					break;
				}
			}
		}
	}

	if(bDoCommit)
	{
		PhysicsSettings->PhysicalSurfaces.Empty();
		for(auto Iter = PhysicalSurfaceList.CreateConstIterator()+1; Iter; ++Iter)
		{
			TSharedPtr<FPhysicalSurfaceListItem> ListItem = *Iter;
			if(ListItem->PhysicalSurface->Name != NAME_None)
			{
				PhysicsSettings->PhysicalSurfaces.Add(FPhysicalSurfaceName(ListItem->PhysicalSurface->Type, ListItem->PhysicalSurface->Name));
			}
		}
	}

	// if so, refresh window
	UpdatePhysicalSurfaceList();
}
#undef LOCTEXT_NAMESPACE

