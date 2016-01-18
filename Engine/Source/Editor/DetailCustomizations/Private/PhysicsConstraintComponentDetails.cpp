// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "PhysicsConstraintComponentDetails.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"

#define LOCTEXT_NAMESPACE "PhysicsConstraintComponentDetails"

TSharedRef<IDetailCustomization> FPhysicsConstraintComponentDetails::MakeInstance()
{
	return MakeShareable(new FPhysicsConstraintComponentDetails());
}

void FPhysicsConstraintComponentDetails::AddConstraintPresets(IDetailLayoutBuilder& DetailBuilder, APhysicsConstraintActor* OwningConstraintActor)
{
	if (OwningConstraintActor != NULL)
	{
		DetailBuilder.EditCategory("Joint Presets")
			.AddCustomRow(FText::GetEmpty())
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0, 2.0f, 0, 0)
				.FillHeight(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.OnClicked(this, &FPhysicsConstraintComponentDetails::OnHingeClicked)
						.Text(LOCTEXT("HingePreset", "Hinge"))
						.ToolTipText(LOCTEXT("HingePresetTooltip", "Setup joint as hinge. A hinge joint allows motion only in one plane."))
					]
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SButton)
							.VAlign(VAlign_Center)
							.OnClicked(this, &FPhysicsConstraintComponentDetails::OnPrismaticClicked)
							.Text(LOCTEXT("PrismaticPreset", "Prismatic"))
							.ToolTipText(LOCTEXT("PrismaticPresetTooltip", "Setup joint as prismatic. A prismatic joint allows only linear sliding movement."))
						]
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SButton)
							.VAlign(VAlign_Center)
							.OnClicked(this, &FPhysicsConstraintComponentDetails::OnBallSocketClicked)
							.Text(LOCTEXT("BSPreset", "Ball and Socket"))
							.ToolTipText(LOCTEXT("BSPresetTooltip", "Setup joint as ball and socket. A Ball and Socket joint allows motion around an indefinite number of axes, which have one common center"))
						]
				]
			];
	}
}

void FPhysicsConstraintComponentDetails::AddLinearLimits(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance)
{
	IDetailCategoryBuilder& LinearLimitCat = DetailBuilder.EditCategory("Linear Limits");

	LinearXMotionProperty = ConstraintInstance->GetChildHandle("LinearXMotion");
	LinearYMotionProperty = ConstraintInstance->GetChildHandle("LinearYMotion");
	LinearZMotionProperty = ConstraintInstance->GetChildHandle("LinearZMotion");

	TArray<TSharedPtr<FString>> LinearLimitOptionNames;
	TArray<FText> LinearLimitOptionTooltips;
	TArray<bool> LinearLimitOptionRestrictItems;

	const int32 ExpectedLinearLimitOptionCount = 3;
	LinearXMotionProperty->GeneratePossibleValues(LinearLimitOptionNames, LinearLimitOptionTooltips, LinearLimitOptionRestrictItems);
	checkf(LinearLimitOptionNames.Num() == ExpectedLinearLimitOptionCount &&
		LinearLimitOptionTooltips.Num() == ExpectedLinearLimitOptionCount &&
		LinearLimitOptionRestrictItems.Num() == ExpectedLinearLimitOptionCount,
		TEXT("It seems the number of enum entries in ELinearConstraintMotion has changed. This must be handled here as well. "));


	uint8 LinearLimitEnum[LCM_MAX] = { LCM_Free, LCM_Limited, LCM_Locked };
	TSharedPtr<IPropertyHandle> LinearLimitProperties[] = { LinearXMotionProperty, LinearYMotionProperty, LinearZMotionProperty };


	for (int32 PropertyIdx = 0; PropertyIdx < 3; ++PropertyIdx)
	{
		TSharedPtr<IPropertyHandle> CurProperty = LinearLimitProperties[PropertyIdx];

		LinearLimitCat.AddProperty(CurProperty).CustomWidget()
			.NameContent()
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.Text(CurProperty->GetPropertyDisplayName())
				.ToolTipText(CurProperty->GetToolTipText())
			]
		.ValueContent()
			.MinDesiredWidth(125.0f * 3.0f)
			.MaxDesiredWidth(125.0f * 3.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					SNew(SCheckBox)
					.Style(FEditorStyle::Get(), "RadioButton")
					.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[0])
					.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[0])
					.ToolTipText(LinearLimitOptionTooltips[0])
					[
						SNew(STextBlock)
						.Text(FText::FromString(*LinearLimitOptionNames[0].Get()))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(5, 0, 0, 0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[1])
						.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[1])
						.ToolTipText(LinearLimitOptionTooltips[1])
						[
							SNew(STextBlock)
							.Text(FText::FromString(*LinearLimitOptionNames[1].Get()))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(5, 0, 0, 0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[2])
						.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[2])
						.ToolTipText(LinearLimitOptionTooltips[2])
						[
							SNew(STextBlock)
							.Text(FText::FromString(*LinearLimitOptionNames[2].Get()))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
			];
	}


	LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearLimitSize)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearLimit)));
	LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bLinearLimitSoft)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearLimit)));
	LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearLimitStiffness)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearLimit)));
	LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearLimitDamping)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearLimit)));
	LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bLinearBreakable)).ToSharedRef());
	LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearBreakThreshold)).ToSharedRef());
}

