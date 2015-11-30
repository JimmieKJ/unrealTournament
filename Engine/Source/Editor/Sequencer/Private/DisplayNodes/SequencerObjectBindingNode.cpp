// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerObjectBindingNode.h"
#include "Sequencer.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "ObjectEditorUtils.h"
#include "AssetEditorManager.h"


#define LOCTEXT_NAMESPACE "FObjectBindingNode"


namespace SequencerNodeConstants
{
	extern const float CommonPadding;
}


void GetKeyablePropertyPaths(UClass* Class, UStruct* PropertySource, TArray<UProperty*>& PropertyPath, FSequencer& Sequencer, TArray<TArray<UProperty*>>& KeyablePropertyPaths)
{
	//@todo need to resolve this between UMG and the level editor sequencer
	const bool bRecurseAllProperties = Sequencer.IsLevelEditorSequencer();

	for (TFieldIterator<UProperty> PropertyIterator(PropertySource); PropertyIterator; ++PropertyIterator)
	{
		UProperty* Property = *PropertyIterator;

		if (Property && !Property->HasAnyPropertyFlags(CPF_Deprecated))
		{
			PropertyPath.Add(Property);

			bool bIsPropertyKeyable = Sequencer.CanKeyProperty(FCanKeyPropertyParams(Class, PropertyPath));
			if (bIsPropertyKeyable)
			{
				KeyablePropertyPaths.Add(PropertyPath);
			}

			if (!bIsPropertyKeyable || bRecurseAllProperties)
			{
				UStructProperty* StructProperty = Cast<UStructProperty>(Property);
				if (StructProperty != nullptr)
				{
					GetKeyablePropertyPaths(Class, StructProperty->Struct, PropertyPath, Sequencer, KeyablePropertyPaths);
				}
			}

			PropertyPath.RemoveAt(PropertyPath.Num() - 1);
		}
	}
}


struct PropertyMenuData
{
	FString MenuName;
	TArray<UProperty*> PropertyPath;
};


/* FSequencerDisplayNode interface
 *****************************************************************************/

void FSequencerObjectBindingNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	if (GetSequencer().IsLevelEditorSequencer())
	{
		UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);

		if (Spawnable)
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Spawnable->GetClass()->ClassGeneratedBy);

			if (Blueprint)
			{
				FFormatNamedArguments Args;
				MenuBuilder.AddMenuEntry(
					FText::Format( LOCTEXT("EditDefaults", "Edit Defaults"), Args),
					FText::Format( LOCTEXT("EditDefaultsTooltip", "Edit the defaults for this spawnable object"), Args ),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([=]{
						FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);
					}))
				);
			}
		}
		else
		{
			const UClass* ObjectClass = GetClassForObjectBinding();
			
			if (ObjectClass->IsChildOf(AActor::StaticClass()))
			{
				FFormatNamedArguments Args;

				MenuBuilder.AddSubMenu(
					FText::Format( LOCTEXT("Assign Actor ", "Assign Actor"), Args),
					FText::Format( LOCTEXT("AssignActorTooltip", "Assign an actor to this track"), Args ),
					FNewMenuDelegate::CreateRaw(&GetSequencer(), &FSequencer::AssignActor, ObjectBinding));
			}
		}

#if 0
		MenuBuilder.BeginSection("Organize", LOCTEXT("OrganizeContextMenuSectionName", "Organize"));
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("LabelsSubMenuText", "Labels"),
				LOCTEXT("LabelsSubMenuTip", "Add or remove labels on this track"),
				FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::HandleLabelsSubMenuCreate)
			);
		}
		MenuBuilder.EndSection();
#endif
	}

	FSequencerDisplayNode::BuildContextMenu(MenuBuilder);
}


bool FSequencerObjectBindingNode::CanRenameNode() const
{
	return true;
}


TSharedRef<SWidget> FSequencerObjectBindingNode::GenerateEditWidgetForOutliner()
{
	// Create a container edit box
	TSharedPtr<class SHorizontalBox> EditBox;
	SAssignNew(EditBox, SHorizontalBox);	

	// Add the property combo box
	EditBox.Get()->AddSlot()
	[
		SNew(SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Light")
			.OnGetMenuContent(this, &FSequencerObjectBindingNode::HandleAddTrackComboButtonGetMenuContent)
			.ContentPadding(FMargin(2, 0))
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
							.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.8"))
							.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, 0, 0, 0)
					[
						SNew(STextBlock)
							.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
							.Text(LOCTEXT("AddTrackButton", "Track"))
					]
			]
	];

	const UClass* ObjectClass = GetClassForObjectBinding();

	GetSequencer().BuildObjectBindingEditButtons(EditBox, ObjectBinding, ObjectClass);

	return EditBox.ToSharedRef();
}


FText FSequencerObjectBindingNode::GetDisplayName() const
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene != nullptr)
	{
		return MovieScene->GetObjectDisplayName(ObjectBinding);
	}

	return DefaultDisplayName;
}


float FSequencerObjectBindingNode::GetNodeHeight() const
{
	return SequencerLayoutConstants::ObjectNodeHeight;
}


