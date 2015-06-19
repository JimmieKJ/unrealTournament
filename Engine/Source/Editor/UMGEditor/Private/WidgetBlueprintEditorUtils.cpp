// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "WidgetBlueprintEditorUtils.h"
#include "WidgetBlueprintEditor.h"
#include "Kismet2NameValidators.h"
#include "BlueprintEditorUtils.h"
#include "K2Node_Variable.h"
#include "WidgetTemplateClass.h"
#include "Factories.h"
#include "UnrealExporter.h"
#include "GenericCommands.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "ScopedTransaction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "CanvasPanel.h"
#include "WidgetSlotPair.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "UMG"

class FWidgetObjectTextFactory : public FCustomizableTextObjectFactory
{
public:
	FWidgetObjectTextFactory()
		: FCustomizableTextObjectFactory(GWarn)
	{
	}

	// FCustomizableTextObjectFactory implementation

	virtual bool CanCreateClass(UClass* ObjectClass) const override
	{
		const bool bIsWidget = ObjectClass->IsChildOf(UWidget::StaticClass());
		const bool bIsSlot = ObjectClass->IsChildOf(UPanelSlot::StaticClass());
		const bool bIsSlotMetaData = ObjectClass->IsChildOf(UWidgetSlotPair::StaticClass());

		return bIsWidget || bIsSlot || bIsSlotMetaData;
	}

	virtual void ProcessConstructedObject(UObject* NewObject) override
	{
		check(NewObject);

		if ( UWidget* Widget = Cast<UWidget>(NewObject) )
		{
			NewWidgetMap.Add(Widget->GetFName(), Widget);
		}
		else if ( UWidgetSlotPair* SlotMetaData = Cast<UWidgetSlotPair>(NewObject) )
		{
			MissingSlotData.Add(SlotMetaData->GetWidgetName(), SlotMetaData);
		}
	}

	// FCustomizableTextObjectFactory (end)

public:

	// Name->Instance object mapping
	TMap<FName, UWidget*> NewWidgetMap;

	// Instance->OldSlotMetaData that didn't survive the journey because it wasn't copied.
	TMap<FName, UWidgetSlotPair*> MissingSlotData;
};

bool FWidgetBlueprintEditorUtils::VerifyWidgetRename(TSharedRef<class FWidgetBlueprintEditor> BlueprintEditor, FWidgetReference Widget, const FText& NewName, FText& OutErrorMessage)
{
	if (NewName.IsEmptyOrWhitespace())
	{
		OutErrorMessage = LOCTEXT("EmptyWidgetName", "Empty Widget Name");
		return false;
	}

	const FString& NewNameString = NewName.ToString();

	if (NewNameString.Len() >= NAME_SIZE)
	{
		OutErrorMessage = LOCTEXT("WidgetNameTooLong", "Widget Name is Too Long");
		return false;
	}

	const FName NewFName(*NewNameString);
	
	UWidgetBlueprint* Blueprint = BlueprintEditor->GetWidgetBlueprintObj();
	UWidget* ExistingTemplate = Blueprint->WidgetTree->FindWidget(NewFName);

	bool bIsSameWidget = false;
	if (ExistingTemplate != nullptr)
	{
		if (Widget.GetTemplate() != ExistingTemplate)
		{
			OutErrorMessage = LOCTEXT("ExistingWidgetName", "Existing Widget Name");
			return false;
		}
		else
		{
			bIsSameWidget = true;
		}
	}

	FKismetNameValidator Validator(Blueprint);

	const bool bUniqueNameForVariable = (EValidatorResult::Ok == Validator.IsValid(NewFName));

	if (!bUniqueNameForVariable && !bIsSameWidget)
	{
		OutErrorMessage = LOCTEXT("ExistingVariableName", "Existing Variable Name");
		return false;
	}

	return true;
}