void FPhysicsConstraintComponentDetails::AddAngularLimits(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance)
{
	IDetailCategoryBuilder& AngularLimitCat = DetailBuilder.EditCategory("Angular Limits");

	AngularSwing1MotionProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularSwing1Motion));
	AngularSwing2MotionProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularSwing2Motion));
	AngularTwistMotionProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularTwistMotion));

	TArray<TSharedPtr<FString>> AngularLimitOptionNames;
	TArray<FText> AngularLimitOptionTooltips;
	TArray<bool> AngularLimitOptionRestrictItems;

	const int32 ExpectedAngularLimitOptionCount = 3;
	AngularSwing1MotionProperty->GeneratePossibleValues(AngularLimitOptionNames, AngularLimitOptionTooltips, AngularLimitOptionRestrictItems);
	checkf(AngularLimitOptionNames.Num() == ExpectedAngularLimitOptionCount &&
		AngularLimitOptionTooltips.Num() == ExpectedAngularLimitOptionCount &&
		AngularLimitOptionRestrictItems.Num() == ExpectedAngularLimitOptionCount,
		TEXT("It seems the number of enum entries in EAngularConstraintMotion has changed. This must be handled here as well. "));


	uint8 AngularLimitEnum[LCM_MAX] = { ACM_Free, LCM_Limited, LCM_Locked };
	TSharedPtr<IPropertyHandle> AngularLimitProperties[] = { AngularSwing1MotionProperty, AngularTwistMotionProperty, AngularSwing2MotionProperty };

	for (int32 PropertyIdx = 0; PropertyIdx < 3; ++PropertyIdx)
	{
		TSharedPtr<IPropertyHandle> CurProperty = AngularLimitProperties[PropertyIdx];

		AngularLimitCat.AddProperty(CurProperty).CustomWidget()
			.NameContent()
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.Text(CurProperty->GetPropertyDisplayName())
				.ToolTipText(CurProperty->GetToolTipText())
			]
		.ValueContent()
			.MinDesiredWidth(125.0f * 3.0f)
			.MaxDesiredWidth(125.0f * 3.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					SNew(SCheckBox)
					.Style(FEditorStyle::Get(), "RadioButton")
					.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[0])
					.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[0])
					.ToolTipText(AngularLimitOptionTooltips[0])
					[
						SNew(STextBlock)
						.Text(FText::FromString(*AngularLimitOptionNames[0].Get()))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(5, 0, 0, 0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[1])
						.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[1])
						.ToolTipText(AngularLimitOptionTooltips[1])
						[
							SNew(STextBlock)
							.Text(FText::FromString(*AngularLimitOptionNames[1].Get()))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(5, 0, 0, 0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[2])
						.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[2])
						.ToolTipText(AngularLimitOptionTooltips[2])
						[
							SNew(STextBlock)
							.Text(FText::FromString(*AngularLimitOptionNames[2].Get()))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
			];
	}

	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, Swing1LimitAngle)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwing1Limit)));
	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, TwistLimitAngle)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));
	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, Swing2LimitAngle)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwing2Limit)));


	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bSwingLimitSoft)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwingLimit)));
	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, SwingLimitStiffness)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwingLimit)));
	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, SwingLimitDamping)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwingLimit)));



	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bTwistLimitSoft)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));
	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, TwistLimitStiffness)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));
	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, TwistLimitDamping)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));

	if (bInPhat == false)
	{
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularRotationOffset)).ToSharedRef())
			.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularAnyLimit)));

	}
	else
	{
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularRotationOffset)).ToSharedRef())
			.Visibility(EVisibility::Collapsed);
	}

	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bAngularBreakable)).ToSharedRef());
	AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularBreakThreshold)).ToSharedRef());
}

