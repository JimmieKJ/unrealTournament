// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "ActorDetails.h"
#include "AssetSelection.h"
#include "Editor/Layers/Public/LayersModule.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetDebugUtilities.h"
#include "ClassIconFinder.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "BlueprintGraphDefinitions.h"
#include "Engine/Breakpoint.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "ScopedTransaction.h"
#include "CreateBlueprintFromActorDialog.h"
#include "ActorEditorUtils.h"
#include "BlueprintEditorUtils.h"
#include "ComponentTransformDetails.h"
#include "IPropertyUtilities.h"
#include "IDocumentation.h"
#include "Runtime/Engine/Classes/Engine/BrushShape.h"
#include "ActorDetailsDelegates.h"
#include "EditorCategoryUtils.h"
#include "Engine/DocumentationActor.h"
#include "SHyperlink.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "GameFramework/Volume.h"
#include "Engine/Selection.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Components/BillboardComponent.h"

#define LOCTEXT_NAMESPACE "ActorDetails"

FExtendActorDetails OnExtendActorDetails;

TSharedRef<IDetailCustomization> FActorDetails::MakeInstance()
{
	return MakeShareable( new FActorDetails );
}

FActorDetails::~FActorDetails()
{
}

void FActorDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// Get the list of hidden categories
	TArray<FString> HideCategories;
	FEditorCategoryUtils::GetClassHideCategories(DetailLayout.GetDetailsView().GetBaseClass(), HideCategories); 

	// These details only apply when adding an instance of the actor in a level
	if( !DetailLayout.GetDetailsView().HasClassDefaultObject() && DetailLayout.GetDetailsView().GetSelectedActorInfo().NumSelected > 0 )
	{
		// Build up a list of unique blueprints in the selection set (recording the first actor in the set for each one)
		TMap<UBlueprint*, UObject*> UniqueBlueprints;

		// Per level Actor Counts
		TMap<ULevel*, int32> ActorsPerLevelCount;

		bool bHasBillboardComponent = false;
		const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetDetailsView().GetSelectedObjects();
		for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
		{
			AActor* Actor = Cast<AActor>( SelectedObjects[ObjectIndex].Get() );

			if (Actor != NULL)
			{
				// Store the selected actors for use later. Its fine to do this when CustomizeDetails is called because if the selected actors changes, CustomizeDetails will be called again on a new instance
				// and our current resource would be destroyed.
				SelectedActors.Add( Actor );

				// Record the level that contains this actor and increment it's actor count
				ULevel* Level = Actor->GetTypedOuter<ULevel>();
				if (Level != NULL)
				{
					int32& ActorCountForThisLevel = ActorsPerLevelCount.FindOrAdd(Level);
					++ActorCountForThisLevel;
				}

				// Add to the unique blueprint map if the actor is generated from a blueprint
				if (UBlueprint* Blueprint = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy))
				{
					if (!UniqueBlueprints.Find(Blueprint))
					{
						UniqueBlueprints.Add(Blueprint, Actor);
					}
				}

				if (!bHasBillboardComponent)
				{
					bHasBillboardComponent = Actor->FindComponentByClass<UBillboardComponent>() != NULL;
				}
			}
		}

		if (!bHasBillboardComponent)
		{
			// Actor billboard scale is not relevant if the actor doesn't have a billboard component
			DetailLayout.HideProperty( GET_MEMBER_NAME_CHECKED(AActor, SpriteScale) );
		}

		AddExperimentalWarningCategory(DetailLayout);

		if (!HideCategories.Contains(TEXT("Transform")))
		{
			AddTransformCategory(DetailLayout);
		}
		
		if (!HideCategories.Contains(TEXT("Actor")))
		{
			AddActorCategory(DetailLayout, ActorsPerLevelCount);
		}

		// Add Blueprint category, if not being hidden
		if (!HideCategories.Contains(TEXT("Blueprint")))
		{
			AddBlutilityCategory(DetailLayout, UniqueBlueprints);
		}

		OnExtendActorDetails.Broadcast(DetailLayout, FGetSelectedActors::CreateSP(this, &FActorDetails::GetSelectedActors));
	}

	TSharedPtr<IPropertyHandle> PrimaryTickProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(AActor, PrimaryActorTick));

	// Defaults only show tick properties
	if (DetailLayout.GetDetailsView().HasClassDefaultObject() && !HideCategories.Contains(TEXT("Tick")))
	{
		// Note: the category is renamed to differentiate between 
		IDetailCategoryBuilder& TickCategory = DetailLayout.EditCategory("Tick", LOCTEXT("TickCategoryName", "Actor Tick") );

		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bStartWithTickEnabled)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickInterval)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bTickEvenWhenPaused)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bAllowTickOnDedicatedServer)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickGroup)), EPropertyLocation::Advanced);
	}

	PrimaryTickProperty->MarkHiddenByCustomization();
}