bool FWidgetBlueprintEditorUtils::RenameWidget(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, const FName& OldName, const FName& NewName)
{
	UWidgetBlueprint* Blueprint = BlueprintEditor->GetWidgetBlueprintObj();
	check(Blueprint);

	bool bRenamed = false;

	TSharedPtr<INameValidatorInterface> NameValidator = MakeShareable(new FKismetNameValidator(Blueprint));

	// NewName should be already validated. But one must make sure that NewTemplateName is also unique.
	const bool bUniqueNameForTemplate = ( EValidatorResult::Ok == NameValidator->IsValid(NewName) );

	const FString NewNameStr = NewName.ToString();
	const FString OldNameStr = OldName.ToString();

	UWidget* Widget = Blueprint->WidgetTree->FindWidget(OldName);
	check(Widget);

	if ( Widget )
	{
		const FScopedTransaction Transaction(LOCTEXT("RenameWidget", "Rename Widget"));

		// Rename Template
		Blueprint->Modify();
		Widget->Modify();


		// Rename Preview before renaming the template widget so the preview widget can be found
		UWidget* WidgetPreview = BlueprintEditor->GetReferenceFromTemplate(Widget).GetPreview();
		if(WidgetPreview)
		{
			WidgetPreview->Rename(*NewNameStr);
		}

		// Find and update all variable references in the graph

		Widget->Rename(*NewNameStr);

		// Update Variable References and
		// Update Event References to member variables
		FBlueprintEditorUtils::ReplaceVariableReferences(Blueprint, OldName, NewName);
		
		// Find and update all binding references in the widget blueprint
		for ( FDelegateEditorBinding& Binding : Blueprint->Bindings )
		{
			if ( Binding.ObjectName == OldNameStr )
			{
				Binding.ObjectName = NewNameStr;
			}
		}

		// Update widget blueprint names
		for( UWidgetAnimation* WidgetAnimation : Blueprint->Animations )
		{
			for( FWidgetAnimationBinding& AnimBinding : WidgetAnimation->AnimationBindings )
			{
				if( AnimBinding.WidgetName == OldName )
				{
					AnimBinding.WidgetName = NewName;
				}
			}
		}

		// Update named slot bindings?
		// TODO...

		// Validate child blueprints and adjust variable names to avoid a potential name collision
		FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, NewName);

		// Refresh references and flush editors
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		bRenamed = true;
	}

	return bRenamed;
}

void FWidgetBlueprintEditorUtils::CreateWidgetContextMenu(FMenuBuilder& MenuBuilder, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, FVector2D TargetLocation)
{
	BlueprintEditor->PasteDropLocation = TargetLocation;

	TSet<FWidgetReference> Widgets = BlueprintEditor->GetSelectedWidgets();
	UWidgetBlueprint* BP = BlueprintEditor->GetWidgetBlueprintObj();

	MenuBuilder.PushCommandList(BlueprintEditor->DesignerCommandList.ToSharedRef());

	MenuBuilder.BeginSection("Edit", LOCTEXT("Edit", "Edit"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Actions");
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "EditBlueprint_Label", "Edit Widget Blueprint..." ),
			LOCTEXT( "EditBlueprint_Tooltip", "Open the selected Widget Blueprint(s) for edit." ),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic( &FWidgetBlueprintEditorUtils::ExecuteOpenSelectedWidgetsForEdit, Widgets ),
				FCanExecuteAction(),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateStatic( &FWidgetBlueprintEditorUtils::CanOpenSelectedWidgetsForEdit, Widgets )
				)
			);

		MenuBuilder.AddSubMenu(
			LOCTEXT("WidgetTree_WrapWith", "Wrap With..."),
			LOCTEXT("WidgetTree_WrapWithToolTip", "Wraps the currently selected widgets inside of another container widget"),
			FNewMenuDelegate::CreateStatic(&FWidgetBlueprintEditorUtils::BuildWrapWithMenu, BP, Widgets)
			);

		if ( Widgets.Num() == 1 )
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("WidgetTree_ReplaceWith", "Replace With..."),
				LOCTEXT("WidgetTree_ReplaceWithToolTip", "Replaces the currently selected panel widget, with another panel widget"),
				FNewMenuDelegate::CreateStatic(&FWidgetBlueprintEditorUtils::BuildReplaceWithMenu, BP, Widgets)
				);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.PopCommandList();
}

