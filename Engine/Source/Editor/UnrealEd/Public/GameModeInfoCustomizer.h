// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDocumentation.h"
#include "KismetEditorUtilities.h"
#include "EditorClassUtils.h"
#include "GameFramework/GameMode.h"
#include "Engine/BlueprintGeneratedClass.h"

#define LOCTEXT_NAMESPACE "FGameModeInfoCustomizer"

static FString GameModeCategory(LOCTEXT("GameModeCategory", "GameMode").ToString());

/** Class to help customize a GameMode class picker, to show settings 'withing' GameMode. */
class FGameModeInfoCustomizer : public TSharedFromThis<FGameModeInfoCustomizer>
{
public:
	FGameModeInfoCustomizer(UObject* InOwningObject, FName InGameModePropertyName)
	{
		OwningObject = InOwningObject;
		GameModePropertyName = InGameModePropertyName;
		CachedGameModeClass = nullptr;
	}

	/** Create widget for the name of a default class property */
	TSharedRef<SWidget> CreateGameModePropertyLabelWidget(FName PropertyName)
	{
		UProperty* Prop = FindFieldChecked<UProperty>(AGameMode::StaticClass(), PropertyName);

		FString DisplayName = Prop->GetDisplayNameText().ToString();
		if (DisplayName.Len() == 0)
		{
			DisplayName = Prop->GetName();
		}
		DisplayName = FName::NameToDisplayString(DisplayName, false);

		return
			SNew(STextBlock)
			.Text(FText::FromString(DisplayName))
			.ToolTip(IDocumentation::Get()->CreateToolTip(Prop->GetToolTipText(), NULL, TEXT("Shared/Types/AGameMode"), Prop->GetName()))
			.Font(IDetailLayoutBuilder::GetDetailFont());
	}

