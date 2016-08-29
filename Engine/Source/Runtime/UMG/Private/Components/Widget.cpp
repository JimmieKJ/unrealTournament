// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "ReflectionMetadata.h"
#include "SObjectWidget.h"

#define LOCTEXT_NAMESPACE "UMG"

/**
* Interface for tool tips.
*/
class FDelegateToolTip : public IToolTip
{
public:

	/**
	* Gets the widget that this tool tip represents.
	*
	* @return The tool tip widget.
	*/
	virtual TSharedRef<class SWidget> AsWidget() override
	{
		return GetContentWidget();
	}

	/**
	* Gets the tool tip's content widget.
	*
	* @return The content widget.
	*/
	virtual TSharedRef<SWidget> GetContentWidget() override
	{
		if ( CachedToolTip.IsValid() )
		{
			return CachedToolTip.ToSharedRef();
		}

		UWidget* Widget = ToolTipWidgetDelegate.Execute();
		if ( Widget )
		{
			CachedToolTip = Widget->TakeWidget();
			return CachedToolTip.ToSharedRef();
		}

		return SNullWidget::NullWidget;
	}

	/**
	* Sets the tool tip's content widget.
	*
	* @param InContentWidget The new content widget to set.
	*/
	virtual void SetContentWidget(const TSharedRef<SWidget>& InContentWidget) override
	{
		CachedToolTip = InContentWidget;
	}

	/**
	* Checks whether this tool tip has no content to display right now.
	*
	* @return true if the tool tip has no content to display, false otherwise.
	*/
	virtual bool IsEmpty() const override
	{
		return !ToolTipWidgetDelegate.IsBound();
	}

	/**
	* Checks whether this tool tip can be made interactive by the user (by holding Ctrl).
	*
	* @return true if it is an interactive tool tip, false otherwise.
	*/
	virtual bool IsInteractive() const override
	{
		return false;
	}

	virtual void OnClosed() override
	{
		//TODO Notify interface implementing widget of closure

		CachedToolTip.Reset();
	}

	virtual void OnOpening() override
	{
		//TODO Notify interface implementing widget of opening
	}

public:
	UWidget::FGetWidget ToolTipWidgetDelegate;

private:
	TSharedPtr<SWidget> CachedToolTip;
};


/////////////////////////////////////////////////////
// UWidget

TArray<TSubclassOf<UPropertyBinding>> UWidget::BinderClasses;

UWidget::UWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsEnabled = true;
	bIsVariable = true;
#if WITH_EDITOR
	DesignerFlags = EWidgetDesignFlags::None;
#endif
	Visiblity_DEPRECATED = Visibility = ESlateVisibility::Visible;	
	RenderTransformPivot = FVector2D(0.5f, 0.5f);
	Cursor = EMouseCursor::Default;
}

void UWidget::SetRenderTransform(FWidgetTransform Transform)
{
	RenderTransform = Transform;
	UpdateRenderTransform();
}

void UWidget::SetRenderScale(FVector2D Scale)
{
	RenderTransform.Scale = Scale;
	UpdateRenderTransform();
}

void UWidget::SetRenderShear(FVector2D Shear)
{
	RenderTransform.Shear = Shear;
	UpdateRenderTransform();
}

void UWidget::SetRenderAngle(float Angle)
{
	RenderTransform.Angle = Angle;
	UpdateRenderTransform();
}

void UWidget::SetRenderTranslation(FVector2D Translation)
{
	RenderTransform.Translation = Translation;
	UpdateRenderTransform();
}

void UWidget::UpdateRenderTransform()
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		if (RenderTransform.IsIdentity())
		{
			SafeWidget->SetRenderTransform(TOptional<FSlateRenderTransform>());
		}
		else
		{
			SafeWidget->SetRenderTransform(RenderTransform.ToSlateRenderTransform());
		}
	}
}

void UWidget::SetRenderTransformPivot(FVector2D Pivot)
{
	RenderTransformPivot = Pivot;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SafeWidget->SetRenderTransformPivot(Pivot);
	}
}

bool UWidget::GetIsEnabled() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	return SafeWidget.IsValid() ? SafeWidget->IsEnabled() : bIsEnabled;
}

void UWidget::SetIsEnabled(bool bInIsEnabled)
{
	bIsEnabled = bInIsEnabled;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SafeWidget->SetEnabled(bInIsEnabled);
	}
}