void FWidgetBlueprintEditorUtils::ExecuteOpenSelectedWidgetsForEdit( TSet<FWidgetReference> SelectedWidgets )
{
	for ( auto& Widget : SelectedWidgets )
	{
		FAssetEditorManager::Get().OpenEditorForAsset( Widget.GetTemplate()->GetClass()->ClassGeneratedBy );
	}
}

bool FWidgetBlueprintEditorUtils::CanOpenSelectedWidgetsForEdit( TSet<FWidgetReference> SelectedWidgets )
{
	bool bCanOpenAllForEdit = SelectedWidgets.Num() > 0;
	for ( auto& Widget : SelectedWidgets )
	{
		auto Blueprint = Widget.GetTemplate()->GetClass()->ClassGeneratedBy;
		if ( !Blueprint || !Blueprint->IsA( UWidgetBlueprint::StaticClass() ) )
		{
			bCanOpenAllForEdit = false;
			break;
		}
	}

	return bCanOpenAllForEdit;
}

void FWidgetBlueprintEditorUtils::DeleteWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets)
{
	if ( Widgets.Num() > 0 )
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveWidget", "Remove Widget"));
		BP->WidgetTree->SetFlags(RF_Transactional);
		BP->WidgetTree->Modify();

		bool bRemoved = false;
		for ( FWidgetReference& Item : Widgets )
		{
			UWidget* WidgetTemplate = Item.GetTemplate();

			// Modify the widget's parent
			UPanelWidget* Parent = WidgetTemplate->GetParent();
			if ( Parent )
			{
				Parent->Modify();
			}
			
			// Modify the widget being removed.
			WidgetTemplate->Modify();

			bRemoved = BP->WidgetTree->RemoveWidget(WidgetTemplate);

			// If the widget we're removing doesn't have a parent it may be rooted in a named slot,
			// so check there as well.
			if ( WidgetTemplate->GetParent() == nullptr )
			{
				bRemoved |= FindAndRemoveNamedSlotContent(WidgetTemplate, BP->WidgetTree);
			}

			// Rename the removed widget to the transient package so that it doesn't conflict with future widgets sharing the same name.
			WidgetTemplate->Rename(nullptr, nullptr);

			// Rename all child widgets as well, to the transient package so that they don't conflict with future widgets sharing the same name.
			TArray<UWidget*> ChildWidgets;
			UWidgetTree::GetChildWidgets(WidgetTemplate, ChildWidgets);
			for ( UWidget* Widget : ChildWidgets )
			{
				Widget->Rename(nullptr, nullptr);
			}
		}

		//TODO UMG There needs to be an event for widget removal so that caches can be updated, and selection

		if ( bRemoved )
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		}
	}
}

bool FWidgetBlueprintEditorUtils::FindAndRemoveNamedSlotContent(UWidget* WidgetTemplate, UWidgetTree* WidgetTree)
{
	bool bSuccess = false;

	WidgetTree->ForEachWidget([&] (UWidget* Widget) {

		if ( bSuccess )
		{
			return;
		}

		if ( INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Widget) )
		{
			TArray<FName> SlotNames;
			NamedSlotHost->GetSlotNames(SlotNames);

			for ( FName SlotName : SlotNames )
			{
				if ( UWidget* SlotContent = NamedSlotHost->GetContentForSlot(SlotName) )
				{
					if ( SlotContent == WidgetTemplate )
					{
						NamedSlotHost->SetContentForSlot(SlotName, nullptr);
						bSuccess = true;
					}
				}
			}
		}
	});

	return bSuccess;
}

