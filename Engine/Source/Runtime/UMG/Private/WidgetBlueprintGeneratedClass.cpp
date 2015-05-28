// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieScene.h"
#include "WidgetAnimation.h"
#include "Engine/InputDelegateBinding.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UWidgetBlueprintGeneratedClass

UWidgetBlueprintGeneratedClass::UWidgetBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetBlueprintGeneratedClass::InitializeWidget(UUserWidget* UserWidget) const
{
	UWidgetTree* ClonedTree = DuplicateObject<UWidgetTree>( WidgetTree, UserWidget );

#if WITH_EDITOR
	UserWidget->WidgetGeneratedBy = ClassGeneratedBy;
#endif

	if ( ClonedTree )
	{
		ClonedTree->SetFlags(RF_Transactional);

		UserWidget->WidgetTree = ClonedTree;

		UClass* WidgetBlueprintClass = UserWidget->GetClass();

		for(UWidgetAnimation* Animation : Animations)
		{
			UWidgetAnimation* Anim = DuplicateObject<UWidgetAnimation>( Animation, UserWidget );

			if( Anim->MovieScene )
			{
				// Find property with the same name as the template and assign the new widget to it.
				UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetBlueprintClass, Anim->MovieScene->GetFName());
				if(Prop)
				{
					Prop->SetObjectPropertyValue_InContainer(UserWidget, Anim);
				}
			}
		}

		ClonedTree->ForEachWidget([&] (UWidget* Widget) {
			// Not fatal if NULL, but shouldn't happen
			if ( !ensure(Widget != nullptr) )
			{
				return;
			}

#if WITH_EDITOR
			Widget->WidgetGeneratedBy = ClassGeneratedBy;
#endif

			// TODO UMG Make this an FName
			FString VariableName = Widget->GetName();

			Widget->bCreatedByConstructionScript = true; // Indicate it comes from a blueprint so it gets cleared when we rerun construction scripts

			// Find property with the same name as the template and assign the new widget to it.
			UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetBlueprintClass, *VariableName);
			if ( Prop )
			{
				Prop->SetObjectPropertyValue_InContainer(UserWidget, Widget);
				UObject* Value = Prop->GetObjectPropertyValue_InContainer(UserWidget);
				check(Value == Widget);
			}

			// Perform binding
			for ( const FDelegateRuntimeBinding& Binding : Bindings )
			{
				//TODO UMG Make this faster.
				if ( Binding.ObjectName == VariableName )
				{
					UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>(Widget->GetClass(), FName(*( Binding.PropertyName.ToString() + TEXT("Delegate") )));
					if ( !DelegateProperty )
					{
						DelegateProperty = FindField<UDelegateProperty>(Widget->GetClass(), Binding.PropertyName);
					}

					if ( DelegateProperty )
					{
						bool bSourcePathBound = false;

						if ( Binding.SourcePath.IsValid() )
						{
							bSourcePathBound = Widget->AddBinding(DelegateProperty, UserWidget, Binding.SourcePath);
						}

						// If no native binder is found then the only possibility is that the binding is for
						// a delegate that doesn't match the known native binders available and so we
						// fallback to just attempting to bind to the function directly.
						if ( bSourcePathBound == false )
						{
							FScriptDelegate* ScriptDelegate = DelegateProperty->GetPropertyValuePtr_InContainer(Widget);
							if ( ScriptDelegate )
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
		BindDynamicDelegates(UserWidget);

		//TODO UMG Add OnWidgetInitialized?
	}
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