void UWidget::SetCursor(EMouseCursor::Type InCursor)
{
	bOverride_Cursor = true;
	Cursor = InCursor;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		SafeWidget->SetCursor(Cursor);
	}
}

void UWidget::ResetCursor()
{
	bOverride_Cursor = false;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		SafeWidget->SetCursor(TOptional<EMouseCursor::Type>());
	}
}

bool UWidget::IsVisible() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		return SafeWidget->GetVisibility().IsVisible();
	}

	return false;
}

ESlateVisibility UWidget::GetVisibility() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return UWidget::ConvertRuntimeToSerializedVisibility(SafeWidget->GetVisibility());
	}

	return Visibility;
}

void UWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Visibility = InVisibility;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		EVisibility SlateVisibility = UWidget::ConvertSerializedVisibilityToRuntime(InVisibility);
		return SafeWidget->SetVisibility(SlateVisibility);
	}
}

void UWidget::ForceVolatile(bool bForce)
{
	bIsVolatile = bForce;
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		SafeWidget->ForceVolatile(bForce);
	}
}

void UWidget::SetToolTipText(const FText& InToolTipText)
{
	ToolTipText = InToolTipText;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->SetToolTipText(InToolTipText);
	}
}

void UWidget::SetToolTip(UWidget* InToolTipWidget)
{
	ToolTipWidget = InToolTipWidget;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		if ( ToolTipWidget )
		{
			TSharedRef<SToolTip> ToolTip = SNew(SToolTip)
				.TextMargin(FMargin(0))
				.BorderImage(nullptr)
				[
					ToolTipWidget->TakeWidget()
				];

			SafeWidget->SetToolTip(ToolTip);
		}
		else
		{
			SafeWidget->SetToolTip(TSharedPtr<IToolTip>());
		}
	}
}

bool UWidget::IsHovered() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->IsHovered();
	}

	return false;
}

bool UWidget::HasKeyboardFocus() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->HasKeyboardFocus();
	}

	return false;
}

bool UWidget::HasMouseCapture() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->HasMouseCapture();
	}

	return false;
}

void UWidget::SetKeyboardFocus()
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(SafeWidget);
	}
}

bool UWidget::HasUserFocus(APlayerController* PlayerController) const
{
	if ( PlayerController == nullptr || !PlayerController->IsLocalPlayerController() )
	{
		return false;
	}

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		FLocalPlayerContext Context(PlayerController);

		if ( ULocalPlayer* LocalPlayer = Context.GetLocalPlayer() )
		{
			// HACK: We use the controller Id as the local player index for focusing widgets in Slate.
			int32 UserIndex = LocalPlayer->GetControllerId();

			TOptional<EFocusCause> FocusCause = SafeWidget->HasUserFocus(UserIndex);
			return FocusCause.IsSet();
		}
	}

	return false;
}

bool UWidget::HasAnyUserFocus() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		TOptional<EFocusCause> FocusCause = SafeWidget->HasAnyUserFocus();
		return FocusCause.IsSet();
	}

	return false;
}

bool UWidget::HasFocusedDescendants() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		return SafeWidget->HasFocusedDescendants();
	}

	return false;
}

bool UWidget::HasUserFocusedDescendants(APlayerController* PlayerController) const
{
	if ( PlayerController == nullptr || !PlayerController->IsLocalPlayerController() )
	{
		return false;
	}

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		FLocalPlayerContext Context(PlayerController);

		if ( ULocalPlayer* LocalPlayer = Context.GetLocalPlayer() )
		{
			// HACK: We use the controller Id as the local player index for focusing widgets in Slate.
			int32 UserIndex = LocalPlayer->GetControllerId();

			return SafeWidget->HasUserFocusedDescendants(UserIndex);
		}
	}

	return false;
}

void UWidget::SetUserFocus(APlayerController* PlayerController)
{
	if ( PlayerController == nullptr || !PlayerController->IsLocalPlayerController() || PlayerController->Player == nullptr )
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FMessageLog("PIE").Error(LOCTEXT("NoPlayerControllerToFocus", "The PlayerController is not a valid local player so it can't focus the widget."));
#endif
		return;
	}

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( !SafeWidget->SupportsKeyboardFocus() )
		{
			FMessageLog("PIE").Warning(LOCTEXT("ThisWidgetDoesntSupportFocus", "This widget does not support focus.  If this is a UserWidget, you should set bIsFocusable to true."));
		}