void FActorDetails::OnConvertActor(UClass* ChosenClass)
{
	if (ChosenClass)
	{
		// Check each selected actor's pointer.
		TArray<AActor*> SelectedActorsRaw;
		for (int32 i=0; i<SelectedActors.Num(); i++)
		{
			if (SelectedActors[i].IsValid())
			{
				SelectedActorsRaw.Add(SelectedActors[i].Get());
			}
		}

		// If there are valid pointers, convert the actors.
		if (SelectedActorsRaw.Num())
		{
			// Dismiss the menu BEFORE converting actors as it can refresh the details panel and if the menu is still open
			// it will be parented to an invalid actor details widget
			FSlateApplication::Get().DismissAllMenus();

			GEditor->ConvertActors(SelectedActorsRaw, ChosenClass, TSet<FString>(), true);
		}
	}
}

class FConvertToClassFilter : public IClassViewerFilter
{
public:
	/** All classes in this set will be allowed. */
	TSet< const UClass* > AllowedClasses;

	/** All classes in this set will be disallowed. */
	TSet< const UClass* > DisallowedClasses;

	/** Allowed ChildOf relationship. */
	TSet< const UClass* > AllowedChildOfRelationship;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		EFilterReturn::Type eState = InFilterFuncs->IfInClassesSet(AllowedClasses, InClass);
		if(eState == EFilterReturn::NoItems)
		{
			eState = InFilterFuncs->IfInChildOfClassesSet(AllowedChildOfRelationship, InClass);
		}

		// As long as it has not failed to be on an allowed list, check if it is on a disallowed list.
		if(eState == EFilterReturn::Passed)
		{
			eState = InFilterFuncs->IfInClassesSet(DisallowedClasses, InClass);

			// If it passes, it's on the disallowed list, so we do not want it.
			if(eState == EFilterReturn::Passed)
			{
				return false;
			}
			else
			{
				return true;
			}
		}

		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		EFilterReturn::Type eState = InFilterFuncs->IfInClassesSet(AllowedClasses, InUnloadedClassData);
		if(eState == EFilterReturn::NoItems)
		{
			eState = InFilterFuncs->IfInChildOfClassesSet(AllowedChildOfRelationship, InUnloadedClassData);
		}

		// As long as it has not failed to be on an allowed list, check if it is on a disallowed list.
		if(eState == EFilterReturn::Passed)
		{
			eState = InFilterFuncs->IfInClassesSet(DisallowedClasses, InUnloadedClassData);

			// If it passes, it's on the disallowed list, so we do not want it.
			if(eState == EFilterReturn::Passed)
			{
				return false;
			}
			else
			{
				return true;
			}
		}

		return false;
	}
};

UClass* FActorDetails::GetConversionRoot( UClass* InCurrentClass ) const
{
	UClass* ParentClass = InCurrentClass;

	while(ParentClass)
	{
		if( ParentClass->GetBoolMetaData(FName(TEXT("IsConversionRoot"))) )
		{
			break;
		}
		ParentClass = ParentClass->GetSuperClass();
	}

	return ParentClass;
}

void FActorDetails::CreateClassPickerConvertActorFilter(const TWeakObjectPtr<AActor> ConvertActor, class FClassViewerInitializationOptions* ClassPickerOptions)
{
	// Shouldn't ever be overwriting an already established filter
	check( ConvertActor.IsValid() )
	check( ClassPickerOptions != NULL && !ClassPickerOptions->ClassFilter.IsValid() )
	TSharedPtr<FConvertToClassFilter> Filter = MakeShareable(new FConvertToClassFilter);
	ClassPickerOptions->ClassFilter = Filter;

	UClass* ConvertClass = ConvertActor->GetClass();
	UClass* RootConversionClass = GetConversionRoot(ConvertClass);

	if(RootConversionClass)
	{
		Filter->AllowedChildOfRelationship.Add(RootConversionClass);
	}

	// Never convert to the same class
	Filter->DisallowedClasses.Add(ConvertClass);

	if( ConvertActor->IsA<ABrush>() )
	{
		// Volumes cannot be converted to brushes or brush shapes or the abstract type
		Filter->DisallowedClasses.Add(ABrush::StaticClass());
		Filter->DisallowedClasses.Add(ABrushShape::StaticClass());
		Filter->DisallowedClasses.Add(AVolume::StaticClass());
	}
}