TSharedRef<SWidget> FPhysicsConstraintComponentDetails::JoinPropertyWidgets(TSharedPtr<IPropertyHandle> TargetProperty, FName TargetChildName, TSharedPtr<IPropertyHandle> ConstraintInstance, FName CheckPropertyName, TSharedPtr<IPropertyHandle>& StoreCheckProperty, EPropertyType::Type DriveType)
{
	StoreCheckProperty = ConstraintInstance->GetChildHandle(CheckPropertyName);
	StoreCheckProperty->MarkHiddenByCustomization();
	TSharedRef<SWidget> TargetWidget = TargetProperty->GetChildHandle(TargetChildName)->CreatePropertyValueWidget();
	TargetWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, DriveType)));

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 5, 0)
		[
			StoreCheckProperty->CreatePropertyValueWidget()
		]
	+ SHorizontalBox::Slot()
		[
			TargetWidget
		];
}


void FPhysicsConstraintComponentDetails::AddLinearDrive(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance)
{
	IDetailCategoryBuilder& LinearMotorCat = DetailBuilder.EditCategory("LinearMotor");
	LinearPositionDriveProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bLinearPositionDrive));

	IDetailGroup& PositionGroup = LinearMotorCat.AddGroup("Linear Position Drive", FText::GetEmpty());
	PositionGroup.HeaderProperty(LinearPositionDriveProperty.ToSharedRef());

	TSharedRef<IPropertyHandle> LinearPositionTargetProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearPositionTarget)).ToSharedRef();
	

	TSharedRef<SWidget> LinearPositionXWidget = JoinPropertyWidgets(LinearPositionTargetProperty, FName("X"), ConstraintInstance, FName("bLinearXPositionDrive"), LinearXPositionDriveProperty, EPropertyType::LinearXPositionDrive);
	TSharedRef<SWidget> LinearPositionYWidget = JoinPropertyWidgets(LinearPositionTargetProperty, FName("Y"), ConstraintInstance, FName("bLinearYPositionDrive"), LinearYPositionDriveProperty, EPropertyType::LinearYPositionDrive);
	TSharedRef<SWidget> LinearPositionZWidget = JoinPropertyWidgets(LinearPositionTargetProperty, FName("Z"), ConstraintInstance, FName("bLinearZPositionDrive"), LinearZPositionDriveProperty, EPropertyType::LinearZPositionDrive);


	FDetailWidgetRow& LinearPositionTargetWidget = PositionGroup.AddPropertyRow(LinearPositionTargetProperty).CustomWidget(true);
	LinearPositionTargetWidget.NameContent()
	[
			LinearPositionTargetProperty->CreatePropertyNameWidget()
	];

	LinearPositionTargetWidget.ValueContent()
	.MinDesiredWidth(125 * 3 + 18 * 3)
	.MaxDesiredWidth(125 * 3 + 18 * 3)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			LinearPositionXWidget
		]
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		[
			LinearPositionYWidget
		]

		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		[
			LinearPositionZWidget
		]
	];

	PositionGroup.AddPropertyRow(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearDriveSpring)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearPositionDrive)));

	//velocity
	LinearVelocityDriveProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bLinearVelocityDrive));
	TSharedRef<IPropertyHandle> LinearVelocityTargetProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearVelocityTarget)).ToSharedRef();

	IDetailGroup& VelocityGroup = LinearMotorCat.AddGroup("Linear Velocity Drive", FText::GetEmpty());
	VelocityGroup.HeaderProperty(LinearVelocityDriveProperty.ToSharedRef());

	TSharedRef<SWidget> LinearVelocityXWidget = JoinPropertyWidgets(LinearVelocityTargetProperty, FName("X"), ConstraintInstance, FName("bLinearXVelocityDrive"), LinearXVelocityDriveProperty, EPropertyType::LinearXVelocityDrive);
	TSharedRef<SWidget> LinearVelocityYWidget = JoinPropertyWidgets(LinearVelocityTargetProperty, FName("Y"), ConstraintInstance, FName("bLinearYVelocityDrive"), LinearYVelocityDriveProperty, EPropertyType::LinearYVelocityDrive);
	TSharedRef<SWidget> LinearVelocityZWidget = JoinPropertyWidgets(LinearVelocityTargetProperty, FName("Z"), ConstraintInstance, FName("bLinearZVelocityDrive"), LinearZVelocityDriveProperty, EPropertyType::LinearZVelocityDrive);


	FDetailWidgetRow& LinearVelocityTargetWidget = VelocityGroup.AddPropertyRow(LinearVelocityTargetProperty).CustomWidget(true);
	LinearVelocityTargetWidget.NameContent()
	[
		LinearVelocityTargetProperty->CreatePropertyNameWidget()
	];

	LinearVelocityTargetWidget.ValueContent()
	.MinDesiredWidth(125 * 3 + 18 * 3)
	.MaxDesiredWidth(125 * 3 + 18 * 3)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			LinearVelocityXWidget
		]
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		[
			LinearVelocityYWidget
		]

		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		[
			LinearVelocityZWidget
		]
	];

	VelocityGroup.AddPropertyRow(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearDriveDamping)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearVelocityDrive)));

	LinearMotorCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, LinearDriveForceLimit)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearAnyDrive)));
}

