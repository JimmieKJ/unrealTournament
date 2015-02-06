// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "BodyInstanceCustomization.h"
#include "ScopedTransaction.h"
#include "Editor/Documentation/Public/IDocumentation.h"
#include "Engine/CollisionProfile.h"

#define LOCTEXT_NAMESPACE "BodyInstanceCustomization"

#define RowWidth_Customization 50

////////////////////////////////////////////////////////////////
FBodyInstanceCustomization::FBodyInstanceCustomization()
{
	CollisionProfile = UCollisionProfile::Get();

	RefreshCollisionProfiles();
}

void FBodyInstanceCustomization::RefreshCollisionProfiles()
{
	int32 NumProfiles = CollisionProfile->GetNumOfProfiles();
	// first create profile combo list
	CollisionProfileComboList.Empty(NumProfiles + 1);

	// first one is default one
	CollisionProfileComboList.Add(MakeShareable(new FString(TEXT("Custom..."))));

	// go through profile and see if it has mine
	for (int32 ProfileId = 0; ProfileId < NumProfiles; ++ProfileId)
	{
		CollisionProfileComboList.Add(MakeShareable(new FString(CollisionProfile->GetProfileByIndex(ProfileId)->Name.ToString())));
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBodyInstanceCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	CollisionProfileNameHandle = StructPropertyHandle->GetChildHandle(TEXT("CollisionProfileName"));
	CollisionEnabledHandle = StructPropertyHandle->GetChildHandle(TEXT("CollisionEnabled"));
	ObjectTypeHandle = StructPropertyHandle->GetChildHandle(TEXT("ObjectType"));

	CollisionResponsesHandle = StructPropertyHandle->GetChildHandle(TEXT("CollisionResponses"));

	check (CollisionProfileNameHandle.IsValid());
	check (CollisionEnabledHandle.IsValid());
	check (ObjectTypeHandle.IsValid());

	// copy all bodyinstances I'm accessing right now
	TArray<void*> StructPtrs;
	StructPropertyHandle->AccessRawData( StructPtrs );
	check(StructPtrs.Num()!=0);

	BodyInstances.AddUninitialized(StructPtrs.Num());
	for (auto Iter = StructPtrs.CreateIterator(); Iter; ++Iter)
	{
		check (*Iter);
		BodyInstances[Iter.GetIndex()] = (FBodyInstance*)(*Iter);
	}

	// need to find profile name
	FName ProfileName;
	TSharedPtr< FString > DisplayName = CollisionProfileComboList[0];
	bool bDisplayAdvancedCollisionSettings = true;

	// if I have valid profile name
	if (CollisionProfileNameHandle->GetValue(ProfileName) ==  FPropertyAccess::Result::Success && FBodyInstance::IsValidCollisionProfileName(ProfileName) )
	{
		DisplayName = GetProfileString(ProfileName);
		bDisplayAdvancedCollisionSettings = false;
	}

	const FString PresetsDocLink = TEXT("Shared/Collision");
	TSharedPtr<SToolTip> ProfileTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("SelectCollisionPreset", "Select collision presets. You can set this data in Project settings."), NULL, PresetsDocLink, TEXT("PresetDetail"));

	IDetailGroup& CollisionGroup = StructBuilder.AddChildGroup( TEXT("Collision"), LOCTEXT("CollisionPresetsLabel", "Collision Presets") );
	CollisionGroup.HeaderRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("CollisionPresetsLabel", "Collision Presets"))
		.ToolTip(ProfileTooltip)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(CollsionProfileComboBox, SComboBox< TSharedPtr<FString> >)
			.OptionsSource(&CollisionProfileComboList)
			.OnGenerateWidget(this, &FBodyInstanceCustomization::MakeCollisionProfileComboWidget)
			.OnSelectionChanged(this, &FBodyInstanceCustomization::OnCollisionProfileChanged, &CollisionGroup )
			.OnComboBoxOpening(this, &FBodyInstanceCustomization::OnCollisionProfileComboOpening)
			.InitiallySelectedItem(DisplayName)
			.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
			.ContentPadding(2)
			.Content()
			[
				SNew( STextBlock )
				.Text(this, &FBodyInstanceCustomization::GetCollisionProfileComboBoxContent)
				.Font( IDetailLayoutBuilder::GetDetailFont() )
				.ToolTipText(this, &FBodyInstanceCustomization::GetCollisionProfileComboBoxToolTip)
			]
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding( 2.0f )
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this, &FBodyInstanceCustomization::SetToDefaultProfile)
			.Visibility(this, &FBodyInstanceCustomization::ShouldShowResetToDefaultProfile)
			.ContentPadding(0.f)
			.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.Content()
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
			]
		]
	];

	CollisionGroup.ToggleExpansion(bDisplayAdvancedCollisionSettings);
	// now create custom set up
	CreateCustomCollisionSetup( StructPropertyHandle, CollisionGroup );
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