TSharedRef<SWidget> FActorDetails::OnGetConvertContent()
{
	// Build a class picker widget

	// Fill in options
	FClassViewerInitializationOptions Options;

	Options.bShowUnloadedBlueprints = true;
	Options.bIsActorsOnly = true;
	Options.bIsPlaceableOnly = true;

	// All selected actors are of the same class, so just need to use one to generate the filter
	if ( SelectedActors.Num() > 0 )
	{
		CreateClassPickerConvertActorFilter(SelectedActors.Top(), &Options);
	}

	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::ListView;

	TSharedRef<SWidget> ClassPicker = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &FActorDetails::OnConvertActor));

	return
		SNew(SBox)
			.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(500)
			[
				ClassPicker
			]			
		];
}

EVisibility FActorDetails::GetConvertMenuVisibility() const
{
	return GLevelEditorModeTools().EnsureNotInMode(FBuiltinEditorModes::EM_InterpEdit) ?
		EVisibility::Visible :
		EVisibility::Collapsed;
}

TSharedRef<SWidget> FActorDetails::MakeConvertMenu( const FSelectedActorInfo& SelectedActorInfo )
{
	UClass* RootConversionClass = GetConversionRoot(SelectedActorInfo.SelectionClass);
	return
		SNew(SComboButton)
		.ContentPadding(2)
		.IsEnabled(RootConversionClass != NULL)
		.Visibility(this, &FActorDetails::GetConvertMenuVisibility)
		.OnGetMenuContent(this, &FActorDetails::OnGetConvertContent)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConvertButton", "Select a Type"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

}

void FActorDetails::OnNarrowSelectionSetToSpecificLevel( TWeakObjectPtr<ULevel> LevelToNarrowInto )
{
	if (ULevel* RequiredLevel = LevelToNarrowInto.Get())
	{
		// Remove any selected objects that aren't in the specified level
		TArray<AActor*> ActorsToDeselect;
		for ( TArray< TWeakObjectPtr<AActor> >::TConstIterator Iter(SelectedActors); Iter; ++Iter)
		{ 
			if( (*Iter).IsValid() )
			{
				AActor* Actor = (*Iter).Get();
				if (!Actor->IsIn(RequiredLevel))
				{
					ActorsToDeselect.Add(Actor);
				}
			}
		}

		for (TArray<AActor*>::TIterator DeselectIt(ActorsToDeselect); DeselectIt; ++DeselectIt)
		{
			AActor* Actor = *DeselectIt;
			GEditor->SelectActor(Actor, /*bSelected=*/ false, /*bNotify=*/ false);
		}

		// Tell the editor selection status was changed.
		GEditor->NoteSelectionChange();
	}
}

bool FActorDetails::IsActorValidForLevelScript() const
{
	AActor* Actor = GEditor->GetSelectedActors()->GetTop<AActor>();
	return FKismetEditorUtilities::IsActorValidForLevelScript(Actor);
}

FReply FActorDetails::FindSelectedActorsInLevelScript()
{
	GUnrealEd->FindSelectedActorsInLevelScript();
	return FReply::Handled();
};

bool FActorDetails::AreAnySelectedActorsInLevelScript() const
{
	return GUnrealEd->AreAnySelectedActorsInLevelScript();
};

/** Util to create a menu for events we can add for the selected actor */
TSharedRef<SWidget> FActorDetails::MakeEventOptionsWidgetFromSelection()
{
	FMenuBuilder EventMenuBuilder( true, NULL );

	AActor* Actor = SelectedActors[0].Get();
	FKismetEditorUtilities::AddLevelScriptEventOptionsForActor(EventMenuBuilder, SelectedActors[0], true, true, false);
	return EventMenuBuilder.MakeWidget();
}


void FActorDetails::AddLayersCategory( IDetailLayoutBuilder& DetailBuilder )
{
	if( !FModuleManager::Get().IsModuleLoaded( TEXT("Layers") ) )
	{
		return;
	}

	FLayersModule& LayersModule = FModuleManager::LoadModuleChecked< FLayersModule >( TEXT("Layers") );

	const FText LayerCategory = LOCTEXT("LayersCategory", "Layers");

	DetailBuilder.EditCategory( "Layers", LayerCategory, ECategoryPriority::Uncommon )
	.AddCustomRow( FText::GetEmpty() )
	[
		LayersModule.CreateLayerCloud( SelectedActors )
	];
}