	/** Create widget fo modifying a default class within the current GameMode */
	void CustomizeGameModeDefaultClass(IDetailGroup& Group, FName DefaultClassPropertyName)
	{
		// Find the metaclass of this property
		UClassProperty* ClassProp = FindFieldChecked<UClassProperty>(AGameMode::StaticClass(), DefaultClassPropertyName);

		UClass* MetaClass = ClassProp->MetaClass;
		const bool bAllowNone = !(ClassProp->PropertyFlags & CPF_NoClear);

		// This is inconsistent with all the other browsers, so disabling it for now
		//TAttribute<bool> CanBrowseAtrribute = TAttribute<bool>::Create( TAttribute<bool>::FGetter::CreateSP( this, &FGameModeInfoCustomizer::CanBrowseDefaultClass, DefaultClassPropertyName) ) ;

		// Add a row for choosing a new default class
		Group.AddWidgetRow()
		.NameContent()
		[
			CreateGameModePropertyLabelWidget(DefaultClassPropertyName)
		]
		.ValueContent()
		.MaxDesiredWidth(0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(125)
				[
					SNew(SClassPropertyEntryBox)
					.AllowNone(bAllowNone)
					.MetaClass(MetaClass)
					.IsEnabled(this, &FGameModeInfoCustomizer::AllowModifyGameMode)
					.SelectedClass(this, &FGameModeInfoCustomizer::OnGetDefaultClass, DefaultClassPropertyName)
					.OnSetClass(FOnSetClass::CreateSP(this, &FGameModeInfoCustomizer::OnSetDefaultClass, DefaultClassPropertyName))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FGameModeInfoCustomizer::OnMakeSelectedDefaultClassClicked, DefaultClassPropertyName))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FGameModeInfoCustomizer::OnBrowseDefaultClassClicked, DefaultClassPropertyName))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeNewBlueprintButton(FSimpleDelegate::CreateSP(this, &FGameModeInfoCustomizer::OnMakeNewDefaultClassClicked, DefaultClassPropertyName))
			]
		];
	}

	/** Add special customization for the GameMode setting */
	void CustomizeGameModeSetting(IDetailLayoutBuilder& LayoutBuilder, IDetailCategoryBuilder& CategoryBuilder)
	{
		// Add GameMode picker widget
		DefaultGameModeClassHandle = LayoutBuilder.GetProperty(GameModePropertyName);
		check(DefaultGameModeClassHandle.IsValid());

		IDetailPropertyRow& DefaultGameModeRow = CategoryBuilder.AddProperty(DefaultGameModeClassHandle);
		// SEe if we are allowed to choose 'no' GameMode
		const bool bAllowNone = !(DefaultGameModeClassHandle->GetProperty()->PropertyFlags & CPF_NoClear);

		// This is inconsistent with other property 
		//TAttribute<bool> CanBrowseAtrribute = TAttribute<bool>(this, &FGameModeInfoCustomizer::CanBrowseGameMode);

		DefaultGameModeRow
		.ShowPropertyButtons(false)
		.CustomWidget()
		.NameContent()
		[
			DefaultGameModeClassHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(125)
				[
					SNew(SClassPropertyEntryBox)
					.AllowNone(bAllowNone)
					.MetaClass(AGameMode::StaticClass())
					.SelectedClass(this, &FGameModeInfoCustomizer::GetCurrentGameModeClass)
					.OnSetClass(FOnSetClass::CreateSP(this, &FGameModeInfoCustomizer::SetCurrentGameModeClass))
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FGameModeInfoCustomizer::OnUseSelectedGameModeClicked, &LayoutBuilder))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FGameModeInfoCustomizer::OnBrowseGameModeClicked))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeNewBlueprintButton(FSimpleDelegate::CreateSP(this, &FGameModeInfoCustomizer::OnClickNewGameMode, &LayoutBuilder))
			]
		];

		static FName SelectedGameModeDetailsName(TEXT("SelectedGameModeDetails"));		
		IDetailGroup& Group = CategoryBuilder.AddGroup(SelectedGameModeDetailsName, LOCTEXT("SelectedGameModeDetails", "Selected GameMode"));

		// Then add rows to show key properties and let you edit them
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, DefaultPawnClass));
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, HUDClass));
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, PlayerControllerClass));
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, GameStateClass));
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, PlayerStateClass));
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, SpectatorClass));
	}

	/** Get the currently set GameMode class */
	const UClass* GetCurrentGameModeClass() const
	{
		FString ClassName;
		DefaultGameModeClassHandle->GetValueAsFormattedString(ClassName);

		// Blueprints may have type information before the class name, so make sure and strip that off now
		ConstructorHelpers::StripObjectClass(ClassName);

		// Do we have a valid cached class pointer? (Note: We can't search for the class while a save is happening)
		const UClass* GameModeClass = CachedGameModeClass.Get();
		if ((!GameModeClass || GameModeClass->GetPathName() != ClassName) && !GIsSavingPackage)
		{
			GameModeClass = FEditorClassUtils::GetClassFromString(ClassName);
			CachedGameModeClass = GameModeClass;
		}
		return GameModeClass;
	}

	void SetCurrentGameModeClass(const UClass* NewGameModeClass)
	{
		if (DefaultGameModeClassHandle->SetValueFromFormattedString((NewGameModeClass) ? NewGameModeClass->GetPathName() : TEXT("None")) == FPropertyAccess::Success)
		{
			CachedGameModeClass = NewGameModeClass;
		}
	}

	/** Get the CDO from the currently set GameMode class */
	AGameMode* GetCurrentGameModeCDO() const
	{
		UClass* GameModeClass = const_cast<UClass*>( GetCurrentGameModeClass() );
		if (GameModeClass != NULL)
		{
			return GameModeClass->GetDefaultObject<AGameMode>();
		}
		else
		{
			return NULL;
		}
	}

	/** Find the current default class by property name */
	const UClass* OnGetDefaultClass(FName ClassPropertyName) const
	{
		UClass* CurrentDefaultClass = NULL;
		const UClass* GameModeClass = GetCurrentGameModeClass();
		if (GameModeClass != NULL)
		{
			UClassProperty* ClassProp = FindFieldChecked<UClassProperty>(GameModeClass, ClassPropertyName);
			CurrentDefaultClass = (UClass*)ClassProp->GetObjectPropertyValue(ClassProp->ContainerPtrToValuePtr<void>(GetCurrentGameModeCDO()));
		}
		return CurrentDefaultClass;
	}

	/** Set a new default class by property name */
	void OnSetDefaultClass(const UClass* NewDefaultClass, FName ClassPropertyName)
	{
		const UClass* GameModeClass = GetCurrentGameModeClass();
		if (GameModeClass != NULL && AllowModifyGameMode())
		{
			UClassProperty* ClassProp = FindFieldChecked<UClassProperty>(GameModeClass, ClassPropertyName);
			const UClass** DefaultClassPtr = ClassProp->ContainerPtrToValuePtr<const UClass*>(GetCurrentGameModeCDO());
			*DefaultClassPtr = NewDefaultClass;

			// Indicate that the BP has changed and would need to be saved.
			GameModeClass->MarkPackageDirty();
		}
	}

	bool CanBrowseDefaultClass(FName ClassPropertyName) const
	{
		return CanSyncToClass(OnGetDefaultClass(ClassPropertyName));
	}

	void OnBrowseDefaultClassClicked(FName ClassPropertyName)
	{
		SyncBrowserToClass(OnGetDefaultClass(ClassPropertyName));
	}

	void OnMakeNewDefaultClassClicked(FName ClassPropertyName)
	{
		UClassProperty* ClassProp = FindFieldChecked<UClassProperty>(AGameMode::StaticClass(), ClassPropertyName);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(LOCTEXT("CreateNewBlueprint", "Create New Blueprint"), ClassProp->MetaClass, FString::Printf(TEXT("New%s"),*ClassProp->MetaClass->GetName()));

		if(Blueprint != NULL && Blueprint->GeneratedClass)
		{
			OnSetDefaultClass(Blueprint->GeneratedClass, ClassPropertyName);

			FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);
		}
	}

	void OnMakeSelectedDefaultClassClicked(FName ClassPropertyName)
	{
		UClassProperty* ClassProp = FindFieldChecked<UClassProperty>(AGameMode::StaticClass(), ClassPropertyName);
		const UClass* SelectedClass = GEditor->GetFirstSelectedClass(ClassProp->MetaClass);
		if (SelectedClass)
		{
			OnSetDefaultClass(SelectedClass, ClassPropertyName);
		}
	}

	bool CanBrowseGameMode() const
	{
		return CanSyncToClass(GetCurrentGameModeClass());
	}

	void OnBrowseGameModeClicked()
	{
		SyncBrowserToClass(GetCurrentGameModeClass());
	}

	bool CanSyncToClass(const UClass* Class) const
	{
		return (Class != NULL && Class->ClassGeneratedBy != NULL);
	}

	void SyncBrowserToClass(const UClass* Class)
	{
		if (CanSyncToClass(Class))
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
			if (ensure(Blueprint != NULL))
			{
				TArray<UObject*> SyncObjects;
				SyncObjects.Add(Blueprint);
				GEditor->SyncBrowserToObjects(SyncObjects);
			}
		}
	}

	void OnUseSelectedGameModeClicked(IDetailLayoutBuilder* DetailLayout)
	{
		const UClass* SelectedClass = GEditor->GetFirstSelectedClass(AGameMode::StaticClass());
		if (SelectedClass)
		{
			DefaultGameModeClassHandle->SetValueFromFormattedString(SelectedClass->GetPathName());
		}

		if (DetailLayout)
		{
			DetailLayout->ForceRefreshDetails();
		}
	}

	void OnClickNewGameMode(IDetailLayoutBuilder* DetailLayout)
	{
		// Create a new GameMode BP
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(LOCTEXT("CreateNewGameMode", "Create New GameMode"), AGameMode::StaticClass(), TEXT("NewGameMode"));
		// if that worked, assign it
		if(Blueprint != NULL && Blueprint->GeneratedClass)
		{
			DefaultGameModeClassHandle->SetValueFromFormattedString(Blueprint->GeneratedClass->GetPathName());
		}

		if (DetailLayout)
		{
			DetailLayout->ForceRefreshDetails();
		}
	}

	/** Are we allowed to modify the currently selected GameMode */
	bool AllowModifyGameMode() const
	{
		// Only allow editing GameMode BP, not native class!
		const UBlueprintGeneratedClass* GameModeBPClass = Cast<UBlueprintGeneratedClass>(GetCurrentGameModeClass());
		return (GameModeBPClass != NULL);
	}

private:
	/** Object that owns the pointer to the GameMode we want to customize */
	TWeakObjectPtr<UObject>	OwningObject;
	/** Name of GameMode property inside OwningObject */
	FName GameModePropertyName;
	/** Handle to the DefaultGameMode property */
	TSharedPtr<IPropertyHandle> DefaultGameModeClassHandle;
	/** Cached class pointer from the DefaultGameModeClassHandle */
	mutable TWeakObjectPtr<UClass> CachedGameModeClass;
};

#undef LOCTEXT_NAMESPACE
