// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "Runtime/MovieScene/Public/MovieScene.h"
#include "Editor/UnrealEd/Public/Kismet2/StructureEditorUtils.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "PropertyTag.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintCompiler.h"
#include "PropertyBinding.h"
#include "Engine/UserDefinedStruct.h"

#define LOCTEXT_NAMESPACE "UMG"

FEditorPropertyPathSegment::FEditorPropertyPathSegment()
	: Struct(nullptr)
	, MemberName(NAME_None)
	, MemberGuid()
	, IsProperty(true)
{
}

FEditorPropertyPathSegment::FEditorPropertyPathSegment(const UProperty* InProperty)
{
	IsProperty = true;
	MemberName = InProperty->GetFName();
	if ( InProperty->GetOwnerStruct() )
	{
		Struct = InProperty->GetOwnerStruct();
		MemberGuid = FStructureEditorUtils::GetGuidForProperty(InProperty);
	}
	else if ( InProperty->GetOwnerClass() )
	{
		Struct = InProperty->GetOwnerClass();
		UBlueprint::GetGuidFromClassByFieldName<UProperty>(InProperty->GetOwnerClass(), InProperty->GetFName(), MemberGuid);
	}
	else
	{
		// Should not be possible to hit.
		check(false);
	}
}

FEditorPropertyPathSegment::FEditorPropertyPathSegment(const UFunction* InFunction)
{
	IsProperty = false;
	MemberName = InFunction->GetFName();
	if ( InFunction->GetOwnerClass() )
	{
		Struct = InFunction->GetOwnerClass();
		UBlueprint::GetGuidFromClassByFieldName<UFunction>(InFunction->GetOwnerClass(), InFunction->GetFName(), MemberGuid);
	}
	else
	{
		// Should not be possible to hit.
		check(false);
	}
}

FEditorPropertyPathSegment::FEditorPropertyPathSegment(const UEdGraph* InFunctionGraph)
{
	IsProperty = false;
	MemberName = InFunctionGraph->GetFName();
	UBlueprint* Blueprint = CastChecked<UBlueprint>(InFunctionGraph->GetOuter());
	Struct = Blueprint->GeneratedClass;
	check(Struct);
	MemberGuid = InFunctionGraph->GraphGuid;
}

void FEditorPropertyPathSegment::Rebase(UBlueprint* SegmentBase)
{
	Struct = SegmentBase->GeneratedClass;
}