#endif

		FLocalPlayerContext Context(PlayerController);

		if ( ULocalPlayer* LocalPlayer = Context.GetLocalPlayer() )
		{
			// HACK: We use the controller Id as the local player index for focusing widgets in Slate.
			int32 UserIndex = LocalPlayer->GetControllerId();

			FSlateApplication::Get().SetUserFocus(UserIndex, SafeWidget);
		}
	}
}

void UWidget::ForceLayoutPrepass()
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SafeWidget->SlatePrepass();
	}
}

void UWidget::InvalidateLayoutAndVolatility()
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		SafeWidget->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

FVector2D UWidget::GetDesiredSize() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->GetDesiredSize();
	}

	return FVector2D(0, 0);
}

UPanelWidget* UWidget::GetParent() const
{
	if ( Slot )
	{
		return Slot->Parent;
	}

	return nullptr;
}

void UWidget::RemoveFromParent()
{
	UPanelWidget* CurrentParent = GetParent();
	if ( CurrentParent )
	{
		CurrentParent->RemoveChild(this);
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UWidget::VerifySynchronizeProperties()
{
	ensureMsgf(bRoutedSynchronizeProperties, TEXT("%s failed to route SynchronizeProperties.  Please call Super::SynchronizeProperties() in your <className>::SynchronizeProperties() function."), *GetFullName());
}
#endif

void UWidget::OnWidgetRebuilt()
{
}

TSharedRef<SWidget> UWidget::TakeWidget()
{
	return TakeWidget_Private( []( UUserWidget* Widget, TSharedRef<SWidget> Content ) -> TSharedPtr<SObjectWidget> {
		       return SNew( SObjectWidget, Widget )[ Content ];
		   } );
}

TSharedRef<SWidget> UWidget::TakeWidget_Private( ConstructMethodType ConstructMethod )
{
	TSharedPtr<SWidget> SafeWidget;
	bool bNewlyCreated = false;

	// If the underlying widget doesn't exist we need to construct and cache the widget for the first run.
	if ( !MyWidget.IsValid() )
	{
		SafeWidget = RebuildWidget();
		MyWidget = SafeWidget;

		bNewlyCreated = true;
	}
	else
	{
		SafeWidget = MyWidget.Pin();
	}

	// If it is a user widget wrap it in a SObjectWidget to keep the instance from being GC'ed
	if ( IsA( UUserWidget::StaticClass() ) )
	{
		TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();

		// If the GC Widget is still valid we still exist in the slate hierarchy, so just return the GC Widget.
		if ( SafeGCWidget.IsValid() )
		{
			return SafeGCWidget.ToSharedRef();
		}
		else // Otherwise we need to recreate the wrapper widget
		{
			SafeGCWidget = ConstructMethod( Cast<UUserWidget>( this ), SafeWidget.ToSharedRef() );

			MyGCWidget = SafeGCWidget;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			bRoutedSynchronizeProperties = false;
#endif

			// Always synchronize properties of a user widget and call construct AFTER we've
			// properly setup the GCWidget and synced all the properties.
			SynchronizeProperties();
			VerifySynchronizeProperties();
			OnWidgetRebuilt();

			return SafeGCWidget.ToSharedRef();
		}
	}
	else
	{
		if ( bNewlyCreated )
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			bRoutedSynchronizeProperties = false;
#endif

			SynchronizeProperties();
			VerifySynchronizeProperties();
			OnWidgetRebuilt();
		}

		return SafeWidget.ToSharedRef();
	}
}

TSharedPtr<SWidget> UWidget::GetCachedWidget() const
{
	if ( MyGCWidget.IsValid() )
	{
		return MyGCWidget.Pin();
	}

	return MyWidget.Pin();
}

TSharedRef<SWidget> UWidget::BuildDesignTimeWidget(TSharedRef<SWidget> WrapWidget)
{
#if WITH_EDITOR
	if (IsDesignTime())
	{
		return SNew(SOverlay)
		
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			WrapWidget
		]
		
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.Visibility(HasAnyDesignerFlags(EWidgetDesignFlags::ShowOutline) ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			.BorderImage(FUMGStyle::Get().GetBrush("MarchingAnts"))
		];
	}
	else
	{
		return WrapWidget;
	}
#else
	return WrapWidget;
#endif
}

#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "UMGEditor"

void UWidget::SetDesignerFlags(EWidgetDesignFlags::Type NewFlags)
{
	DesignerFlags = ( EWidgetDesignFlags::Type )( DesignerFlags | NewFlags );
}

