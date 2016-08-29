// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "SkeletalMeshComponentDetails.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetData.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Engine/Selection.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshComponentDetails"

// Filter class for animation blueprint picker
class FAnimBlueprintFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		if(InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed)
		{
			return true;
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const class IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
	}

	/** Only children of the classes in this set will be unfiltered */
	TSet<const UClass*> AllowedChildrenOfClasses;
};

FSkeletalMeshComponentDetails::FSkeletalMeshComponentDetails()
	: CurrentDetailBuilder(NULL)
{

}

FSkeletalMeshComponentDetails::~FSkeletalMeshComponentDetails()
{
	UnregisterAllMeshPropertyChangedCallers();
}

TSharedRef<IDetailCustomization> FSkeletalMeshComponentDetails::MakeInstance()
{
	return MakeShareable(new FSkeletalMeshComponentDetails);
}

void FSkeletalMeshComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if(!CurrentDetailBuilder)
	{
		CurrentDetailBuilder = &DetailBuilder;
	}
	DetailBuilder.EditCategory("SkeletalMesh", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Materials", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory("Physics", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	DetailBuilder.HideProperty("bCastStaticShadow", UPrimitiveComponent::StaticClass());
	DetailBuilder.HideProperty("bLightAsIfStatic", UPrimitiveComponent::StaticClass());
	DetailBuilder.EditCategory("Animation", FText::GetEmpty(), ECategoryPriority::Important);

	OnSkeletalMeshPropertyChanged = USkeletalMeshComponent::FOnSkeletalMeshPropertyChanged::CreateSP(this, &FSkeletalMeshComponentDetails::SkeletalMeshPropertyChanged);

	UpdateAnimationCategory(DetailBuilder);
}

void FSkeletalMeshComponentDetails::UpdateAnimationCategory( IDetailLayoutBuilder& DetailBuilder )
{
	// Grab the objects we're editing so we can filter the skeleton when we need to change the animation.
	// Cache them for updates later if the mesh/skeleton changes
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);

	check(SelectedObjects.Num() > 0);
	
	// Check for incompatible (different) skeletons. This can happen when the user selects more than
	// one object in the viewport. If this happens we disable and hide the anim selection picker
	USkeleton* Skeleton = NULL;

	for(auto ObjectIter = SelectedObjects.CreateIterator() ; ObjectIter ; ++ObjectIter)
	{
		if(USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(ObjectIter->Get()))
		{
			RegisterSkeletalMeshPropertyChanged(Mesh);

			if(Mesh->SkeletalMesh)
			{
				if(!Skeleton)
				{
					Skeleton = Mesh->SkeletalMesh->Skeleton;
				}
				else if(Mesh->SkeletalMesh->Skeleton != Skeleton)
				{
					// There's an incompatible skeleton in the selection, stop checking
					AnimPickerVisibility = EVisibility::Hidden;
					break;
				}
			}
		}
	}

	if(!Skeleton)
	{
		// No valid skeleton in selection, treat as invalid
		SelectedSkeletonName = "";
		AnimPickerVisibility = EVisibility::Hidden;
	}

	if(AnimPickerVisibility == EVisibility::Visible)
	{
		// If we're showing the animation asset then we have a valid skeleton
		SelectedSkeletonName = FString::Printf(TEXT("%s'%s'"), *Skeleton->GetClass()->GetName(), *Skeleton->GetPathName());
	}

	IDetailCategoryBuilder& AnimationCategory = DetailBuilder.EditCategory("Animation", FText::GetEmpty(), ECategoryPriority::Important);

	// Force the mode switcher to be first
	AnimationModeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, AnimationMode));
	check (AnimationModeHandle->IsValidHandle());

	AnimationBlueprintHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, AnimClass));
	check(AnimationBlueprintHandle->IsValidHandle());

	AnimationCategory.AddProperty(AnimationModeHandle);

	// Place the blueprint property next (which may be hidden, depending on the mode)
	TAttribute<EVisibility> BlueprintVisibility( this, &FSkeletalMeshComponentDetails::VisibilityForBlueprintMode );

	DetailBuilder.HideProperty(AnimationBlueprintHandle);
	AnimationCategory.AddCustomRow(AnimationBlueprintHandle->GetPropertyDisplayName())
		.Visibility(BlueprintVisibility)
		.NameContent()
		[
			AnimationBlueprintHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(ClassPickerComboButton, SComboButton)
				.OnGetMenuContent(this, &FSkeletalMeshComponentDetails::GetClassPickerMenuContent)
				.ContentPadding(0)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(this, &FSkeletalMeshComponentDetails::GetSelectedAnimBlueprintName)
					.MinDesiredWidth(200.f)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 1.0f)
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FSkeletalMeshComponentDetails::OnBrowseToAnimBlueprint))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 1.0f)
			[
				PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FSkeletalMeshComponentDetails::UseSelectedAnimBlueprint))
			]
		];

	// Hide the parent AnimationData property, and inline the children with custom visibility delegates
	const FName AnimationDataFName(GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, AnimationData));

	TSharedPtr<IPropertyHandle> AnimationDataHandle = DetailBuilder.GetProperty(AnimationDataFName);
	check(AnimationDataHandle->IsValidHandle());
	TAttribute<EVisibility> SingleAnimVisibility(this, &FSkeletalMeshComponentDetails::VisibilityForSingleAnimMode);
	DetailBuilder.HideProperty(AnimationDataFName);

	// Process Animation asset selection
	uint32 TotalChildren=0;
	AnimationDataHandle->GetNumChildren(TotalChildren);
	for (uint32 ChildIndex=0; ChildIndex < TotalChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = AnimationDataHandle->GetChildHandle(ChildIndex);
	
		if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FSingleAnimationPlayData, AnimToPlay))
		{
			// Hide the property, as we're about to add it differently
			DetailBuilder.HideProperty(ChildHandle);

			// Add it differently
			TSharedPtr<SWidget> NameWidget = ChildHandle->CreatePropertyNameWidget();

			TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
				.PropertyHandle(ChildHandle)
				.AllowedClass(UAnimationAsset::StaticClass())
				.AllowClear(true)
				.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FSkeletalMeshComponentDetails::OnShouldFilterAnimAsset));

			TAttribute<EVisibility> AnimPickerVisibilityAttr(this, &FSkeletalMeshComponentDetails::VisibilityForAnimPicker);

			AnimationCategory.AddCustomRow(ChildHandle->GetPropertyDisplayName())
				.Visibility(AnimPickerVisibilityAttr)
				.NameContent()
				[
					NameWidget.ToSharedRef()
				]
				.ValueContent()
				[
					PropWidget
				];
		}
		else
		{
			AnimationCategory.AddProperty(ChildHandle).Visibility(SingleAnimVisibility);
		}
	}
}