FNodePadding FSequencerObjectBindingNode::GetNodePadding() const
{
	return FNodePadding(SequencerNodeConstants::CommonPadding * 2, 0.f);
}


ESequencerNode::Type FSequencerObjectBindingNode::GetType() const
{
	return ESequencerNode::Object;
}


void FSequencerObjectBindingNode::SetDisplayName(const FText& NewDisplayName)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene != nullptr)
	{
		MovieScene->SetObjectDisplayName(ObjectBinding, NewDisplayName);
	}
}


/* FSequencerObjectBindingNode implementation
 *****************************************************************************/

void FSequencerObjectBindingNode::AddPropertyMenuItems(FMenuBuilder& AddTrackMenuBuilder, TArray<TArray<UProperty*> > KeyableProperties, int32 PropertyNameIndexStart, int32 PropertyNameIndexEnd)
{
	TArray<PropertyMenuData> KeyablePropertyMenuData;

	for (auto KeyableProperty : KeyableProperties)
	{
		TArray<FString> PropertyNames;
		if (PropertyNameIndexEnd == -1)
		{
			PropertyNameIndexEnd = KeyableProperty.Num();
		}

		//@todo
		if (PropertyNameIndexStart >= KeyableProperty.Num())
		{
			continue;
		}

		for (int32 PropertyNameIndex = PropertyNameIndexStart; PropertyNameIndex < PropertyNameIndexEnd; ++PropertyNameIndex)
		{
			PropertyNames.Add(KeyableProperty[PropertyNameIndex]->GetDisplayNameText().ToString());
		}

		PropertyMenuData KeyableMenuData;
		{
			KeyableMenuData.PropertyPath = KeyableProperty;
			KeyableMenuData.MenuName = FString::Join( PropertyNames, TEXT( "." ) );
		}

		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});

	// Add menu items
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); ++MenuDataIndex)
	{
		FUIAction AddTrackMenuAction(FExecuteAction::CreateSP(this, &FSequencerObjectBindingNode::HandlePropertyMenuItemExecute, KeyablePropertyMenuData[MenuDataIndex].PropertyPath));
		AddTrackMenuBuilder.AddMenuEntry(FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName), FText(), FSlateIcon(), AddTrackMenuAction);
	}
}


const UClass* FSequencerObjectBindingNode::GetClassForObjectBinding()
{
	FSequencer& ParentSequencer = GetSequencer();

	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding);
	
	// should exist, but also shouldn't be both a spawnable and a possessable
	check((Spawnable != nullptr) ^ (Possessable != nullptr));
	check((nullptr == Spawnable) || (nullptr != Spawnable->GetClass())  );
	const UClass* ObjectClass = Spawnable ? Spawnable->GetClass()->GetSuperClass() : Possessable->GetPossessedObjectClass();

	return ObjectClass;
}


/* FSequencerObjectBindingNode callbacks
 *****************************************************************************/

TSharedRef<SWidget> FSequencerObjectBindingNode::HandleAddTrackComboButtonGetMenuContent()
{
	FSequencer& Sequencer = GetSequencer();

	//@todo need to resolve this between UMG and the level editor sequencer
	const bool bUseSubMenus = Sequencer.IsLevelEditorSequencer();

	UObject* BoundObject = Sequencer.GetFocusedMovieSceneSequenceInstance()->FindObject(ObjectBinding, Sequencer);

	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>( "Sequencer" );
	TSharedRef<FUICommandList> CommandList(new FUICommandList);
	FMenuBuilder AddTrackMenuBuilder(true, nullptr, SequencerModule.GetMenuExtensibilityManager()->GetAllExtenders(CommandList, TArrayBuilder<UObject*>().Add(BoundObject)));

	const UClass* ObjectClass = GetClassForObjectBinding();
	AddTrackMenuBuilder.BeginSection(NAME_None, LOCTEXT("TracksMenuHeader" , "Tracks"));
	GetSequencer().BuildObjectBindingTrackMenu(AddTrackMenuBuilder, ObjectBinding, ObjectClass);
	AddTrackMenuBuilder.EndSection();

	TArray<TArray<UProperty*>> KeyablePropertyPaths;

	if (BoundObject != nullptr)
	{
		TArray<UProperty*> PropertyPath;
		GetKeyablePropertyPaths(BoundObject->GetClass(), BoundObject->GetClass(), PropertyPath, Sequencer, KeyablePropertyPaths);
	}

	// [Aspect Ratio]
	// [PostProcess Settings] [Bloom1Tint] [X]
	// [PostProcess Settings] [Bloom1Tint] [Y]
	// [PostProcess Settings] [ColorGrading]
	// [Ortho View]

	// Create property menu data based on keyable property paths
	TArray<PropertyMenuData> KeyablePropertyMenuData;
	for (auto KeyablePropertyPath : KeyablePropertyPaths)
	{
		PropertyMenuData KeyableMenuData;
		KeyableMenuData.PropertyPath = KeyablePropertyPath;
		KeyableMenuData.MenuName = KeyablePropertyPath[0]->GetDisplayNameText().ToString();
		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});
	

	// Add menu items
	AddTrackMenuBuilder.BeginSection( SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, LOCTEXT("PropertiesMenuHeader" , "Properties"));
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); )
	{
		TArray<TArray<UProperty*> > KeyableSubMenuPropertyPaths;

		KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);

		// If this menu data only has one property name, add the menu item
		if (KeyablePropertyMenuData[MenuDataIndex].PropertyPath.Num() == 1 || !bUseSubMenus)
		{
			AddPropertyMenuItems(AddTrackMenuBuilder, KeyableSubMenuPropertyPaths);
			++MenuDataIndex;
		}
		// Otherwise, look to the next menu data to gather up new data
		else
		{
			for (; MenuDataIndex < KeyablePropertyMenuData.Num()-1; )
			{
				if (KeyablePropertyMenuData[MenuDataIndex].MenuName == KeyablePropertyMenuData[MenuDataIndex+1].MenuName)
				{	
					++MenuDataIndex;
					KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);
				}
				else
				{
					break;
				}
			}

			AddTrackMenuBuilder.AddSubMenu(
				FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName),
				FText::GetEmpty(), 
				FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::HandleAddTrackSubMenuNew, KeyableSubMenuPropertyPaths));

			++MenuDataIndex;
		}
	}
	AddTrackMenuBuilder.EndSection();

	return AddTrackMenuBuilder.MakeWidget();
}