void FActorDetails::AddTransformCategory( IDetailLayoutBuilder& DetailBuilder )
{
	const FSelectedActorInfo& SelectedActorInfo = DetailBuilder.GetDetailsView().GetSelectedActorInfo();

	bool bAreBrushesSelected = SelectedActorInfo.bHaveBrush;
	bool bIsOnlyWorldPropsSelected =  SelectedActors.Num() == 1 && SelectedActors[0].IsValid() && SelectedActors[0]->IsA<AWorldSettings>();
	bool bLacksRootComponent = SelectedActors[0].IsValid() && (SelectedActors[0]->GetRootComponent()==NULL);

	// Don't show the Transform details if the only actor selected is world properties, or if they have no RootComponent
	if ( bIsOnlyWorldPropsSelected || bLacksRootComponent )
	{
		return;
	}
	
	TSharedRef<FComponentTransformDetails> TransformDetails = MakeShareable( new FComponentTransformDetails( DetailBuilder.GetDetailsView().GetSelectedObjects(), SelectedActorInfo, DetailBuilder ) );

	IDetailCategoryBuilder& TransformCategory = DetailBuilder.EditCategory( "TransformCommon", LOCTEXT("TransformCommonCategory", "Transform"), ECategoryPriority::Transform );

	TransformCategory.AddCustomBuilder( TransformDetails );
}