bool FEditorPropertyPathSegment::ValidateMember(UDelegateProperty* DelegateProperty, FText& OutError) const
{
	// We may be binding to a function that doesn't have a explicit binder system that can handle it.  In that case
	// check to see if the function signatures are compatible, if it is, even if we don't have a binder we can just
	// directly bind the function to the delegate.
	if ( UFunction* Function = Cast<UFunction>(GetMember()) )
	{
		// Check the signatures to ensure these functions match.
		if ( Function->IsSignatureCompatibleWith(DelegateProperty->SignatureFunction, UFunction::GetDefaultIgnoredSignatureCompatibilityFlags() | CPF_ReturnParm) )
		{
			return true;
		}
	}

	// Next check to see if we have a binder suitable for handling this case.
	if ( DelegateProperty->SignatureFunction->NumParms == 1 )
	{
		if ( UProperty* ReturnProperty = DelegateProperty->SignatureFunction->GetReturnProperty() )
		{
			// TODO I don't like having the path segment system needing to have knowledge of the binding layer.
			// think about divorcing the two.

			// Find the binder that can handle the delegate return type.
			TSubclassOf<UPropertyBinding> Binder = UWidget::FindBinderClassForDestination(ReturnProperty);
			if ( Binder == nullptr )
			{
				OutError = FText::Format(LOCTEXT("Binding_Binder_NotFound", "Member:{0}: No binding exists for {1}."),
					GetMemberDisplayText(),
					ReturnProperty->GetClass()->GetDisplayNameText());
				return false;
			}

			if ( UField* Field = GetMember() )
			{
				if ( UProperty* Property = Cast<UProperty>(Field) )
				{
					// Ensure that the binder also can handle binding from the property we care about.
					if ( Binder->GetDefaultObject<UPropertyBinding>()->IsSupportedSource(Property) )
					{
						return true;
					}
					else
					{
						OutError = FText::Format(LOCTEXT("Binding_UnsupportedType_Property", "Member:{0} Unable to bind {1}, unsupported type."),
							GetMemberDisplayText(),
							Property->GetClass()->GetDisplayNameText());
						return false;
					}
				}
				else if ( UFunction* Function = Cast<UFunction>(Field) )
				{
					if ( Function->NumParms == 1 )
					{
						if ( Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure) )
						{
							if ( UProperty* MemberReturn = Function->GetReturnProperty() )
							{
								// Ensure that the binder also can handle binding from the property we care about.
								if ( Binder->GetDefaultObject<UPropertyBinding>()->IsSupportedSource(MemberReturn) )
								{
									return true;
								}
								else
								{
									OutError = FText::Format(LOCTEXT("Binding_UnsupportedType_Function", "Member:{0} Unable to bind {1}, unsupported type."),
										GetMemberDisplayText(),
										MemberReturn->GetClass()->GetDisplayNameText());
									return false;
								}
							}
							else
							{
								OutError = FText::Format(LOCTEXT("Binding_NoReturn", "Member:{0} Has no return value, unable to bind."),
									GetMemberDisplayText());
								return false;
							}
						}
						else
						{
							OutError = FText::Format(LOCTEXT("Binding_Pure", "Member:{0} Unable to bind, the function is not marked as pure."),
								GetMemberDisplayText());
							return false;
						}
					}
					else
					{
						OutError = FText::Format(LOCTEXT("Binding_NumArgs", "Member:{0} Has the wrong number of arguments, it needs to return 1 value and take no parameters."),
							GetMemberDisplayText());
						return false;
					}
				}
			}
		}
	}

	OutError = LOCTEXT("Binding_UnknownError", "Unknown Error");

	return false;
}

UField* FEditorPropertyPathSegment::GetMember() const
{
	FName FieldName = GetMemberName();
	if ( FieldName != NAME_None )
	{
		UField* Field = FindField<UField>(Struct, FieldName);
		//if ( Field == nullptr )
		//{
		//	if ( UClass* Class = Cast<UClass>(Struct) )
		//	{
		//		if ( UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy) )
		//		{
		//			if ( UClass* SkeletonClass = Blueprint->SkeletonGeneratedClass )
		//			{
		//				Field = FindField<UField>(SkeletonClass, FieldName);
		//			}
		//		}
		//	}
		//}

		return Field;
	}

	return nullptr;
}

FName FEditorPropertyPathSegment::GetMemberName() const
{
	if ( MemberGuid.IsValid() )
	{
		FName NameFromGuid = NAME_None;

		if ( UClass* Class = Cast<UClass>(Struct) )
		{
			if ( UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy) )
			{
				if ( IsProperty )
				{
					NameFromGuid = UBlueprint::GetFieldNameFromClassByGuid<UProperty>(Class, MemberGuid);
				}
				else
				{
					NameFromGuid = UBlueprint::GetFieldNameFromClassByGuid<UFunction>(Class, MemberGuid);
				}
			}
		}
		else if ( UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Struct) )
		{
			if ( UProperty* Property = FStructureEditorUtils::GetPropertyByGuid(UserStruct, MemberGuid) )
			{
				NameFromGuid = Property->GetFName();
			}
		}

		//check(NameFromGuid != NAME_None);
		return NameFromGuid;
	}
	
	//check(MemberName != NAME_None);
	return MemberName;
}

FText FEditorPropertyPathSegment::GetMemberDisplayText() const
{
	if ( MemberGuid.IsValid() )
	{
		if ( UClass* Class = Cast<UClass>(Struct) )
		{
			if ( UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy) )
			{
				if ( IsProperty )
				{
					return FText::FromName(UBlueprint::GetFieldNameFromClassByGuid<UProperty>(Class, MemberGuid));
				}
				else
				{
					return FText::FromName(UBlueprint::GetFieldNameFromClassByGuid<UFunction>(Class, MemberGuid));
				}
			}
		}
		else if ( UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Struct) )
		{
			if ( UProperty* Property = FStructureEditorUtils::GetPropertyByGuid(UserStruct, MemberGuid) )
			{
				return Property->GetDisplayNameText();
			}
		}
	}

	return FText::FromName(MemberName);
}