void FSequencerObjectBindingNode::HandleAddTrackSubMenuNew(FMenuBuilder& AddTrackMenuBuilder, TArray<TArray<UProperty*> > KeyablePropertyPaths)
{
	// [PostProcessSettings] [Bloom1Tint] [X]
	// [PostProcessSettings] [Bloom1Tint] [Y]
	// [PostProcessSettings] [ColorGrading]

	// Create property menu data based on keyable property paths
	TSet<UProperty*> PropertiesTraversed;
	TArray<PropertyMenuData> KeyablePropertyMenuData;
	for (auto KeyablePropertyPath : KeyablePropertyPaths)
	{		
		PropertyMenuData KeyableMenuData;
		KeyableMenuData.PropertyPath = KeyablePropertyPath;

		// If the path is greater than 1, keep track of the actual properties (not channels) and only add these properties once since we can't do single channel keying of a property yet.
		if (KeyablePropertyPath.Num() > 1) //@todo
		{
			if (PropertiesTraversed.Find(KeyablePropertyPath[1]) != nullptr)
			{
				continue;
			}

			KeyableMenuData.MenuName = FObjectEditorUtils::GetCategoryFName(KeyablePropertyPath[1]).ToString();
			PropertiesTraversed.Add(KeyablePropertyPath[1]);
		}
		else
		{
			// No sub menus items, so skip
			continue; 
		}
		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});

	// Add menu items
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); )
	{
		TArray<TArray<UProperty*> > KeyableSubMenuPropertyPaths;
		KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);

		for (; MenuDataIndex < KeyablePropertyMenuData.Num()-1; )
		{
			if (KeyablePropertyMenuData[MenuDataIndex].MenuName == KeyablePropertyMenuData[MenuDataIndex+1].MenuName)
			{
				++MenuDataIndex;
				KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);
			}
			else
			{
				break;
			}
		}

		const int32 PropertyNameIndexStart = 1; // Strip off the struct property name
		const int32 PropertyNameIndexEnd = 2; // Stop at the property name, don't descend into the channels

		AddTrackMenuBuilder.AddSubMenu(
			FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName),
			FText::GetEmpty(), 
			FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::AddPropertyMenuItems, KeyableSubMenuPropertyPaths, PropertyNameIndexStart, PropertyNameIndexEnd));

		++MenuDataIndex;
	}
}


void FSequencerObjectBindingNode::HandleLabelsSubMenuCreate(FMenuBuilder& MenuBuilder)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
	MenuBuilder.AddWidget(SNew(SSequencerLabelEditor, MovieScene, ObjectBinding), FText::GetEmpty(), true);
}


void FSequencerObjectBindingNode::HandlePropertyMenuItemExecute(TArray<UProperty*> PropertyPath)
{
	FSequencer& Sequencer = GetSequencer();
	UObject* BoundObject = Sequencer.GetFocusedMovieSceneSequenceInstance()->FindObject(ObjectBinding, Sequencer);

	TArray<UObject*> KeyableBoundObjects;
	if (BoundObject != nullptr)
	{
		if (Sequencer.CanKeyProperty(FCanKeyPropertyParams(BoundObject->GetClass(), PropertyPath)))
		{
			KeyableBoundObjects.Add(BoundObject);
		}
	}

	FKeyPropertyParams KeyPropertyParams(KeyableBoundObjects, PropertyPath);
	{
		KeyPropertyParams.KeyParams.bCreateTrackIfMissing = true;
		KeyPropertyParams.KeyParams.bCreateHandleIfMissing = true;
		KeyPropertyParams.KeyParams.bCreateKeyIfUnchanged = true;
		KeyPropertyParams.KeyParams.bCreateKeyIfEmpty = true;
	}

	Sequencer.KeyProperty(KeyPropertyParams);
}


#undef LOCTEXT_NAMESPACE