EVisibility FSkeletalMeshComponentDetails::VisibilityForAnimationMode(EAnimationMode::Type AnimationMode) const
{
	uint8 AnimationModeValue=0;
	FPropertyAccess::Result Ret = AnimationModeHandle.Get()->GetValue(AnimationModeValue);
	if (Ret == FPropertyAccess::Result::Success)
	{
		return (AnimationModeValue == AnimationMode) ? EVisibility::Visible : EVisibility::Hidden;
	}

	return EVisibility::Hidden; //Hidden if we get fail or MultipleValues from the property
}

bool FSkeletalMeshComponentDetails::OnShouldFilterAnimAsset( const FAssetData& AssetData )
{
	const FString SkeletonName = AssetData.GetTagValueRef<FString>("Skeleton");
	return SkeletonName != SelectedSkeletonName;
}

void FSkeletalMeshComponentDetails::SkeletalMeshPropertyChanged()
{
	// Update the selected skeleton name and the picker visibility
	USkeleton* Skeleton = NULL;
	for(auto ObjectIter = SelectedObjects.CreateIterator() ; ObjectIter ; ++ObjectIter)
	{
		if(USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(ObjectIter->Get()))
		{
			if(!Mesh->SkeletalMesh)
			{
				// One of the selected meshes has no skeletal mesh, stop checking
				Skeleton = NULL;
				break;
			}
			else if(!Skeleton)
			{
				Skeleton = Mesh->SkeletalMesh->Skeleton;
			}
			else if(Mesh->SkeletalMesh->Skeleton != Skeleton)
			{
				// There's an incompatible skeleton in the selection, stop checking
				Skeleton = NULL;
				break;
			}
		}
	}

	if(Skeleton)
	{
		AnimPickerVisibility = EVisibility::Visible;
		SelectedSkeletonName = FString::Printf(TEXT("%s'%s'"), *Skeleton->GetClass()->GetName(), *Skeleton->GetPathName());
	}
	else
	{
		AnimPickerVisibility = EVisibility::Hidden;
		SelectedSkeletonName = "";
	}
}

void FSkeletalMeshComponentDetails::RegisterSkeletalMeshPropertyChanged(TWeakObjectPtr<USkeletalMeshComponent> Mesh)
{
	if(Mesh.IsValid() && OnSkeletalMeshPropertyChanged.IsBound())
	{
		OnSkeletalMeshPropertyChangedDelegateHandles.Add(Mesh.Get(), Mesh->RegisterOnSkeletalMeshPropertyChanged(OnSkeletalMeshPropertyChanged));
	}
}