void FWidgetBlueprintEditorUtils::BuildWrapWithMenu(FMenuBuilder& Menu, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets)
{
	Menu.BeginSection("WrapWith", LOCTEXT("WidgetTree_WrapWith", "Wrap With..."));
	{
		for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
		{
			UClass* WidgetClass = *ClassIt;
			if ( FWidgetBlueprintEditorUtils::IsUsableWidgetClass(WidgetClass) )
			{
				if ( WidgetClass->IsChildOf(UPanelWidget::StaticClass()) )
				{
					Menu.AddMenuEntry(
						WidgetClass->GetDisplayNameText(),
						FText::GetEmpty(),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateStatic(&FWidgetBlueprintEditorUtils::WrapWidgets, BP, Widgets, WidgetClass),
							FCanExecuteAction()
						));
				}
			}
		}
	}
	Menu.EndSection();
}

void FWidgetBlueprintEditorUtils::WrapWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, UClass* WidgetClass)
{
	const FScopedTransaction Transaction(LOCTEXT("WrapWidgets", "Wrap Widgets"));

	TSharedPtr<FWidgetTemplateClass> Template = MakeShareable(new FWidgetTemplateClass(WidgetClass));

	// Old Parent -> New Parent Map
	TMap<UPanelWidget*, UPanelWidget*> OldParentToNewParent;

	for ( FWidgetReference& Item : Widgets )
	{
		int32 OutIndex;
		UPanelWidget* CurrentParent = BP->WidgetTree->FindWidgetParent(Item.GetTemplate(), OutIndex);

		// If the widget doesn't currently have a parent, and isn't the root, ignore it.
		if ( CurrentParent == nullptr && Item.GetTemplate() != BP->WidgetTree->RootWidget )
		{
			continue;
		}

		UPanelWidget*& NewWrapperWidget = OldParentToNewParent.FindOrAdd(CurrentParent);
		if ( NewWrapperWidget == nullptr || !NewWrapperWidget->CanAddMoreChildren() )
		{
			NewWrapperWidget = CastChecked<UPanelWidget>(Template->Create(BP->WidgetTree));

			if ( CurrentParent )
			{
				CurrentParent->Modify();
				CurrentParent->ReplaceChildAt(OutIndex, NewWrapperWidget);
			}
			else // Root Widget
			{
				BP->WidgetTree->Modify();
				BP->WidgetTree->RootWidget = NewWrapperWidget;
			}
		}

		NewWrapperWidget->AddChild(Item.GetTemplate());
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
}

void FWidgetBlueprintEditorUtils::BuildReplaceWithMenu(FMenuBuilder& Menu, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets)
{
	Menu.BeginSection("ReplaceWith", LOCTEXT("WidgetTree_ReplaceWith", "Replace With..."));
	{
		for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
		{
			UClass* WidgetClass = *ClassIt;
			if ( FWidgetBlueprintEditorUtils::IsUsableWidgetClass(WidgetClass) )
			{
				if ( WidgetClass->IsChildOf(UPanelWidget::StaticClass()) )
				{
					// Only allow replacement with panels that accept multiple children
					if ( WidgetClass->GetDefaultObject<UPanelWidget>()->CanHaveMultipleChildren() )
					{
						Menu.AddMenuEntry(
							WidgetClass->GetDisplayNameText(),
							FText::GetEmpty(),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateStatic(&FWidgetBlueprintEditorUtils::ReplaceWidgets, BP, Widgets, WidgetClass),
								FCanExecuteAction()
							));
					}
				}
			}
		}
	}
	Menu.EndSection();
}