int32 FBodyInstanceCustomization::InitializeObjectTypeComboList()
{
	ObjectTypeComboList.Empty();
	ObjectTypeValues.Empty();

	UEnum * Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ECollisionChannel"), true);
	const FString KeyName = TEXT("DisplayName");
	const FString QueryType = TEXT("TraceQuery");

	int32 NumEnum = Enum->NumEnums();
	int32 Selected = 0;
	uint8 ObjectTypeIndex = 0;
	if (ObjectTypeHandle->GetValue(ObjectTypeIndex) != FPropertyAccess::Result::Success)
	{
		ObjectTypeIndex = 0; // if multi, just let it be 0
	}

	// go through enum and fill up the list
	for (int32 EnumIndex = 0; EnumIndex < NumEnum; ++EnumIndex)
	{
		// make sure the enum entry is object channel
		FString MetaData = Enum->GetMetaData(*QueryType, EnumIndex);
		// if query type is object, we allow it to be on movement channel
		if (MetaData.Len() == 0 || MetaData[0] == '0')
		{
			MetaData = Enum->GetMetaData(*KeyName, EnumIndex);

			if ( MetaData.Len() > 0 )
			{
				int32 NewIndex = ObjectTypeComboList.Add( MakeShareable( new FString (MetaData) ));
				// @todo: I don't think this would work well if we customize entry, but I don't think we can do that yet
				// i.e. enum a { a1=5, a2=6 }
				ObjectTypeValues.Add((ECollisionChannel)EnumIndex);

				// this solution poses problem when the item was saved with ALREADY INVALID movement channel
				// that will automatically select 0, but I think that is the right solution
				if (ObjectTypeIndex == EnumIndex)
				{
					Selected = NewIndex;
				}
			}
		}
	}

	// it can't be zero. If so you need to fix it
	check ( ObjectTypeComboList.Num() > 0 );

	return Selected;
}

TSharedPtr<FString> FBodyInstanceCustomization::GetProfileString(FName ProfileName) const
{
	FString ProfileNameString = ProfileName.ToString();

	// go through profile and see if it has mine
	int32 NumProfiles = CollisionProfile->GetNumOfProfiles();
	// refresh collision count
	if( NumProfiles+1==CollisionProfileComboList.Num() )
	{
		for(int32 ProfileId = 0; ProfileId < NumProfiles; ++ProfileId)
		{
			if(*CollisionProfileComboList[ProfileId+1].Get() == ProfileNameString)
			{
				return CollisionProfileComboList[ProfileId+1];
			}
		}
	}

	return CollisionProfileComboList[0];
}

