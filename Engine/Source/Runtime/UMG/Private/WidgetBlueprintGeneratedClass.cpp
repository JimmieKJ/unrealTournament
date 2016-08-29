// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieScene.h"
#include "WidgetAnimation.h"
#include "Engine/InputDelegateBinding.h"
#include "TextReferenceCollector.h"

#define LOCTEXT_NAMESPACE "UMG"

#if WITH_EDITORONLY_DATA
namespace
{
	void CollectWidgetBlueprintGeneratedClassTextReferences(UObject* Object, FArchive& Ar)
	{
		// In an editor build, both UWidgetBlueprint and UWidgetBlueprintGeneratedClass reference an identical WidgetTree.
		// So we ignore the UWidgetBlueprintGeneratedClass when looking for persistent text references since it will be overwritten by the UWidgetBlueprint version.
	}
}
#endif

/////////////////////////////////////////////////////
// UWidgetBlueprintGeneratedClass

UWidgetBlueprintGeneratedClass::UWidgetBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	{ static const FAutoRegisterTextReferenceCollectorCallback AutomaticRegistrationOfTextReferenceCollector(UWidgetBlueprintGeneratedClass::StaticClass(), &CollectWidgetBlueprintGeneratedClassTextReferences); }
#endif
}

void UWidgetBlueprintGeneratedClass::InitializeWidgetStatic(UUserWidget* UserWidget
	, const UClass* InClass
	, bool InCanEverTick
	, bool InCanEverPaint
	, UWidgetTree* InWidgetTree
	, const TArray< UWidgetAnimation* >& InAnimations
	, const TArray< FDelegateRuntimeBinding >& InBindings)
{
	check(InClass);

	UserWidget->bCanEverTick = InCanEverTick;
	UserWidget->bCanEverPaint = InCanEverPaint;

	UWidgetTree* ClonedTree = UserWidget->WidgetTree;

	// Normally the ClonedTree should be null - we do in the case of design time with the widget, actually
	// clone the widget tree directly from the WidgetBlueprint so that the rebuilt preview matches the newest
	// widget tree, without a full blueprint compile being required.  In that case, the WidgetTree on the UserWidget
	// will have already been initialized to some value.  When that's the case, we'll avoid duplicating it from the class
	// similar to how we use to use the DesignerWidgetTree.
	if ( ClonedTree == nullptr )
	{
		ClonedTree = DuplicateObject<UWidgetTree>(InWidgetTree, UserWidget);
	}

	UserWidget->WidgetGeneratedByClass = InClass;

#if WITH_EDITOR
	UserWidget->WidgetGeneratedBy = InClass->ClassGeneratedBy;
#endif

	if (ClonedTree)
	{
		UserWidget->WidgetTree = ClonedTree;

		UClass* WidgetBlueprintClass = UserWidget->GetClass();

		for (UWidgetAnimation* Animation : InAnimations)
		{
			UWidgetAnimation* Anim = DuplicateObject<UWidgetAnimation>(Animation, UserWidget);

			if( Anim->GetMovieScene() )
			{
				// Find property with the same name as the template and assign the new widget to it.
				UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetBlueprintClass, Anim->GetMovieScene()->GetFName());
				if (Prop)
				{
					Prop->SetObjectPropertyValue_InContainer(UserWidget, Anim);
				}
			}
		}

		ClonedTree->ForEachWidget([&](UWidget* Widget) {
			// Not fatal if NULL, but shouldn't happen
			if (!ensure(Widget != nullptr))
			{
				return;
			}

			Widget->WidgetGeneratedByClass = InClass;

#if WITH_EDITOR
			Widget->WidgetGeneratedBy = InClass->ClassGeneratedBy;
#endif

			// TODO UMG Make this an FName
			FString VariableName = Widget->GetName();

			Widget->bCreatedByConstructionScript = true; // Indicate it comes from a blueprint so it gets cleared when we rerun construction scripts

			// Find property with the same name as the template and assign the new widget to it.
			UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetBlueprintClass, *VariableName);
			if (Prop)
			{
				Prop->SetObjectPropertyValue_InContainer(UserWidget, Widget);
				UObject* Value = Prop->GetObjectPropertyValue_InContainer(UserWidget);
				check(Value == Widget);
			}

			// Perform binding
			for (const FDelegateRuntimeBinding& Binding : InBindings)
			{
				//TODO UMG Make this faster.
				if (Binding.ObjectName == VariableName)
				{
					UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>(Widget->GetClass(), FName(*(Binding.PropertyName.ToString() + TEXT("Delegate"))));
					if (!DelegateProperty)
					{
						DelegateProperty = FindField<UDelegateProperty>(Widget->GetClass(), Binding.PropertyName);
					}

					if (DelegateProperty)
					{
						bool bSourcePathBound = false;

						if (Binding.SourcePath.IsValid())
						{
							bSourcePathBound = Widget->AddBinding(DelegateProperty, UserWidget, Binding.SourcePath);
						}

						// If no native binder is found then the only possibility is that the binding is for
						// a delegate that doesn't match the known native binders available and so we
						// fallback to just attempting to bind to the function directly.
						if (bSourcePathBound == false)
						{
							FScriptDelegate* ScriptDelegate = DelegateProperty->GetPropertyValuePtr_InContainer(Widget);
							if (ScriptDelegate)
							{
								ScriptDelegate->BindUFunction(UserWidget, Binding.FunctionName);
							}
						}
					}
				}
			}

			// Initialize Navigation Data
			if (Widget->Navigation)
			{
				Widget->Navigation->ResolveExplictRules(ClonedTree);
			}

#if WITH_EDITOR
			Widget->ConnectEditorData();
#endif
		});

		// Bind any delegates on widgets
		UBlueprintGeneratedClass::BindDynamicDelegates(InClass, UserWidget);

		//TODO UMG Add OnWidgetInitialized?
	}
}

void UWidgetBlueprintGeneratedClass::InitializeWidget(UUserWidget* UserWidget) const
{
	InitializeWidgetStatic(UserWidget, this, bCanEverTick, bCanEverPaint, WidgetTree, Animations, Bindings);
}

void UWidgetBlueprintGeneratedClass::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_RENAME_WIDGET_VISIBILITY )
	{
		static const FName Visiblity(TEXT("Visiblity"));
		static const FName Visibility(TEXT("Visibility"));

		for ( FDelegateRuntimeBinding& Binding : Bindings )
		{
			if ( Binding.PropertyName == Visiblity )
			{
				Binding.PropertyName = Visibility;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