void FWidgetBlueprintEditorUtils::ReplaceWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, UClass* WidgetClass)
{
	const FScopedTransaction Transaction(LOCTEXT("ReplaceWidgets", "Replace Widgets"));

	TSharedPtr<FWidgetTemplateClass> Template = MakeShareable(new FWidgetTemplateClass(WidgetClass));

	for ( FWidgetReference& Item : Widgets )
	{
		if ( UPanelWidget* ExistingPanel = Cast<UPanelWidget>(Item.GetTemplate()) )
		{
			UPanelWidget* NewReplacementWidget = CastChecked<UPanelWidget>(Template->Create(BP->WidgetTree));

			ExistingPanel->Modify();

			if ( UPanelWidget* CurrentParent = ExistingPanel->GetParent() )
			{
				CurrentParent->Modify();
				CurrentParent->ReplaceChild(ExistingPanel, NewReplacementWidget);
			}
			else if ( Item.GetTemplate() == BP->WidgetTree->RootWidget )
			{
				BP->WidgetTree->Modify();
				BP->WidgetTree->RootWidget = NewReplacementWidget;
			}
			else
			{
				continue;
			}

			while ( ExistingPanel->GetChildrenCount() > 0 )
			{
				UWidget* Widget = ExistingPanel->GetChildAt(0);
				Widget->Modify();

				NewReplacementWidget->AddChild(Widget);
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
}

void FWidgetBlueprintEditorUtils::CutWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets)
{
	CopyWidgets(BP, Widgets);
	DeleteWidgets(BP, Widgets);
}

void FWidgetBlueprintEditorUtils::CopyWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets)
{
	TArray<UWidget*> CopyableWidets;
	for ( const FWidgetReference& Widget : Widgets )
	{
		UWidget* ParentWidget = Widget.GetTemplate();
		CopyableWidets.Add(ParentWidget);

		// When copying a widget users expect all sub widgets to be copied as well, so we need to ensure that
		// we gather all the child widgets and copy them as well.
		UWidgetTree::GetChildWidgets(ParentWidget, CopyableWidets);
	}

	TSet<UWidget*> CopyableWidetsSet(CopyableWidets);

	FString ExportedText;
	FWidgetBlueprintEditorUtils::ExportWidgetsToText(CopyableWidetsSet, /*out*/ ExportedText);
	FPlatformMisc::ClipboardCopy(*ExportedText);
}

void FWidgetBlueprintEditorUtils::ExportWidgetsToText(TSet<UWidget*> WidgetsToExport, /*out*/ FString& ExportedText)
{
	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	// Export each of the selected nodes
	UObject* LastOuter = nullptr;
	for ( UWidget* Widget : WidgetsToExport )
	{
		// The nodes should all be from the same scope
		UObject* ThisOuter = Widget->GetOuter();
		check(( LastOuter == ThisOuter ) || ( LastOuter == nullptr ));
		LastOuter = ThisOuter;

		UExporter::ExportToOutputDevice(&Context, Widget, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ThisOuter);

		// Check to see if this widget was content of another widget holding it in a named slot.
		if ( Widget->GetParent() == nullptr )
		{
			for ( UWidget* ExportableWidget : WidgetsToExport )
			{
				if ( INamedSlotInterface* NamedSlotContainer = Cast<INamedSlotInterface>(ExportableWidget) )
				{
					if ( NamedSlotContainer->ContainsContent(Widget) )
					{
						continue;
					}
				}
			}
		}

		if ( Widget->GetParent() == nullptr || !WidgetsToExport.Contains(Widget->GetParent()) )
		{
			auto SlotMetaData = NewObject<UWidgetSlotPair>();
			SlotMetaData->SetWidget(Widget);

			UExporter::ExportToOutputDevice(&Context, SlotMetaData, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, nullptr);
		}
	}

	ExportedText = Archive;
}

void FWidgetBlueprintEditorUtils::PasteWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference ParentWidgetRef, FVector2D PasteLocation)
{
	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());

	UPanelWidget* ParentWidget = nullptr;
	
	if ( ParentWidgetRef.IsValid() )
	{
		ParentWidget = CastChecked<UPanelWidget>(ParentWidgetRef.GetTemplate());
	}
	
	// TODO UMG Find paste parent, may not be the selected widget...  Maybe it should be the parent of the copied widget until,
	// we do a paste here, from a right click menu.

	if ( !ParentWidget )
	{
		// If we already have a root widget, then we can't replace the root.
		if ( BP->WidgetTree->RootWidget )
		{
			return;
		}
	}

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UWidget*> PastedWidgets;
	TMap<FName, UWidgetSlotPair*> PastedExtraSlotData;
	FWidgetBlueprintEditorUtils::ImportWidgetsFromText(BP, TextToImport, /*out*/ PastedWidgets, /*out*/ PastedExtraSlotData);

	// Ignore an empty set of widget paste data.
	if ( PastedWidgets.Num() == 0 )
	{
		return;
	}

	TArray<UWidget*> RootPasteWidgets;
	for ( UWidget* NewWidget : PastedWidgets )
	{
		// Widgets with a null parent mean that they were the root most widget of their selection set when
		// they were copied and thus we need to paste only the root most widgets.  All their children will be added
		// automatically.
		if ( NewWidget->GetParent() == nullptr )
		{
			RootPasteWidgets.Add(NewWidget);
		}
	}

	// If there isn't a root widget and we're copying multiple root widgets, then we need to add a container root
	// to hold the pasted data since multiple root widgets isn't permitted.
	if ( !ParentWidget && RootPasteWidgets.Num() > 1 )
	{
		ParentWidget = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		BP->WidgetTree->Modify();
		BP->WidgetTree->RootWidget = ParentWidget;
	}

	if ( ParentWidget )
	{
		if ( !ParentWidget->CanHaveMultipleChildren() )
		{
			if ( ParentWidget->GetChildrenCount() > 0 || RootPasteWidgets.Num() > 1 )
			{
				FNotificationInfo Info(LOCTEXT("NotEnoughSlots", "Can't paste contents, not enough available slots in target widget."));
				FSlateNotificationManager::Get().AddNotification(Info);

				return;
			}
		}

		ParentWidget->Modify();

		for ( UWidget* NewWidget : RootPasteWidgets )
		{
			UPanelSlot* Slot = ParentWidget->AddChild(NewWidget);
			if ( Slot )
			{
				if ( UWidgetSlotPair* OldSlotData = PastedExtraSlotData.FindRef(NewWidget->GetFName()) )
				{
					TMap<FName, FString> OldSlotProperties;
					OldSlotData->GetSlotProperties(OldSlotProperties);
					FWidgetBlueprintEditorUtils::ImportPropertiesFromText(Slot, OldSlotProperties);
				}

				BlueprintEditor->AddPostDesignerLayoutAction(
				[=] {
					FWidgetReference WidgetRef = BlueprintEditor->GetReferenceFromTemplate(NewWidget);
					UPanelSlot* PreviewSlot = WidgetRef.GetPreview()->Slot;
					UPanelSlot* TemplateSlot = WidgetRef.GetTemplate()->Slot;
					
					if ( UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PreviewSlot) )
					{
						CanvasSlot->SaveBaseLayout();
						CanvasSlot->SetDesiredPosition(PasteLocation);
						CanvasSlot->RebaseLayout();
					}

					TMap<FName, FString> SlotProperties;
					FWidgetBlueprintEditorUtils::ExportPropertiesToText(PreviewSlot, SlotProperties);
					FWidgetBlueprintEditorUtils::ImportPropertiesFromText(TemplateSlot, SlotProperties);
				});
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	else
	{
		check(RootPasteWidgets.Num() == 1)
		// If we've arrived here, we must be creating the root widget from paste data, and there can only be
		// one item in the paste data by now.
		BP->WidgetTree->Modify();

		for ( UWidget* NewWidget : RootPasteWidgets )
		{
			BP->WidgetTree->RootWidget = NewWidget;
			break;
		}
		
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
}

void FWidgetBlueprintEditorUtils::ImportWidgetsFromText(UWidgetBlueprint* BP, const FString& TextToImport, /*out*/ TSet<UWidget*>& ImportedWidgetSet, /*out*/ TMap<FName, UWidgetSlotPair*>& PastedExtraSlotData)
{
	// We create our own transient package here so that we can deserialize the data in isolation and ensure unreferenced
	// objects not part of the deserialization set are unresolved.
	UPackage* TempPackage = NewObject<UPackage>(nullptr, TEXT("/Engine/UMG/Editor/Transient"), RF_Transient);
	TempPackage->AddToRoot();

	// Turn the text buffer into objects
	FWidgetObjectTextFactory Factory;
	Factory.ProcessBuffer(TempPackage, RF_Transactional, TextToImport);

	PastedExtraSlotData = Factory.MissingSlotData;

	for ( auto& Entry : Factory.NewWidgetMap )
	{
		UWidget* Widget = Entry.Value;

		ImportedWidgetSet.Add(Widget);

		Widget->SetFlags(RF_Transactional);

		// If there is an existing widget with the same name, rename the newly placed widget.
		FString WidgetOldName = Widget->GetName();
		if ( FindObject<UObject>(BP->WidgetTree, *WidgetOldName) )
		{
			UWidgetSlotPair* SlotData = PastedExtraSlotData.FindRef(Widget->GetFName());
			if ( SlotData )
			{
				PastedExtraSlotData.Remove(Widget->GetFName());
			}

			Widget->Rename(nullptr, BP->WidgetTree);

			if ( SlotData )
			{
				SlotData->SetWidgetName(Widget->GetFName());
				PastedExtraSlotData.Add(Widget->GetFName(), SlotData);
			}
		}
		else
		{
			Widget->Rename(*WidgetOldName, BP->WidgetTree);
		}
	}

	// Remove the temp package from the root now that it has served its purpose.
	TempPackage->RemoveFromRoot();
}

void FWidgetBlueprintEditorUtils::ExportPropertiesToText(UObject* Object, TMap<FName, FString>& ExportedProperties)
{
	if ( Object )
	{
		for ( TFieldIterator<UProperty> PropertyIt(Object->GetClass(), EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt )
		{
			UProperty* Property = *PropertyIt;

			// Don't serialize out object properties, we just want value data.
			if ( !Property->IsA<UObjectProperty>() )
			{
				FString ValueText;
				if ( Property->ExportText_InContainer(0, ValueText, Object, Object, Object, PPF_IncludeTransient) )
				{
					ExportedProperties.Add(Property->GetFName(), ValueText);
				}
			}
		}
	}
}

void FWidgetBlueprintEditorUtils::ImportPropertiesFromText(UObject* Object, const TMap<FName, FString>& ExportedProperties)
{
	if ( Object )
	{
		for ( const auto& Entry : ExportedProperties )
		{
			if ( UProperty* Property = FindField<UProperty>(Object->GetClass(), Entry.Key) )
			{
				FEditPropertyChain PropertyChain;
				PropertyChain.AddHead(Property);
				Object->PreEditChange(PropertyChain);

				Property->ImportText(*Entry.Value, Property->ContainerPtrToValuePtr<uint8>(Object), 0, Object);

				FPropertyChangedEvent ChangedEvent(Property);
				Object->PostEditChangeProperty(ChangedEvent);
			}
		}
	}
}

bool FWidgetBlueprintEditorUtils::IsUsableWidgetClass(UClass* WidgetClass)
{
	if ( WidgetClass->IsChildOf(UWidget::StaticClass()) )
	{
		// We aren't interested in classes that are experimental or cannot be instantiated
		bool bIsExperimental, bIsEarlyAccess;
		FObjectEditorUtils::GetClassDevelopmentStatus(WidgetClass, bIsExperimental, bIsEarlyAccess);
		const bool bIsInvalid = WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
		if ( bIsExperimental || bIsEarlyAccess || bIsInvalid )
		{
			return false;
		}

		// Don't include skeleton classes or the same class as the widget being edited
		const bool bIsSkeletonClass = WidgetClass->HasAnyFlags(RF_Transient) && WidgetClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

		// Check that the asset that generated this class is valid (necessary b/c of a larger issue wherein force delete does not wipe the generated class object)
		if ( bIsSkeletonClass )
		{
			return false;
		}

		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