// filter through find valid index of enum values matching each item
// this needs refresh when displayname of the enum has change
// which can happen when we have engine project settings in place working
void FBodyInstanceCustomization::UpdateValidCollisionChannels()
{
	// find the enum
	UEnum * Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ECollisionChannel"), true);
	// we need this Enum
	check (Enum);
	const FString KeyName = TEXT("DisplayName");
	const FString TraceType = TEXT("TraceQuery");

	// need to initialize displaynames separate
	int32 NumEnum = Enum->NumEnums();
	ValidCollisionChannels.Empty(NumEnum);

	// first go through enum entry, and add suffix to displaynames
	for ( int32 EnumIndex=0; EnumIndex<NumEnum; ++EnumIndex )
	{
		const FString MetaData = Enum->GetMetaData(*KeyName, EnumIndex);
		if ( MetaData.Len() > 0 )
		{
			FCollisionChannelInfo Info;
			Info.DisplayName = MetaData;
			Info.CollisionChannel = (ECollisionChannel)EnumIndex;
			if (Enum->GetMetaData(*TraceType, EnumIndex) == TEXT("1"))
			{
				Info.TraceType = true;
			}
			else
			{
				Info.TraceType = false;
			}

			ValidCollisionChannels.Add(Info);
		}
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBodyInstanceCustomization::CreateCustomCollisionSetup( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailGroup& CollisionGroup )
{
	UpdateValidCollisionChannels();

	if (ValidCollisionChannels.Num() == 0)
	{
		return;
	}

	int32 TotalNumChildren = ValidCollisionChannels.Num();
	TAttribute<bool> CustomCollisionEnabled( this, &FBodyInstanceCustomization::ShouldEnableCustomCollisionSetup );
	TAttribute<EVisibility> CustomCollisionVisibility(this, &FBodyInstanceCustomization::ShouldShowCustomCollisionSetup);

	// initialize ObjectTypeComboList
	// we only display things that has "DisplayName"
	int32 IndexSelected = InitializeObjectTypeComboList();

	CollisionGroup.AddPropertyRow(CollisionEnabledHandle.ToSharedRef())
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility);

	if (!StructPropertyHandle->GetProperty()->GetBoolMetaData(TEXT("HideObjectType")))
	{
		CollisionGroup.AddWidgetRow()
		.IsEnabled(CustomCollisionEnabled)
		.Visibility(CustomCollisionVisibility)
		.NameContent()
		[
			ObjectTypeHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SAssignNew(ObjectTypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&ObjectTypeComboList)
			.OnGenerateWidget(this, &FBodyInstanceCustomization::MakeObjectTypeComboWidget)
			.OnSelectionChanged(this, &FBodyInstanceCustomization::OnObjectTypeChanged)
			.InitiallySelectedItem(ObjectTypeComboList[IndexSelected])
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
			.ContentPadding(2)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FBodyInstanceCustomization::GetObjectTypeComboBoxContent)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}

	// Add Title
	CollisionGroup.AddWidgetRow()
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility)
	.ValueContent()
	.MaxDesiredWidth(0)
	.MinDesiredWidth(0)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(RowWidth_Customization)
			.HAlign( HAlign_Left )
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IgnoreCollisionLabel", "Ignore"))
				.Font( IDetailLayoutBuilder::GetDetailFontBold() )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.HAlign( HAlign_Left )
			.WidthOverride(RowWidth_Customization)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OverlapCollisionLabel", "Overlap"))
				.Font( IDetailLayoutBuilder::GetDetailFontBold() )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BlockCollisionLabel", "Block"))
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		]
	];

	// Add All check box
	CollisionGroup.AddWidgetRow()
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility)
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( 2.0f )
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew( STextBlock )
			.Text(LOCTEXT("CollisionResponsesLabel", "Collision Responses"))
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
			.ToolTipText(LOCTEXT("CollsionResponse_ToolTip", "When trace by channel, this information will be used for filtering."))
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			IDocumentation::Get()->CreateAnchor( TEXT("Engine/Physics/Collision") )
		]
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(RowWidth_Customization)
			.Content()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnAllCollisionChannelChanged, ECR_Ignore )
				.IsChecked( this, &FBodyInstanceCustomization::IsAllCollisionChannelChecked, ECR_Ignore )
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(RowWidth_Customization)
			.Content()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnAllCollisionChannelChanged, ECR_Overlap )
				.IsChecked( this, &FBodyInstanceCustomization::IsAllCollisionChannelChecked, ECR_Overlap )
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(RowWidth_Customization)
			.Content()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnAllCollisionChannelChanged, ECR_Block )
				.IsChecked( this, &FBodyInstanceCustomization::IsAllCollisionChannelChecked, ECR_Block )
			]
		]
	];

	// add header
	// Add Title
	CollisionGroup.AddWidgetRow()
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility)
	.NameContent()
	[
		SNew(SBox)
		.Padding(FMargin(10, 0))
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CollisionTraceResponsesLabel", "Trace Responses"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
	];

	// each channel set up
	FName MetaData(TEXT("DisplayName"));
	// Add option for each channel - first do trace
	for ( int32 Index=0; Index<TotalNumChildren; ++Index )
	{
		if (ValidCollisionChannels[Index].TraceType)
		{
			FString DisplayName = ValidCollisionChannels[Index].DisplayName;
			EVisibility Visibility = EVisibility::Visible;

			CollisionGroup.AddWidgetRow()
			.IsEnabled(CustomCollisionEnabled)
			.Visibility(CustomCollisionVisibility)
			.NameContent()
			[
				SNew(SBox)
				.Padding(FMargin(15, 0))
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString(DisplayName))
					.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
			]
			.ValueContent()
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnCollisionChannelChanged, Index, ECR_Ignore )
						.IsChecked( this, &FBodyInstanceCustomization::IsCollisionChannelChecked, Index, ECR_Ignore )
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnCollisionChannelChanged, Index, ECR_Overlap )
						.IsChecked( this, &FBodyInstanceCustomization::IsCollisionChannelChecked, Index, ECR_Overlap )
					]

				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnCollisionChannelChanged, Index, ECR_Block )
					.IsChecked( this, &FBodyInstanceCustomization::IsCollisionChannelChecked, Index, ECR_Block )
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.OnClicked(this, &FBodyInstanceCustomization::SetToDefaultResponse, Index)
					.Visibility(this, &FBodyInstanceCustomization::ShouldShowResetToDefaultResponse, Index)
					.ContentPadding(0.f)
					.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.Content()
					[
						SNew(SImage)
						.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
					]
				]
			];
		}
	}

	// Add Title
	CollisionGroup.AddWidgetRow()
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility)
	.NameContent()
	[
		SNew(SBox)
		.Padding(FMargin(10, 0))
		.Content()
		[
			SNew( STextBlock )
			.Text(LOCTEXT("CollisionObjectResponses", "Object Responses"))
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		]
	];

	for ( int32 Index=0; Index<TotalNumChildren; ++Index )
	{
		if (!ValidCollisionChannels[Index].TraceType)
		{
			FString DisplayName = ValidCollisionChannels[Index].DisplayName;
			EVisibility Visibility = EVisibility::Visible;

			CollisionGroup.AddWidgetRow()
			.IsEnabled(CustomCollisionEnabled)
			.Visibility(CustomCollisionVisibility)
			.NameContent()
			[
				SNew(SBox)
				.Padding(FMargin(15, 0))
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString(DisplayName))
					.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
			]
			.ValueContent()
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnCollisionChannelChanged, Index, ECR_Ignore )
						.IsChecked( this, &FBodyInstanceCustomization::IsCollisionChannelChecked, Index, ECR_Ignore )
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnCollisionChannelChanged, Index, ECR_Overlap )
						.IsChecked( this, &FBodyInstanceCustomization::IsCollisionChannelChecked, Index, ECR_Overlap )
					]

				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged( this, &FBodyInstanceCustomization::OnCollisionChannelChanged, Index, ECR_Block )
						.IsChecked( this, &FBodyInstanceCustomization::IsCollisionChannelChecked, Index, ECR_Block )
					]
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.OnClicked(this, &FBodyInstanceCustomization::SetToDefaultResponse, Index)
					.Visibility(this, &FBodyInstanceCustomization::ShouldShowResetToDefaultResponse, Index)
					.ContentPadding(0.f)
					.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.Content()
					[
						SNew(SImage)
						.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
					]
				]
			];
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> FBodyInstanceCustomization::MakeObjectTypeComboWidget( TSharedPtr<FString> InItem )
{
	return SNew(STextBlock) .Text( FText::FromString(*InItem) ) .Font( IDetailLayoutBuilder::GetDetailFont() );
}