FGuid FEditorPropertyPathSegment::GetMemberGuid() const
{
	return MemberGuid;
}

FEditorPropertyPath::FEditorPropertyPath()
{
}

FEditorPropertyPath::FEditorPropertyPath(const TArray<UField*>& BindingChain)
{
	for ( const UField* Field : BindingChain )
	{
		if ( const UProperty* Property = Cast<UProperty>(Field) )
		{
			Segments.Add(FEditorPropertyPathSegment(Property));
		}
		else if ( const UFunction* Function = Cast<UFunction>(Field) )
		{
			Segments.Add(FEditorPropertyPathSegment(Function));
		}
		else
		{
			// Should never happen
			check(false);
		}
	}
}

bool FEditorPropertyPath::Rebase(UBlueprint* SegmentBase)
{
	if ( !IsEmpty() )
	{
		Segments[0].Rebase(SegmentBase);
		return true;
	}

	return false;
}

bool FEditorPropertyPath::Validate(UDelegateProperty* Destination, FText& OutError) const
{
	if ( IsEmpty() )
	{
		OutError = LOCTEXT("Binding_Empty", "The binding is empty.");
		return false;
	}

	for ( int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); SegmentIndex++ )
	{
		const FEditorPropertyPathSegment& Segment = Segments[SegmentIndex];
		if ( UStruct* OwnerStruct = Segment.GetStruct() )
		{
			if ( Segment.GetMember() == nullptr )
			{
				OutError = FText::Format(LOCTEXT("Binding_MemberNotFound", "Binding: '{0}' : '{1}' was not found on '{2}'."),
					GetDisplayText(),
					Segment.GetMemberDisplayText(),
					OwnerStruct->GetDisplayNameText());

				return false;
			}
		}
		else
		{
			OutError = FText::Format(LOCTEXT("Binding_StructNotFound", "Binding: '{0}' : Unable to locate owner class or struct for '{1}'"),
				GetDisplayText(),
				Segment.GetMemberDisplayText());

			return false;
		}
	}

	// Validate the last member in the segment
	const FEditorPropertyPathSegment& LastSegment = Segments[Segments.Num() - 1];
	return LastSegment.ValidateMember(Destination, OutError);
}

FText FEditorPropertyPath::GetDisplayText() const
{
	FString DisplayText;

	for ( int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); SegmentIndex++ )
	{
		const FEditorPropertyPathSegment& Segment = Segments[SegmentIndex];
		DisplayText.Append(Segment.GetMemberDisplayText().ToString());
		if ( SegmentIndex < ( Segments.Num() - 1 ) )
		{
			DisplayText.Append(TEXT("."));
		}
	}

	return FText::FromString(DisplayText);
}

FDynamicPropertyPath FEditorPropertyPath::ToPropertyPath() const
{
	TArray<FString> PropertyChain;

	for ( const FEditorPropertyPathSegment& Segment : Segments )
	{
		FName SegmentName = Segment.GetMemberName();

		if ( SegmentName != NAME_None )
		{
			PropertyChain.Add(SegmentName.ToString());
		}
		else
		{
			return FDynamicPropertyPath();
		}
	}

	return FDynamicPropertyPath(PropertyChain);
}