void UWidget::SetDisplayLabel(const FString& InDisplayLabel)
{
	DisplayLabel = InDisplayLabel;
}

bool UWidget::IsGeneratedName() const
{
	if (!DisplayLabel.IsEmpty())
	{
		return false;
	}

	FString Name = GetName();

	if (Name == GetClass()->GetName() || Name.StartsWith(GetClass()->GetName() + TEXT("_")))
	{
		return true;
	}
	else if (GetClass()->ClassGeneratedBy != nullptr)
	{
		FString BaseNameForBP = GetClass()->GetName();
		BaseNameForBP.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);

		if (Name == BaseNameForBP || Name.StartsWith(BaseNameForBP + TEXT("_")))
		{
			return true;
		}
	}

	return false;
}

FString UWidget::GetLabelMetadata() const
{
	return TEXT("");
}

FString UWidget::GetLabel() const
{
	return GetLabelText().ToString();
}

FText UWidget::GetLabelText() const
{
	FText Label = GetDisplayNameBase();

	if (IsGeneratedName() && !bIsVariable)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("BaseName"), Label);
		Args.Add(TEXT("Metadata"), FText::FromString(GetLabelMetadata()));
		Label = FText::Format(LOCTEXT("NonVariableLabelFormat", "[{BaseName}]{Metadata}"), Args);
	}

	return Label;
}

FText UWidget::GetDisplayNameBase() const
{
	const bool bHasDisplayLabel = !DisplayLabel.IsEmpty();
	if (IsGeneratedName())
	{
		return GetClass()->GetDisplayNameText();
	}
	else
	{
		return FText::FromString(bHasDisplayLabel ? DisplayLabel : GetName());
	}
}

const FText UWidget::GetPaletteCategory()
{
	return LOCTEXT("Uncategorized", "Uncategorized");
}

const FSlateBrush* UWidget::GetEditorIcon()
{
	return nullptr;
}

EVisibility UWidget::GetVisibilityInDesigner() const
{
	return bHiddenInDesigner ? EVisibility::Collapsed : EVisibility::Visible;
}

void UWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SynchronizeProperties();
	}
}

void UWidget::Select()
{
	OnSelected();

	UWidget* Parent = GetParent();
	while ( Parent != nullptr )
	{
		Parent->OnDescendantSelected(this);
		Parent = Parent->GetParent();
	}
}

void UWidget::Deselect()
{
	OnDeselected();

	UWidget* Parent = GetParent();
	while ( Parent != nullptr )
	{
		Parent->OnDescendantDeselected(this);
		Parent = Parent->GetParent();
	}
}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "UMG"
#endif

bool UWidget::Modify(bool bAlwaysMarkDirty)
{
	bool Modified = Super::Modify(bAlwaysMarkDirty);

	if ( Slot )
	{
		Modified &= Slot->Modify(bAlwaysMarkDirty);
	}

	return Modified;
}

bool UWidget::IsChildOf(UWidget* PossibleParent)
{
	UPanelWidget* Parent = GetParent();
	if ( Parent == nullptr )
	{
		return false;
	}
	else if ( Parent == PossibleParent )
	{
		return true;
	}
	
	return Parent->IsChildOf(PossibleParent);
}

TSharedRef<SWidget> UWidget::RebuildWidget()
{
	ensureMsgf(false, TEXT("You must implement RebuildWidget() in your child class"));
	return SNew(SSpacer);
}

void UWidget::SynchronizeProperties()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bRoutedSynchronizeProperties = true;
#endif

	// We want to apply the bindings to the cached widget, which could be the SWidget, or the SObjectWidget, 
	// in the case where it's a user widget.  We always want to prefer the SObjectWidget so that bindings to 
	// visibility and enabled status are not stomping values setup in the root widget in the User Widget.
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( !SafeWidget.IsValid() )
	{
		return;
	}

#if WITH_EDITOR
	// Always use an enabled and visible state in the designer.
	if ( IsDesignTime() )
	{
		SafeWidget->SetEnabled(true);
		SafeWidget->SetVisibility(BIND_UOBJECT_ATTRIBUTE(EVisibility, GetVisibilityInDesigner));
	}
	else 