void FPhysicsConstraintComponentDetails::AddAngularDrive(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance)
{
	AngularPositionDriveProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bAngularOrientationDrive));
	AngularVelocityDriveProperty = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bAngularVelocityDrive));

	IDetailCategoryBuilder& AngularMotorCat = DetailBuilder.EditCategory("AngularMotor");

	IDetailGroup& PositionGroup = AngularMotorCat.AddGroup("Angular Orientation Drive", FText::GetEmpty());
	PositionGroup.HeaderProperty(AngularPositionDriveProperty.ToSharedRef());

	PositionGroup.AddPropertyRow(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularOrientationTarget)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularPositionDrive)));

	PositionGroup.AddPropertyRow(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularDriveSpring)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularAnyDrive)));

	IDetailGroup& VelocityGroup = AngularMotorCat.AddGroup("Angular Velocity Drive", FText::GetEmpty());
	VelocityGroup.HeaderProperty(AngularVelocityDriveProperty.ToSharedRef());
	VelocityGroup.AddPropertyRow(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularVelocityTarget)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularVelocityDrive)));
	VelocityGroup.AddPropertyRow(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularDriveDamping)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularAnyDrive)));
	AngularMotorCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularDriveForceLimit)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularAnyDrive)));
	AngularMotorCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularDriveMode)).ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularAnyDrive)));
}

void FPhysicsConstraintComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	TSharedPtr<IPropertyHandle> ConstraintInstance;
	UPhysicsConstraintComponent* ConstraintComp = NULL;
	APhysicsConstraintActor* OwningConstraintActor = NULL;

	bInPhat = false;

	for (int32 i=0; i < Objects.Num(); ++i)
	{
		if (!Objects[i].IsValid()) { continue; }

		if (Objects[i]->IsA(UPhysicsConstraintTemplate::StaticClass()))
		{
			ConstraintInstance = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPhysicsConstraintTemplate, DefaultInstance));
			bInPhat = true;
			break;
		}
		else if (Objects[i]->IsA(UPhysicsConstraintComponent::StaticClass()))
		{
			ConstraintInstance = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPhysicsConstraintComponent, ConstraintInstance));
			ConstraintComp = (UPhysicsConstraintComponent*)Objects[i].Get();
			OwningConstraintActor = Cast<APhysicsConstraintActor>(ConstraintComp->GetOwner());
			break;
		}
	}

	AddConstraintPresets(DetailBuilder, OwningConstraintActor);
	AddLinearLimits(DetailBuilder, ConstraintInstance);
	AddAngularLimits(DetailBuilder, ConstraintInstance);
	AddLinearDrive(DetailBuilder, ConstraintInstance);
	AddAngularDrive(DetailBuilder, ConstraintInstance);
}