void FActorDetails::AddExperimentalWarningCategory( IDetailLayoutBuilder& DetailBuilder )
{
	const FSelectedActorInfo& SelectedActorInfo = DetailBuilder.GetDetailsView().GetSelectedActorInfo();

	if (SelectedActorInfo.bHaveExperimentalClass || SelectedActorInfo.bHaveEarlyAccessClass)
	{
		const bool bExperimental = SelectedActorInfo.bHaveExperimentalClass;

		const FName CategoryName(TEXT("Warning"));
		const FText CategoryDisplayName = LOCTEXT("WarningCategoryDisplayName", "Warning");
		const FText WarningText = bExperimental ? LOCTEXT("ExperimentalClassWarning", "Uses experimental class") : LOCTEXT("EarlyAccessClassWarning", "Uses early access class");
		const FText SearchString = WarningText;
		const FText Tooltip = bExperimental ? LOCTEXT("ExperimentalClassTooltip", "Here be dragons!  Uses one or more unsupported 'experimental' classes") : LOCTEXT("EarlyAccessClassTooltip", "Uses one or more 'early access' classes");
		const FString ExcerptName = bExperimental ? TEXT("ActorUsesExperimentalClass") : TEXT("ActorUsesEarlyAccessClass");
		const FSlateBrush* WarningIcon = FEditorStyle::GetBrush(bExperimental ? "PropertyEditor.ExperimentalClass" : "PropertyEditor.EarlyAccessClass");

		IDetailCategoryBuilder& WarningCategory = DetailBuilder.EditCategory(CategoryName, CategoryDisplayName, ECategoryPriority::Transform);

		FDetailWidgetRow& WarningRow = WarningCategory.AddCustomRow(SearchString)
			.WholeRowContent()
			[
				SNew(SHorizontalBox)
				.ToolTip(IDocumentation::Get()->CreateToolTip(Tooltip, nullptr, TEXT("Shared/LevelEditor"), ExcerptName))
				.Visibility(EVisibility::Visible)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SImage)
					.Image(WarningIcon)
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(WarningText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
	}
}

void FActorDetails::AddActorCategory( IDetailLayoutBuilder& DetailBuilder, const TMap<ULevel*, int32>& ActorsPerLevelCount )
{		
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );

	const FLevelEditorCommands& Commands = LevelEditor.GetLevelEditorCommands();
	TSharedRef<const FUICommandList> CommandBindings = LevelEditor.GetGlobalLevelEditorActions();

	const FSelectedActorInfo& SelectedActorInfo = DetailBuilder.GetDetailsView().GetSelectedActorInfo();
	TSharedPtr<SVerticalBox> LevelBox;

	IDetailCategoryBuilder& ActorCategory = DetailBuilder.EditCategory("Actor", FText::GetEmpty(), ECategoryPriority::Uncommon );


#if 1
	// Create the info buttons per level
	for ( auto LevelIt( ActorsPerLevelCount.CreateConstIterator() ); LevelIt; ++LevelIt)
	{
		ULevel* Level = LevelIt.Key();
		int32 SelectedActorCountInLevel = LevelIt.Value();
		
		// Get a description of the level
		FText LevelDescription = FText::FromString( FPackageName::GetShortName( Level->GetOutermost()->GetFName() ) );
		if (Level == Level->OwningWorld->PersistentLevel)
		{
			LevelDescription = NSLOCTEXT("UnrealEd", "PersistentLevel", "Persistent Level");
		}

		// Create a description and tooltip for the actor count/selection hyperlink
		const FText ActorCountDescription = FText::Format( LOCTEXT("SelectedActorsInOneLevel", "{0} selected in"), FText::AsNumber( SelectedActorCountInLevel ) );

		const FText Tooltip = FText::Format( LOCTEXT("SelectedActorsHyperlinkTooltip", "Narrow the selection set to just the actors in {0}"), LevelDescription);

		// Create the row for this level
		TWeakObjectPtr<ULevel> WeakLevelPtr = Level;

		ActorCategory.AddCustomRow( LOCTEXT("SelectionFilter", "Selected") )
		.NameContent()
		[
			SNew(SHyperlink)
				.Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
				.OnNavigate( this, &FActorDetails::OnNarrowSelectionSetToSpecificLevel, WeakLevelPtr )
				.Text(ActorCountDescription)
				.TextStyle(FEditorStyle::Get(), "DetailsView.HyperlinkStyle")
				.ToolTipText(Tooltip)
		]
		.ValueContent()
		[
			SNew(STextBlock)
				.Text(LevelDescription)
				.Font( IDetailLayoutBuilder::GetDetailFont() )
		];

	}
#endif

	// WorldSettings should never convert to another class type
	if( SelectedActorInfo.SelectionClass != AWorldSettings::StaticClass() && SelectedActorInfo.HasConvertableAsset() )
	{
		ActorCategory.AddCustomRow( LOCTEXT("ConvertMenu", "Convert") )
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConvertButton", "Convert Actor"))
			.ToolTipText(LOCTEXT("ConvertButton_ToolTip", "Convert actors to different types"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			MakeConvertMenu( SelectedActorInfo )
		];
	}
}


void FActorDetails::AddBlutilityCategory( IDetailLayoutBuilder& DetailBuilder, const TMap<UBlueprint*, UObject*>& UniqueBlueprints )
{
	// Create the Blutilities Category
	IDetailCategoryBuilder& BlutilitiesCategory = DetailBuilder.EditCategory("Blutilities", NSLOCTEXT("Blutilities", "BlutilityTitle", "Blutilities"), ECategoryPriority::Uncommon );

	// Only show the bluetilities section if a single actor is selected
	if ( SelectedActors.Num() > 0 && DoesActorHaveBlutiltyFunctions() )
	{
		// Reset function Selection
		ActiveBlutilityFunction.Reset();

		// Grab actor label for later use
		TWeakObjectPtr<AActor> ActorPtr = SelectedActors[0];
		FText ActorLabel = NSLOCTEXT( "UnrealEd", "None", "None" );

		if ( ActorPtr.IsValid() )
		{
			ActorLabel = FText::FromString( ActorPtr.Get()->GetActorLabel() );
		}

		FFormatNamedArguments Args;
		Args.Add( TEXT( "ActorLabel" ), ActorLabel );
		const FText ButtonLabel = FText::Format( NSLOCTEXT( "Blutilities", "CallInEditor_ButtonLabel", "Run" ), Args );
		const FText ButtonToolTip = FText::Format( NSLOCTEXT( "Blutilities", "CallInEditor_ButtonTooltip", "Run Selected Blutility Function on {ActorLabel}" ), Args );
		// Build Content
		BlutilitiesCategory.AddCustomRow( NSLOCTEXT( "Blutilities", "CallInEditorHeader", "Blutility Functions"))
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 0, 0, 2, 0 )
			[
				SNew( SBox )
				.WidthOverride( 200 )
				[
					SNew( SComboButton )
					.ContentPadding( 2 )
					.HAlign(HAlign_Fill)
					.OnGetMenuContent( this, &FActorDetails::BuildBlutiltyFunctionContent)
					.ButtonContent()
					[
						SNew( STextBlock )
						.Text( this, &FActorDetails::GetBlutilityComboButtonLabel ) 
						.ToolTipText( this, &FActorDetails::GetBlutilityComboButtonLabel )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					]
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride( 50 )
				[
					SNew( SButton )
					.ToolTipText( ButtonToolTip )
					.OnClicked( this, &FActorDetails::CallBlutilityFunction )
					.IsEnabled( this, &FActorDetails::CanCallBlutilityFunction )
					.HAlign( HAlign_Center )
					.ContentPadding( 2 )
					[
						SNew( STextBlock )
						.Text( ButtonLabel )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					]
				]
			]
		];
		// Iterate over actor properties, adding property rows that are editable and may feed into the blutility function.
		TArray<UObject*> ActorArray;
		ActorArray.Add( ActorPtr.Get() );
		for( TFieldIterator<UProperty> PropertyIt(ActorPtr->GetClass()); PropertyIt; ++PropertyIt )
		{
			const bool bBlueprintProperty = PropertyIt->GetOuter()->GetClass() == UBlueprintGeneratedClass::StaticClass();
			if( bBlueprintProperty && !PropertyIt->HasAllPropertyFlags( CPF_DisableEditOnInstance ))
			{
				BlutilitiesCategory.AddExternalProperty( ActorArray, PropertyIt->GetFName(), EPropertyLocation::Advanced );
			}
		}
	}
}