void FSkeletalMeshComponentDetails::UnregisterSkeletalMeshPropertyChanged(TWeakObjectPtr<USkeletalMeshComponent> Mesh)
{
	if(Mesh.IsValid())
	{
		Mesh->UnregisterOnSkeletalMeshPropertyChanged(OnSkeletalMeshPropertyChangedDelegateHandles.FindRef(Mesh.Get()));
		OnSkeletalMeshPropertyChangedDelegateHandles.Remove(Mesh.Get());
	}
}

void FSkeletalMeshComponentDetails::UnregisterAllMeshPropertyChangedCallers()
{
	for(auto MeshIter = SelectedObjects.CreateIterator() ; MeshIter ; ++MeshIter)
	{
		if(USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(MeshIter->Get()))
		{
			Mesh->UnregisterOnSkeletalMeshPropertyChanged(OnSkeletalMeshPropertyChangedDelegateHandles.FindRef(Mesh));
			OnSkeletalMeshPropertyChangedDelegateHandles.Remove(Mesh);
		}
	}
}

EVisibility FSkeletalMeshComponentDetails::VisibilityForAnimPicker() const
{
	return (VisibilityForSingleAnimMode().IsVisible() && AnimPickerVisibility.IsVisible()) ? EVisibility::Visible : EVisibility::Hidden;
}

TSharedRef<SWidget> FSkeletalMeshComponentDetails::GetClassPickerMenuContent()
{
	TSharedPtr<FAnimBlueprintFilter> Filter = MakeShareable(new FAnimBlueprintFilter);
	Filter->AllowedChildrenOfClasses.Add(UAnimInstance::StaticClass());

	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	FClassViewerInitializationOptions InitOptions;
	InitOptions.Mode = EClassViewerMode::ClassPicker;
	InitOptions.ClassFilter = Filter;
	InitOptions.bShowNoneOption = true;

	return SNew(SBorder)
		.Padding(3)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		.ForegroundColor(FEditorStyle::GetColor("DefaultForeground"))
		[
			SNew(SBox)
			.WidthOverride(280)
			[
				ClassViewerModule.CreateClassViewer(InitOptions, FOnClassPicked::CreateSP(this, &FSkeletalMeshComponentDetails::OnClassPicked))
			]
		];
}

FText FSkeletalMeshComponentDetails::GetSelectedAnimBlueprintName() const
{
	check(AnimationBlueprintHandle->IsValidHandle());

	UObject* Object = NULL;
	AnimationBlueprintHandle->GetValue(Object);
	if(Object)
	{
		return FText::FromString(Object->GetName());
	}
	else
	{
		return LOCTEXT("None", "None");
	}
}

void FSkeletalMeshComponentDetails::OnClassPicked( UClass* PickedClass )
{
	check(AnimationBlueprintHandle->IsValidHandle());

	ClassPickerComboButton->SetIsOpen(false);

	if(PickedClass)
	{
		AnimationBlueprintHandle->SetValueFromFormattedString(PickedClass->GetName());
	}
	else
	{
		AnimationBlueprintHandle->SetValueFromFormattedString(FString(TEXT("None")));
	}
}

void FSkeletalMeshComponentDetails::OnBrowseToAnimBlueprint()
{
	check(AnimationBlueprintHandle->IsValidHandle());

	UObject* Object = NULL;
	AnimationBlueprintHandle->GetValue(Object);

	TArray<UObject*> Objects;
	Objects.Add(Object);
	GEditor->SyncBrowserToObjects(Objects);
}

void FSkeletalMeshComponentDetails::UseSelectedAnimBlueprint()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	USelection* AssetSelection = GEditor->GetSelectedObjects();
	if (AssetSelection && AssetSelection->Num() == 1)
	{
		UAnimBlueprint* AnimBlueprintToAssign = AssetSelection->GetTop<UAnimBlueprint>();
		if (AnimBlueprintToAssign)
		{
			if(USkeleton* AnimBlueprintSkeleton = AnimBlueprintToAssign->TargetSkeleton)
			{
				FString BlueprintSkeletonName = FString::Printf(TEXT("%s'%s'"), *AnimBlueprintSkeleton->GetClass()->GetName(), *AnimBlueprintSkeleton->GetPathName());
				if (BlueprintSkeletonName == SelectedSkeletonName)
				{
					OnClassPicked(AnimBlueprintToAssign->GetAnimBlueprintGeneratedClass());
				}
			}
		}
	}
}
#undef LOCTEXT_NAMESPACE