bool FPhysicsConstraintComponentDetails::IsPropertyEnabled( EPropertyType::Type Type ) const
{
	bool bIsVisible = false;
	switch (Type)
	{
		case EPropertyType::LinearXPositionDrive:	return GetBoolProperty(LinearXPositionDriveProperty);
		case EPropertyType::LinearYPositionDrive:	return GetBoolProperty(LinearYPositionDriveProperty);
		case EPropertyType::LinearZPositionDrive:	return GetBoolProperty(LinearZPositionDriveProperty);

		case EPropertyType::LinearXVelocityDrive:	return GetBoolProperty(LinearXVelocityDriveProperty);
		case EPropertyType::LinearYVelocityDrive:	return GetBoolProperty(LinearYVelocityDriveProperty);
		case EPropertyType::LinearZVelocityDrive:	return GetBoolProperty(LinearZVelocityDriveProperty);

		case EPropertyType::LinearPositionDrive:	return GetBoolProperty(LinearPositionDriveProperty);
		case EPropertyType::LinearVelocityDrive:	return GetBoolProperty(LinearVelocityDriveProperty);
		case EPropertyType::LinearAnyDrive:			return GetBoolProperty(LinearPositionDriveProperty) || GetBoolProperty(LinearVelocityDriveProperty);
		case EPropertyType::AngularPositionDrive:	return GetBoolProperty(AngularPositionDriveProperty);
		case EPropertyType::AngularVelocityDrive:	return GetBoolProperty(AngularVelocityDriveProperty);
		case EPropertyType::AngularAnyDrive:		return GetBoolProperty(AngularPositionDriveProperty) || GetBoolProperty(AngularVelocityDriveProperty);
		case EPropertyType::LinearLimit:			return IsLinearMotionLimited();
		case EPropertyType::AngularSwing1Limit:		return IsAngularPropertyLimited(AngularSwing1MotionProperty);
		case EPropertyType::AngularSwing2Limit:		return IsAngularPropertyLimited(AngularSwing2MotionProperty);
		case EPropertyType::AngularSwingLimit:		return IsAngularPropertyLimited(AngularSwing1MotionProperty) || IsAngularPropertyLimited(AngularSwing2MotionProperty);
		case EPropertyType::AngularTwistLimit:		return IsAngularPropertyLimited(AngularTwistMotionProperty);
		case EPropertyType::AngularAnyLimit:		return IsAngularPropertyLimited(AngularSwing1MotionProperty) || IsAngularPropertyLimited(AngularSwing2MotionProperty) || 
																 IsAngularPropertyLimited(AngularTwistMotionProperty);
	}	

	return bIsVisible;
}

bool FPhysicsConstraintComponentDetails::GetBoolProperty(TSharedPtr<IPropertyHandle> Prop) const
{
	bool bIsEnabled = false;

	if (Prop->GetValue(bIsEnabled))
	{
		return bIsEnabled;
	}
	return false;
}

bool FPhysicsConstraintComponentDetails::IsAngularPropertyLimited(TSharedPtr<IPropertyHandle> Prop) const
{
	uint8 Val;
	if (Prop->GetValue(Val))
	{
		return Val == ACM_Limited;
	}
	return false;
}

bool FPhysicsConstraintComponentDetails::IsLinearMotionLimited() const
{
	uint8 XMotion, YMotion, ZMotion;
	if (LinearXMotionProperty->GetValue(XMotion) && 
		LinearYMotionProperty->GetValue(YMotion) && 
		LinearZMotionProperty->GetValue(ZMotion))
	{
		return XMotion == LCM_Limited || YMotion == LCM_Limited || ZMotion == LCM_Limited;
	}
	return false;
}

FReply FPhysicsConstraintComponentDetails::OnHingeClicked()
{
	if (ConstraintComponent.IsValid())
	{
		UPhysicsConstraintComponent* Comp = Cast<UPhysicsConstraintComponent>(ConstraintComponent.Get());
		Comp->ConstraintInstance.ConfigureAsHinge();
		Comp->UpdateSpriteTexture();
	}
	return FReply::Handled();
}

FReply FPhysicsConstraintComponentDetails::OnPrismaticClicked()
{
	if (ConstraintComponent.IsValid())
	{
		UPhysicsConstraintComponent* Comp = Cast<UPhysicsConstraintComponent>(ConstraintComponent.Get());
		Comp->ConstraintInstance.ConfigureAsPrismatic();
		Comp->UpdateSpriteTexture();
	}
	return FReply::Handled();
}

FReply FPhysicsConstraintComponentDetails::OnBallSocketClicked()
{
	if (ConstraintComponent.IsValid())
	{
		UPhysicsConstraintComponent* Comp = Cast<UPhysicsConstraintComponent>(ConstraintComponent.Get());
		Comp->ConstraintInstance.ConfigureAsBS();
		Comp->UpdateSpriteTexture();
	}
	return FReply::Handled();
}

ECheckBoxState FPhysicsConstraintComponentDetails::IsLimitRadioChecked( TSharedPtr<IPropertyHandle> Property, uint8 Value ) const
{
	uint8 PropertyEnumValue = 0;
	if (Property.IsValid() && Property->GetValue(PropertyEnumValue))
	{
		return PropertyEnumValue == Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FPhysicsConstraintComponentDetails::OnLimitRadioChanged( ECheckBoxState CheckType, TSharedPtr<IPropertyHandle> Property, uint8 Value )
{
	if (Property.IsValid() && CheckType == ECheckBoxState::Checked)
	{
		Property->SetValue(Value);
	}
}

#undef LOCTEXT_NAMESPACE