#endif
	{
		if ( bOverride_Cursor /*|| CursorDelegate.IsBound()*/ )
		{
			SafeWidget->SetCursor(Cursor);// GAME_SAFE_OPTIONAL_BINDING(EMouseCursor::Type, Cursor));
		}

		SafeWidget->SetEnabled(GAME_SAFE_OPTIONAL_BINDING( bool, bIsEnabled ));
		SafeWidget->SetVisibility(OPTIONAL_BINDING_CONVERT(ESlateVisibility, Visibility, EVisibility, ConvertVisibility));
	}

	SafeWidget->ForceVolatile(bIsVolatile);

	UpdateRenderTransform();
	SafeWidget->SetRenderTransformPivot(RenderTransformPivot);

	if ( ToolTipWidgetDelegate.IsBound() && !IsDesignTime() )
	{
		TSharedRef<FDelegateToolTip> ToolTip = MakeShareable(new FDelegateToolTip());
		ToolTip->ToolTipWidgetDelegate = ToolTipWidgetDelegate;
		SafeWidget->SetToolTip(ToolTip);
	}
	else if ( ToolTipWidget != nullptr )
	{
		TSharedRef<SToolTip> ToolTip = SNew(SToolTip)
			.TextMargin(FMargin(0))
			.BorderImage(nullptr)
			[
				ToolTipWidget->TakeWidget()
			];

		SafeWidget->SetToolTip(ToolTip);
	}
	else if ( !ToolTipText.IsEmpty() || ToolTipTextDelegate.IsBound() )
	{
		SafeWidget->SetToolTipText(GAME_SAFE_OPTIONAL_BINDING(FText, ToolTipText));
	}

#if WITH_EDITORONLY_DATA
	// In editor builds we add metadata to the widget so that once hit with the widget reflector it can report
	// where it comes from, what blueprint, what the name of the widget was...etc.
	SafeWidget->AddMetadata<FReflectionMetaData>(MakeShareable(new FReflectionMetaData(GetFName(), GetClass(), WidgetGeneratedBy.Get())));
#else

#if !UE_BUILD_SHIPPING
	SafeWidget->AddMetadata<FReflectionMetaData>(MakeShareable(new FReflectionMetaData(GetFName(), GetClass(), WidgetGeneratedByClass.Get())));
#endif

#endif
}

void UWidget::BuildNavigation()
{
	if ( Navigation != nullptr )
	{
		TSharedPtr<SWidget> SafeWidget = GetCachedWidget();

		if ( ensure(SafeWidget.IsValid()) )
		{
			TSharedPtr<FNavigationMetaData> MetaData = SafeWidget->GetMetaData<FNavigationMetaData>();
			if ( !MetaData.IsValid() )
			{
				MetaData = MakeShareable(new FNavigationMetaData());
				SafeWidget->AddMetadata(MetaData.ToSharedRef());
			}

			Navigation->UpdateMetaData(MetaData.ToSharedRef());
		}
	}
}

UWorld* UWidget::GetWorld() const
{
	// UWidget's are given world scope by their owning user widget.  We can get that through the widget tree that should
	// be the outer of this widget.
	if ( UWidgetTree* OwningTree = Cast<UWidgetTree>(GetOuter()) )
	{
		return OwningTree->GetWorld();
	}

	return nullptr;
}

void UWidget::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_RENAME_WIDGET_VISIBILITY )
	{
		Visibility = Visiblity_DEPRECATED;
	}
}

EVisibility UWidget::ConvertSerializedVisibilityToRuntime(ESlateVisibility Input)
{
	switch ( Input )
	{
	case ESlateVisibility::Visible:
		return EVisibility::Visible;
	case ESlateVisibility::Collapsed:
		return EVisibility::Collapsed;
	case ESlateVisibility::Hidden:
		return EVisibility::Hidden;
	case ESlateVisibility::HitTestInvisible:
		return EVisibility::HitTestInvisible;
	case ESlateVisibility::SelfHitTestInvisible:
		return EVisibility::SelfHitTestInvisible;
	default:
		check(false);
		return EVisibility::Visible;
	}
}

ESlateVisibility UWidget::ConvertRuntimeToSerializedVisibility(const EVisibility& Input)
{
	if ( Input == EVisibility::Visible )
	{
		return ESlateVisibility::Visible;
	}
	else if ( Input == EVisibility::Collapsed )
	{
		return ESlateVisibility::Collapsed;
	}
	else if ( Input == EVisibility::Hidden )
	{
		return ESlateVisibility::Hidden;
	}
	else if ( Input == EVisibility::HitTestInvisible )
	{
		return ESlateVisibility::HitTestInvisible;
	}
	else if ( Input == EVisibility::SelfHitTestInvisible )
	{
		return ESlateVisibility::SelfHitTestInvisible;
	}
	else
	{
		check(false);
		return ESlateVisibility::Visible;
	}
}