bool FDelegateEditorBinding::IsBindingValid(UClass* BlueprintGeneratedClass, UWidgetBlueprint* Blueprint, FCompilerResultsLog& MessageLog) const
{
	FDelegateRuntimeBinding RuntimeBinding = ToRuntimeBinding(Blueprint);

	// First find the target widget we'll be attaching the binding to.
	if ( UWidget* TargetWidget = Blueprint->WidgetTree->FindWidget(FName(*ObjectName)) )
	{
		// Next find the underlying delegate we're actually binding to, if it's an event the name will be the same,
		// for properties we need to lookup the delegate property we're actually going to be binding to.
		UDelegateProperty* BindableProperty = FindField<UDelegateProperty>(TargetWidget->GetClass(), FName(*( PropertyName.ToString() + TEXT("Delegate") )));
		UDelegateProperty* EventProperty = FindField<UDelegateProperty>(TargetWidget->GetClass(), PropertyName);

		bool bNeedsToBePure = BindableProperty ? true : false;
		UDelegateProperty* DelegateProperty = BindableProperty ? BindableProperty : EventProperty;

		// Locate the delegate property on the widget that's a delegate for a property we want to bind.
		if ( DelegateProperty )
		{
			if ( !SourcePath.IsEmpty() )
			{
				FText ValidationError;
				if ( SourcePath.Validate(DelegateProperty, ValidationError) == false )
				{
					FText const ErrorFormat = LOCTEXT("BindingError", "Binding: Property '@@' on Widget '@@': %s");
					MessageLog.Error(*FString::Printf(*ErrorFormat.ToString(), *ValidationError.ToString()), DelegateProperty, TargetWidget);

					return false;
				}

				return true;
			}
			else
			{
				// On our incoming blueprint generated class, try and find the function we claim exists that users
				// are binding their property too.
				if ( UFunction* Function = BlueprintGeneratedClass->FindFunctionByName(RuntimeBinding.FunctionName, EIncludeSuperFlag::IncludeSuper) )
				{
					// Check the signatures to ensure these functions match.
					if ( Function->IsSignatureCompatibleWith(DelegateProperty->SignatureFunction, UFunction::GetDefaultIgnoredSignatureCompatibilityFlags() | CPF_ReturnParm) )
					{
						// Only allow binding pure functions to property bindings.
						if ( bNeedsToBePure && !Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure) )
						{
							FText const ErrorFormat = LOCTEXT("BindingNotBoundToPure", "Binding: property '@@' on widget '@@' needs to be bound to a pure function, '@@' is not pure.");
							MessageLog.Error(*ErrorFormat.ToString(), DelegateProperty, TargetWidget, Function);

							return false;
						}

						return true;
					}
					else
					{
						FText const ErrorFormat = LOCTEXT("BindingFunctionSigDontMatch", "Binding: property '@@' on widget '@@' bound to function '@@', but the sigatnures don't match.  The function must return the same type as the property and have no parameters.");
						MessageLog.Error(*ErrorFormat.ToString(), DelegateProperty, TargetWidget, Function);
					}
				}
				else
				{
					//TODO Bindable property removed.
				}
			}
		}
		else
		{
			// TODO Bindable Property Removed
		}
	}
	else
	{
		//TODO Ignore missing widgets
	}

	return false;
}

FDelegateRuntimeBinding FDelegateEditorBinding::ToRuntimeBinding(UWidgetBlueprint* Blueprint) const
{
	FDelegateRuntimeBinding Binding;
	Binding.ObjectName = ObjectName;
	Binding.PropertyName = PropertyName;
	if ( Kind == EBindingKind::Function )
	{
		Binding.FunctionName = ( MemberGuid.IsValid() ) ? Blueprint->GetFieldNameFromClassByGuid<UFunction>(Blueprint->SkeletonGeneratedClass, MemberGuid) : FunctionName;
	}
	else
	{
		Binding.FunctionName = FunctionName;
	}
	Binding.Kind = Kind;
	Binding.SourcePath = SourcePath.ToPropertyPath();

	return Binding;
}

bool FWidgetAnimation_DEPRECATED::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar)
{
	static FName AnimationDataName("AnimationData");
	if(Tag.Type == NAME_StructProperty && Tag.Name == AnimationDataName)
	{
		Ar << MovieScene;
		Ar << AnimationBindings;
		return true;
	}

	return false;
}
/////////////////////////////////////////////////////
// UWidgetBlueprint

UWidgetBlueprint::UWidgetBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WidgetTree = CreateDefaultSubobject<UWidgetTree>(TEXT("WidgetTree"));
	WidgetTree->SetFlags(RF_Transactional);
}