void FBodyInstanceCustomization::OnObjectTypeChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo  )
{
	// if it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString NewValue = *NewSelection.Get();
		uint8 NewEnumVal = ECC_WorldStatic;
		// find index of NewValue
		for (auto Iter = ObjectTypeComboList.CreateConstIterator(); Iter; ++Iter)
		{
			// if value is same
			if (*(Iter->Get()) == NewValue)
			{
				NewEnumVal = ObjectTypeValues[Iter.GetIndex()];
			}
		}
		ensure(ObjectTypeHandle->SetValue(NewEnumVal) == FPropertyAccess::Result::Success);
	}
}

FText FBodyInstanceCustomization::GetObjectTypeComboBoxContent() const
{
	FName ObjectTypeName;
	if (ObjectTypeHandle->GetValue(ObjectTypeName) == FPropertyAccess::Result::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return FText::FromString(*ObjectTypeComboBox.Get()->GetSelectedItem().Get());
}

TSharedRef<SWidget> FBodyInstanceCustomization::MakeCollisionProfileComboWidget(TSharedPtr<FString> InItem)
{
	FString ProfileMessage;

	FCollisionResponseTemplate ProfileData;
	if (CollisionProfile->GetProfileTemplate(FName(**InItem), ProfileData))
	{
		ProfileMessage = ProfileData.HelpMessage;
	}

	return
		SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.ToolTipText(FText::FromString(ProfileMessage))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE! I have a lot of ensure to make sure it's set correctly
// in case for if type changes or any set up changes, this won't work, but ensure will remind you that! :)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBodyInstanceCustomization::OnCollisionProfileComboOpening()
{
	FName ProfileName;
	if (CollisionProfileNameHandle->GetValue(ProfileName) != FPropertyAccess::Result::MultipleValues)
	{
		TSharedPtr<FString> ComboStringPtr = GetProfileString(ProfileName);
		if( ComboStringPtr.IsValid() )
		{
			CollsionProfileComboBox->SetSelectedItem(ComboStringPtr);
		}
	}
}

void FBodyInstanceCustomization::OnCollisionProfileChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo, IDetailGroup* CollisionGroup )
{
	// if it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString NewValue = *NewSelection.Get();
		int32 NumProfiles = CollisionProfile->GetNumOfProfiles();
		for (int32 ProfileId = 0; ProfileId < NumProfiles; ++ProfileId)
		{
			const FCollisionResponseTemplate* CurProfile = CollisionProfile->GetProfileByIndex(ProfileId);
			if ( NewValue == CurProfile->Name.ToString() )
			{
				// trigget transaction before UpdateCollisionProfile
				const FScopedTransaction Transaction( LOCTEXT( "ChangeCollisionProfile", "Change Collision Profile" ) );
				// set profile set up
				ensure ( CollisionProfileNameHandle->SetValue(NewValue) ==  FPropertyAccess::Result::Success );
				UpdateCollisionProfile();
				return;
			}
		}

		if( NewSelection == CollisionProfileComboList[0] )
		{
			// Force expansion when the user chooses the selected item
			CollisionGroup->ToggleExpansion( true );
		}
		// if none of them found, clear it
		FName Name=UCollisionProfile::CustomCollisionProfileName;
		ensure ( CollisionProfileNameHandle->SetValue(Name) ==  FPropertyAccess::Result::Success );
	}
}

