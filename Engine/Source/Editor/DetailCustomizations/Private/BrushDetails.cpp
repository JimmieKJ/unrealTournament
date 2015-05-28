// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "BrushDetails.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/LevelEditorActions.h"
#include "ActorEditorUtils.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "AssetSelection.h"
#include "ClassIconFinder.h"
#include "ScopedTransaction.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Volume.h"

#define LOCTEXT_NAMESPACE "BrushDetails"


TSharedRef<IDetailCustomization> FBrushDetails::MakeInstance()
{
	return MakeShareable( new FBrushDetails );
}

FBrushDetails::~FBrushDetails()
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBrushDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// Get level editor commands for our menus
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	TSharedRef<const FUICommandList> CommandBindings = LevelEditor.GetGlobalLevelEditorActions();
	const FLevelEditorCommands& Commands = LevelEditor.GetLevelEditorCommands();

	// See if we have a volume. If we do - we hide the BSP stuff (solidity, order)
	bool bHaveAVolume = false;
	TArray< TWeakObjectPtr<UObject> > SelectedObjects = DetailLayout.GetDetailsView().GetSelectedObjects();
	for(int32 ObjIdx=0; ObjIdx<SelectedObjects.Num(); ObjIdx++)
	{
		AVolume* Volume = Cast<AVolume>( SelectedObjects[ObjIdx].Get() );

		if (Volume != NULL)
		{
			bHaveAVolume = true;
		}
		else
		{
			// Since the brush is not a volume, it is valid for the SelectedBSPBrushes list.
			ABrush* Brush = Cast<ABrush>( SelectedObjects[ObjIdx].Get() );
			if(!FActorEditorUtils::IsABuilderBrush(Brush))
			{
				// Store the selected actors for use later. Its fine to do this when CustomizeDetails is called because if the selected actors changes, CustomizeDetails will be called again on a new instance
				// and our current resource would be destroyed.
				SelectedBSPBrushes.Add( Brush );
			}
		}
	}
	FMenuBuilder PolygonsMenuBuilder( true, CommandBindings );
	{
		PolygonsMenuBuilder.BeginSection("BrushDetailsPolygons");
		{
			PolygonsMenuBuilder.AddMenuEntry( Commands.MergePolys );
			PolygonsMenuBuilder.AddMenuEntry( Commands.SeparatePolys );
		}
		PolygonsMenuBuilder.EndSection();
	}

	FMenuBuilder SolidityMenuBuilder( true, CommandBindings );
	{
		SolidityMenuBuilder.AddMenuEntry( Commands.MakeSolid );
		SolidityMenuBuilder.AddMenuEntry( Commands.MakeSemiSolid );
		SolidityMenuBuilder.AddMenuEntry( Commands.MakeNonSolid );
	}

	FMenuBuilder OrderMenuBuilder( true, CommandBindings );
	{
		OrderMenuBuilder.AddMenuEntry( Commands.OrderFirst );
		OrderMenuBuilder.AddMenuEntry( Commands.OrderLast );
	}

	struct Local
	{
		static FReply ExecuteExecCommand(FString InCommand)
		{
			GUnrealEd->Exec( GWorld, *InCommand );
			return FReply::Handled();
		}

		static TSharedRef<SWidget> GenerateBuildMenuContent(TSharedRef<IPropertyHandle> BrushBuilderHandle, IDetailLayoutBuilder* InDetailLayout)
		{
			class FBrushFilter : public IClassViewerFilter
			{
			public:
				virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs )
				{
					return !InClass->HasAnyClassFlags(CLASS_NotPlaceable) && !InClass->HasAnyClassFlags(CLASS_Abstract) && InClass->IsChildOf(UBrushBuilder::StaticClass());
				}

				virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const class IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs)
				{
					return false;
				}
			};

			FClassViewerInitializationOptions Options;
			Options.ClassFilter = MakeShareable(new FBrushFilter);
			Options.Mode = EClassViewerMode::ClassPicker;
			Options.DisplayMode = EClassViewerDisplayMode::ListView;
			return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateStatic(&Local::OnClassPicked, BrushBuilderHandle, InDetailLayout));
		}

		static void OnClassPicked(UClass* InChosenClass, TSharedRef<IPropertyHandle> BrushBuilderHandle, IDetailLayoutBuilder* InDetailLayout)
		{
			FSlateApplication::Get().DismissAllMenus();

			TArray<UObject*> OuterObjects;
			BrushBuilderHandle->GetOuterObjects(OuterObjects);

			struct FNewBrushBuilder
			{
				UBrushBuilder* Builder;
				ABrush* Brush;
			};

			TArray<FNewBrushBuilder> NewBuilders;
			TArray<FString> NewObjectPaths;

			{
				const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "BrushSet", "Brush Set"));
				for (UObject* OuterObject : OuterObjects)
				{
					UBrushBuilder* NewUObject = NewObject<UBrushBuilder>(OuterObject, InChosenClass, NAME_None, RF_Transactional);

					FNewBrushBuilder NewBuilder;
					NewBuilder.Builder = NewUObject;
					NewBuilder.Brush = CastChecked<ABrush>(OuterObject);

					NewBuilders.Add(NewBuilder);
					NewObjectPaths.Add(NewUObject->GetPathName());
				}

				BrushBuilderHandle->SetPerObjectValues(NewObjectPaths);

				// make sure the brushes are rebuilt
				for (FNewBrushBuilder& NewObject : NewBuilders)
				{
					NewObject.Builder->Build(NewObject.Brush->GetWorld(), NewObject.Brush);
				}

				GEditor->RebuildAlteredBSP();
			}



			InDetailLayout->ForceRefreshDetails();
		}

		static FText GetBuilderText(TSharedRef<IPropertyHandle> BrushBuilderHandle)
		{
			UObject* Object = nullptr;
			BrushBuilderHandle->GetValue(Object);
			if(Object != nullptr)
			{
				UBrushBuilder* BrushBuilder = CastChecked<UBrushBuilder>(Object);
				const FText NameText = BrushBuilder->GetClass()->GetDisplayNameText();
				if(!NameText.IsEmpty())
				{
					return NameText;
				}
				else
				{
					return FText::FromString(FName::NameToDisplayString(BrushBuilder->GetClass()->GetName(), false));
				}
			}

			return LOCTEXT("None", "None");
		}
	};

	// Hide the brush builder if it is NULL
	TSharedRef<IPropertyHandle> BrushBuilderPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(ABrush, BrushBuilder));
	UObject* BrushBuilderObject = nullptr;
	BrushBuilderPropertyHandle->GetValue(BrushBuilderObject);
	if(BrushBuilderObject == nullptr)
	{
		DetailLayout.HideProperty("BrushBuilder");
	}
	else
	{
		BrushBuilderObject->SetFlags( RF_Transactional );
	}

	IDetailCategoryBuilder& BrushBuilderCategory = DetailLayout.EditCategory( "BrushSettings", FText::GetEmpty(), ECategoryPriority::Important );

	BrushBuilderCategory.AddProperty( GET_MEMBER_NAME_CHECKED(ABrush, BrushType) );
	BrushBuilderCategory.AddCustomRow( LOCTEXT("BrushShape", "Brush Shape") )
	.NameContent()
	[
		SNew( STextBlock )
		.Text( LOCTEXT("BrushShape", "Brush Shape"))
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	.MinDesiredWidth(113)
	.MaxDesiredWidth(113)
	[
		SNew(SComboButton)
		.ToolTipText(LOCTEXT("BspModeBuildTooltip", "Rebuild this brush from a parametric builder."))
		.OnGetMenuContent_Static(&Local::GenerateBuildMenuContent, BrushBuilderPropertyHandle, &DetailLayout)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Static(&Local::GetBuilderText, BrushBuilderPropertyHandle)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
	];

	BrushBuilderCategory.AddCustomRow( FText::GetEmpty(), true )
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.Padding(1.0f)
		[
			SNew(SComboButton)
			.ContentPadding(2)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("BrushDetails", "PolygonsMenu", "Polygons"))
				.ToolTipText(NSLOCTEXT("BrushDetails", "PolygonsMenu_ToolTip", "Polygon options"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.MenuContent()
			[
				PolygonsMenuBuilder.MakeWidget()
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.Padding(1.0f)
		[
			SNew(SComboButton)
			.ContentPadding(2)
			.Visibility(bHaveAVolume ? EVisibility::Collapsed : EVisibility::Visible)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("BrushDetails", "SolidityMenu", "Solidity"))
				.ToolTipText(NSLOCTEXT("BrushDetails", "SolidityMenu_ToolTip", "Solidity options"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.MenuContent()
			[
				SolidityMenuBuilder.MakeWidget()
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.Padding(1.0f)
		[
			SNew(SComboButton)
			.ContentPadding(2)
			.Visibility(bHaveAVolume ? EVisibility::Collapsed : EVisibility::Visible)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("BrushDetails", "OrderMenu", "Order"))
				.ToolTipText(NSLOCTEXT("BrushDetails", "OrderMenu_ToolTip", "Order options"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.MenuContent()
			[
				OrderMenuBuilder.MakeWidget()
			]
		]
	];

	TSharedPtr< SHorizontalBox > BrushHorizontalBox;

	BrushBuilderCategory.AddCustomRow( FText::GetEmpty(), true)
	[
		SAssignNew(BrushHorizontalBox, SHorizontalBox)
		+SHorizontalBox::Slot()
		[
			SNew( SButton )
			.ToolTipText( LOCTEXT("AlignBrushVerts_Tooltip", "Aligns each vertex of the brush to the grid.") )
			.OnClicked( FOnClicked::CreateStatic( &Local::ExecuteExecCommand, FString( TEXT("ACTOR ALIGN VERTS") ) ) )
			.HAlign( HAlign_Center )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("AlignBrushVerts", "Align Brush Vertices") )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		]
	];

	if(SelectedBSPBrushes.Num())
	{
		BrushHorizontalBox->AddSlot()
		[
			SNew( SButton )
			.ToolTipText( LOCTEXT("CreateStaticMeshActor_Tooltip", "Creates a static mesh from selected brushes and replaces the affected brushes in the scene with the new static mesh") )
			.OnClicked( this, &FBrushDetails::OnCreateStaticMesh )
			.HAlign( HAlign_Center )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("CreateStaticMeshActor", "Create Static Mesh") )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		];
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply FBrushDetails::OnCreateStaticMesh()
{
	// Raw list of brushes to be sent to be converted.
	TArray<AActor*> RawSelectedBrushes;
	
	TWeakObjectPtr<ABrush> Brush;
	for(int32 BrushIdx = 0; BrushIdx < SelectedBSPBrushes.Num(); ++BrushIdx )
	{
		Brush = SelectedBSPBrushes[BrushIdx];

		// Make sure the brush is still valid.
		if(Brush.IsValid())
		{
			RawSelectedBrushes.Add(Brush.Get());
		}
	}

	GEditor->ConvertActors(RawSelectedBrushes, AStaticMeshActor::StaticClass(), TSet<FString>(), true);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