const TArray< TWeakObjectPtr<AActor> >& FActorDetails::GetSelectedActors() const
{
	return SelectedActors;
}

bool FActorDetails::DoesActorHaveBlutiltyFunctions() const
{
	bool bFunctionsFound = false;

	if( SelectedActors.Num() > 0 )
	{
		TWeakObjectPtr<AActor> WeakActorPtr = SelectedActors[ 0 ];
		UClass* ActorClass = WeakActorPtr.IsValid() ? WeakActorPtr->GetClass() : nullptr;

		if( ActorClass )
		{
			for( TFieldIterator<UFunction> FunctionIter(ActorClass, EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter )
			{
				if( FunctionIter->GetBoolMetaData( FBlueprintMetadata::MD_CallInEditor ))
				{
					bFunctionsFound = true;
					break;
				}
			}
		}
	}

	return bFunctionsFound;
}

FText FActorDetails::GetBlutilityComboButtonLabel() const
{
	return ActiveBlutilityFunction.IsValid() ?  FText::FromString( ActiveBlutilityFunction->GetName() ) : 
												NSLOCTEXT( "Blutilities", "CallInEditor_ComboLabel", "Select Blutility" );
}

TSharedRef<SWidget> FActorDetails::BuildBlutiltyFunctionContent() const
{
	if( SelectedActors.Num() > 0 )
	{
		TWeakObjectPtr<AActor> WeakActorPtr = SelectedActors[ 0 ];
		UClass* ActorClass = WeakActorPtr.IsValid() ? WeakActorPtr->GetClass() : nullptr;

		if( ActorClass )
		{
			FMenuBuilder MenuBuilder( true, NULL );
			MenuBuilder.BeginSection("BlutilityFunctions", NSLOCTEXT( "Blutilities", "CallInEditorHeader", "Blutility Functions") );
			const FSlateIcon BlutilityIcon(FEditorStyle::GetStyleSetName(), "GraphEditor.CallInEditorEvent_16x");

			for (TFieldIterator<UFunction> FunctionIter(ActorClass, EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter)
			{
				if( FunctionIter->GetBoolMetaData( FBlueprintMetadata::MD_CallInEditor ))
				{
					MenuBuilder.AddMenuEntry(	FText::FromString( *FunctionIter->GetName() ), 
												FunctionIter->GetToolTipText(), 
												BlutilityIcon,
												FUIAction( FExecuteAction::CreateSP( this, &FActorDetails::SetActiveBlutilityFunction, TWeakObjectPtr<UFunction>(*FunctionIter))));
				}
			}
			MenuBuilder.EndSection();

			return MenuBuilder.MakeWidget();
		}
	}

	return SNullWidget::NullWidget;
}

FReply FActorDetails::CallBlutilityFunction()
{
	TWeakObjectPtr<AActor> ActorWeakPtr = SelectedActors.Num() ? SelectedActors[ 0 ] : nullptr;
	
	if( ActorWeakPtr.IsValid() && ActiveBlutilityFunction.IsValid() )
	{
		AActor* Actor = ActorWeakPtr.Get();
		UFunction* ActiveFunction = ActiveBlutilityFunction.Get();
		Actor->ProcessEvent( ActiveFunction, ActiveFunction->Children );
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