void FBodyInstanceCustomization::UpdateCollisionProfile()
{
	FName ProfileName;

	// if I have valid profile name
	if (CollisionProfileNameHandle->GetValue(ProfileName) ==  FPropertyAccess::Result::Success && FBodyInstance::IsValidCollisionProfileName(ProfileName) )
	{
		int32 NumProfiles = CollisionProfile->GetNumOfProfiles();
		for (int32 ProfileId = 0; ProfileId < NumProfiles; ++ProfileId)
		{
			// find the profile
			const FCollisionResponseTemplate* CurProfile = CollisionProfile->GetProfileByIndex(ProfileId);
			if (ProfileName == CurProfile->Name)
			{
				// set the profile set up
				ensure(CollisionEnabledHandle->SetValue((uint8)CurProfile->CollisionEnabled) == FPropertyAccess::Result::Success);
				ensure(ObjectTypeHandle->SetValue((uint8)CurProfile->ObjectType) == FPropertyAccess::Result::Success);

				SetCollisionResponseContainer(CurProfile->ResponseToChannels);

				// now update combo box
				CollsionProfileComboBox.Get()->SetSelectedItem(CollisionProfileComboList[ProfileId+1]);
				if (ObjectTypeComboBox.IsValid())
				{
					for (auto Iter = ObjectTypeValues.CreateConstIterator(); Iter; ++Iter)
					{
						if (*Iter == CurProfile->ObjectType)
						{
							ObjectTypeComboBox.Get()->SetSelectedItem(ObjectTypeComboList[Iter.GetIndex()]);
							break;
						}
					}
				}

				return;
			}
		}
	}

	CollsionProfileComboBox.Get()->SetSelectedItem(CollisionProfileComboList[0]);
}