void UWidgetBlueprint::PostLoad()
{
	Super::PostLoad();

	WidgetTree->ForEachWidget([&] (UWidget* Widget) {
		Widget->ConnectEditorData();
	});

	if( GetLinkerUE4Version() < VER_UE4_FIXUP_WIDGET_ANIMATION_CLASS )
	{
		// Fixup widget animiations.
		for( auto& OldAnim : AnimationData_DEPRECATED )
		{
			FName AnimName = OldAnim.MovieScene->GetFName();

			// Rename the old movie scene so we can reuse the name
			OldAnim.MovieScene->Rename( *MakeUniqueObjectName( this, UMovieScene::StaticClass(), "MovieScene").ToString(), nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);

			UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(this, AnimName, RF_Transactional);

			OldAnim.MovieScene->Rename(*AnimName.ToString(), NewAnimation, REN_ForceNoResetLoaders | REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional );

			NewAnimation->MovieScene = OldAnim.MovieScene;
			NewAnimation->AnimationBindings = OldAnim.AnimationBindings;
			
			Animations.Add( NewAnimation );
		}	

		AnimationData_DEPRECATED.Empty();
	}

	if ( GetLinkerUE4Version() < VER_UE4_RENAME_WIDGET_VISIBILITY )
	{
		static const FName Visiblity(TEXT("Visiblity"));
		static const FName Visibility(TEXT("Visibility"));

		for ( FDelegateEditorBinding& Binding : Bindings )
		{
			if ( Binding.PropertyName == Visiblity )
			{
				Binding.PropertyName = Visibility;
			}
		}
	}
}

void UWidgetBlueprint::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if ( !bDuplicatingReadOnly )
	{
		// We need to update all the bindings and change each bindings first segment in the path
		// to be the new class this blueprint generates, as all bindings must first originate on 
		// the widget blueprint, the first segment is always a reference to 'self'.
		for ( FDelegateEditorBinding& Binding : Bindings )
		{
			Binding.SourcePath.Rebase(this);
		}
	}
}

UClass* UWidgetBlueprint::GetBlueprintClass() const
{
	return UWidgetBlueprintGeneratedClass::StaticClass();
}

bool UWidgetBlueprint::AllowsDynamicBinding() const
{
	return true;
}

void UWidgetBlueprint::GatherDependencies(TSet<TWeakObjectPtr<UBlueprint>>& InDependencies) const
{
	Super::GatherDependencies(InDependencies);

	if ( WidgetTree )
	{
		WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			if ( UBlueprint* WidgetBlueprint = UBlueprint::GetBlueprintFromClass(Widget->GetClass()) )
			{
				bool bWasAlreadyInSet;
				InDependencies.Add(WidgetBlueprint, &bWasAlreadyInSet);

				if ( !bWasAlreadyInSet )
				{
					WidgetBlueprint->GatherDependencies(InDependencies);
				}
			}
		});
	}
}

bool UWidgetBlueprint::ValidateGeneratedClass(const UClass* InClass)
{
	bool Result = Super::ValidateGeneratedClass(InClass);

	const UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<const UWidgetBlueprintGeneratedClass>(InClass);
	if ( !ensure(GeneratedClass) )
	{
		return false;
	}
	const UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(GetBlueprintFromClass(GeneratedClass));
	if ( !ensure(Blueprint) )
	{
		return false;
	}

	if ( !ensure(Blueprint->WidgetTree && ( Blueprint->WidgetTree->GetOuter() == Blueprint )) )
	{
		return false;
	}
	else
	{
		TArray < UWidget* > AllWidgets;
		Blueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for ( UWidget* Widget : AllWidgets )
		{
			if ( !ensure(Widget->GetOuter() == Blueprint->WidgetTree) )
			{
				return false;
			}
		}
	}

	if ( !ensure(GeneratedClass->WidgetTree && ( GeneratedClass->WidgetTree->GetOuter() == GeneratedClass )) )
	{
		return false;
	}
	else
	{
		TArray < UWidget* > AllWidgets;
		GeneratedClass->WidgetTree->GetAllWidgets(AllWidgets);
		for ( UWidget* Widget : AllWidgets )
		{
			if ( !ensure(Widget->GetOuter() == GeneratedClass->WidgetTree) )
			{
				return false;
			}
		}
	}

	return Result;
}

void UWidgetBlueprint::GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const
{
	AllowedChildrenOfClasses.Add( UUserWidget::StaticClass() );
}

bool UWidgetBlueprint::NeedsLoadForClient() const
{
	return false;
}

#undef LOCTEXT_NAMESPACE 