FSizeParam UWidget::ConvertSerializedSizeParamToRuntime(const FSlateChildSize& Input)
{
	switch ( Input.SizeRule )
	{
	default:
	case ESlateSizeRule::Automatic:
		return FAuto();
	case ESlateSizeRule::Fill:
		return FStretch(Input.Value);
	}

	return FAuto();
}

UWidget* UWidget::FindChildContainingDescendant(UWidget* Root, UWidget* Descendant)
{
	if ( Root == nullptr )
	{
		return nullptr;
	}

	UWidget* Parent = Descendant->GetParent();

	while ( Parent != nullptr )
	{
		// If the Descendant's parent is the root, then the child containing the descendant is the descendant.
		if ( Parent == Root )
		{
			return Descendant;
		}

		Descendant = Parent;
		Parent = Parent->GetParent();
	}

	return nullptr;
}

//bool UWidget::BindProperty(const FName& DestinationProperty, UObject* SourceObject, const FName& SourceProperty)
//{
//	UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>(GetClass(), FName(*( DestinationProperty.ToString() + TEXT("Delegate") )));
//
//	if ( DelegateProperty )
//	{
//		FDynamicPropertyPath BindingPath(SourceProperty.ToString());
//		return AddBinding(DelegateProperty, SourceObject, BindingPath);
//	}
//
//	return false;
//}

TSubclassOf<UPropertyBinding> UWidget::FindBinderClassForDestination(UProperty* Property)
{
	if ( BinderClasses.Num() == 0 )
	{
		for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
		{
			if ( ClassIt->IsChildOf(UPropertyBinding::StaticClass()) )
			{
				BinderClasses.Add(*ClassIt);
			}
		}
	}

	for ( int32 ClassIndex = 0; ClassIndex < BinderClasses.Num(); ClassIndex++ )
	{
		if ( GetDefault<UPropertyBinding>(BinderClasses[ClassIndex])->IsSupportedDestination(Property))
		{
			return BinderClasses[ClassIndex];
		}
	}

	return nullptr;
}

static UPropertyBinding* GenerateBinder(UDelegateProperty* DelegateProperty, UObject* Container, UObject* SourceObject, const FDynamicPropertyPath& BindingPath)
{
	FScriptDelegate* ScriptDelegate = DelegateProperty->GetPropertyValuePtr_InContainer(Container);
	if ( ScriptDelegate )
	{
		// Only delegates that take no parameters have native binders.
		UFunction* SignatureFunction = DelegateProperty->SignatureFunction;
		if ( SignatureFunction->NumParms == 1 )
		{
			if ( UProperty* ReturnProperty = SignatureFunction->GetReturnProperty() )
			{
				TSubclassOf<UPropertyBinding> BinderClass = UWidget::FindBinderClassForDestination(ReturnProperty);
				if ( BinderClass != nullptr )
				{
					UPropertyBinding* Binder = NewObject<UPropertyBinding>(Container, BinderClass);
					Binder->SourceObject = SourceObject;
					Binder->SourcePath = BindingPath;
					Binder->Bind(ReturnProperty, ScriptDelegate);

					return Binder;
				}
			}
		}
	}

	return nullptr;
}

bool UWidget::AddBinding(UDelegateProperty* DelegateProperty, UObject* SourceObject, const FDynamicPropertyPath& BindingPath)
{
	if ( UPropertyBinding* Binder = GenerateBinder(DelegateProperty, this, SourceObject, BindingPath) )
	{
		// Remove any existing binding object for this property.
		for ( int32 BindingIndex = 0; BindingIndex < NativeBindings.Num(); BindingIndex++ )
		{
			if ( NativeBindings[BindingIndex]->DestinationProperty == DelegateProperty->GetFName() )
			{
				NativeBindings.RemoveAt(BindingIndex);
				break;
			}
		}

		NativeBindings.Add(Binder);

		// Only notify the bindings have changed if we've already create the underlying slate widget.
		if ( MyWidget.IsValid() )
		{
			OnBindingChanged(DelegateProperty->GetFName());
		}

		return true;
	}

	return false;
}

void UWidget::OnBindingChanged(const FName& Property)
{

}

#undef LOCTEXT_NAMESPACE