FReply FBodyInstanceCustomization::SetToDefaultProfile()
{
	// trigger transaction before UpdateCollisionProfile
	const FScopedTransaction Transaction( LOCTEXT( "ResetCollisionProfile", "Reset Collision Profile" ) );
	CollisionProfileNameHandle.Get()->ResetToDefault();
	UpdateCollisionProfile();
	return FReply::Handled();
}

EVisibility FBodyInstanceCustomization::ShouldShowResetToDefaultProfile() const
{
	if (CollisionProfileNameHandle.Get()->DiffersFromDefault())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

FReply FBodyInstanceCustomization::SetToDefaultResponse(int32 ValidIndex)
{
	if ( ValidCollisionChannels.IsValidIndex(ValidIndex) )
	{
		const FScopedTransaction Transaction( LOCTEXT( "ResetCollisionResponse", "Reset Collision Response" ) );
		const ECollisionResponse DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(ValidCollisionChannels[ValidIndex].CollisionChannel);

		SetResponse(ValidIndex, DefaultResponse);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

EVisibility FBodyInstanceCustomization::ShouldShowResetToDefaultResponse(int32 ValidIndex) const
{
	if ( ValidCollisionChannels.IsValidIndex(ValidIndex) )
	{
		const ECollisionResponse DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(ValidCollisionChannels[ValidIndex].CollisionChannel);

		if (IsCollisionChannelChecked(ValidIndex, DefaultResponse) != ECheckBoxState::Checked)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}

bool FBodyInstanceCustomization::ShouldEnableCustomCollisionSetup() const
{
	FName ProfileName;
	if (CollisionProfileNameHandle->GetValue(ProfileName) == FPropertyAccess::Result::Success && FBodyInstance::IsValidCollisionProfileName(ProfileName) == false)
	{
		return true;
	}

	return false;
}

EVisibility FBodyInstanceCustomization::ShouldShowCustomCollisionSetup() const
{
	return EVisibility::Visible;//return (bDisplayAdvancedCollisionSettings)? EVisibility::Visible : EVisibility::Hidden;
}

FText FBodyInstanceCustomization::GetCollisionProfileComboBoxContent() const
{
	FName ProfileName;
	if (CollisionProfileNameHandle->GetValue(ProfileName) == FPropertyAccess::Result::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return FText::FromString(*GetProfileString(ProfileName).Get());
}

FText FBodyInstanceCustomization::GetCollisionProfileComboBoxToolTip() const
{
	FName ProfileName;
	if (CollisionProfileNameHandle->GetValue(ProfileName) == FPropertyAccess::Result::Success)
	{
		FCollisionResponseTemplate ProfileData;
		if ( CollisionProfile->GetProfileTemplate(ProfileName, ProfileData) )
		{
			return FText::FromString(ProfileData.HelpMessage);
		}
		return FText::GetEmpty();
	}

	return LOCTEXT("MultipleValues", "Multiple Values");
}

void FBodyInstanceCustomization::OnCollisionChannelChanged(ECheckBoxState InNewValue, int32 ValidIndex, ECollisionResponse InCollisionResponse)
{
	if ( ValidCollisionChannels.IsValidIndex(ValidIndex) )
	{
		SetResponse(ValidIndex, InCollisionResponse);
	}
}

void FBodyInstanceCustomization::SetResponse(int32 ValidIndex, ECollisionResponse InCollisionResponse)
{
	const FScopedTransaction Transaction( LOCTEXT( "ChangeIndividualChannel", "Change Individual Channel" ) );

	CollisionResponsesHandle->NotifyPreChange();

	for (auto Iter=BodyInstances.CreateIterator(); Iter; ++Iter)
	{
		FBodyInstance* BodyInstance = *Iter;

		BodyInstance->CollisionResponses.SetResponse(ValidCollisionChannels[ValidIndex].CollisionChannel, InCollisionResponse);
	}

	CollisionResponsesHandle->NotifyPostChange();
}

ECheckBoxState FBodyInstanceCustomization::IsCollisionChannelChecked( int32 ValidIndex, ECollisionResponse InCollisionResponse) const
{
	TArray<uint8> CollisionResponses;

	if ( ValidCollisionChannels.IsValidIndex(ValidIndex) )
	{
		for (auto Iter=BodyInstances.CreateConstIterator(); Iter; ++Iter)
		{
			FBodyInstance* BodyInstance = *Iter;

			CollisionResponses.AddUnique(BodyInstance->CollisionResponses.GetResponse(ValidCollisionChannels[ValidIndex].CollisionChannel));
		}

		if (CollisionResponses.Num() == 1)
		{
			if (CollisionResponses[0] == InCollisionResponse)
			{
				return ECheckBoxState::Checked;
			}
			else
			{
				return ECheckBoxState::Unchecked;
			}
		}
		else if (CollisionResponses.Contains(InCollisionResponse))
		{
			return ECheckBoxState::Undetermined;
		}

		// if it didn't contain and it's not found, return Unchecked
		return ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void FBodyInstanceCustomization::OnAllCollisionChannelChanged(ECheckBoxState InNewValue, ECollisionResponse InCollisionResponse)
{
	FCollisionResponseContainer NewContainer;
	NewContainer.SetAllChannels(InCollisionResponse);
	SetCollisionResponseContainer(NewContainer);
}

ECheckBoxState FBodyInstanceCustomization::IsAllCollisionChannelChecked(ECollisionResponse InCollisionResponse) const
{
	ECheckBoxState State = ECheckBoxState::Undetermined;

	uint32 TotalNumChildren = ValidCollisionChannels.Num();
	if (TotalNumChildren >= 1)
	{
		State = IsCollisionChannelChecked(0, InCollisionResponse);

		for (uint32 Index = 1; Index < TotalNumChildren; ++Index)
		{
			if (State != IsCollisionChannelChecked(Index, InCollisionResponse))
			{
				State = ECheckBoxState::Undetermined;
				break;
			}
		}
	}

	return State;
}

void FBodyInstanceCustomization::SetCollisionResponseContainer(const FCollisionResponseContainer& ResponseContainer)
{
	// trigget transaction before UpdateCollisionProfile
	const FScopedTransaction Transaction( LOCTEXT( "Collision", "Collision Channel Changes" ) );

	CollisionResponsesHandle->NotifyPreChange();

	uint32 TotalNumChildren = ValidCollisionChannels.Num();
	// only go through valid channels
	for (uint32 Index = 0; Index < TotalNumChildren; ++Index)
	{
		ECollisionChannel Channel = ValidCollisionChannels[Index].CollisionChannel;
		ECollisionResponse Response = ResponseContainer.GetResponse(Channel);

		// iterate through bodyinstance and fix it
		for (FBodyInstance* BodyInstance : BodyInstances)
		{
			BodyInstance->CollisionResponses.SetResponse(Channel, Response);
		}
	}

	CollisionResponsesHandle->NotifyPostChange();
}

////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
#undef RowWidth_Customization
