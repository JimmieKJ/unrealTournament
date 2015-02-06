// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once




class SSocketChooserPopup : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam( FOnSocketChosen, FName );

	/** Info about one socket */
	struct FSocketInfo
	{
		FComponentSocketDescription Description;

		static TSharedRef<FSocketInfo> Make(FComponentSocketDescription Description)
		{
			return MakeShareable( new FSocketInfo(Description) );
		}

	protected:
		FSocketInfo(FComponentSocketDescription InDescription)
			: Description(InDescription)
		{}
	};

	/** The Component that contains the sockets we are choosing from */
	TWeakObjectPtr<USceneComponent> SceneComponent;

	/** StaticMesh that we want to pick a socket for. Will only be used if no SkeletalMesh */
	TWeakObjectPtr<UStaticMesh> StaticMesh;

	/** Array of shared pointers to socket infos */
	TArray< TSharedPtr<FSocketInfo> > Sockets;

	/** Delegate to call when OK button is pressed */
	FOnSocketChosen OnSocketChosen;

	/** The combo box */
	TSharedPtr< SListView< TSharedPtr<FSocketInfo> > > SocketListView;

	SLATE_BEGIN_ARGS( SSocketChooserPopup )
		: _SceneComponent(NULL)
		, _ProvideNoSocketOption(true)
		{}

		/** A component that contains sockets */
		SLATE_ARGUMENT( USceneComponent*, SceneComponent )

		/** Called when the text is chosen. */
		SLATE_EVENT( FOnSocketChosen, OnSocketChosen )

		/** Control if the 'none' socket is shown */
		SLATE_ARGUMENT(bool, ProvideNoSocketOption)
	SLATE_END_ARGS()

private:
	/** Cache a weak pointer to my window */
	TWeakPtr<SWindow> WidgetWindow;

public:
	/** Called to create a widget for each socket */
	TSharedRef<ITableRow> MakeItemWidget( TSharedPtr<FSocketInfo> SocketInfo, const TSharedRef<STableViewBase>& OwnerTable )
	{
		const FSlateBrush* Brush = FEditorStyle::GetBrush(TEXT("SocketIcon.None"));
		
		if (SocketInfo->Description.Type == EComponentSocketType::Socket)
		{
			Brush = FEditorStyle::GetBrush( TEXT("SocketIcon.Socket") );
		}
		else if (SocketInfo->Description.Type == EComponentSocketType::Bone) 
		{
			Brush = FEditorStyle::GetBrush(TEXT("SocketIcon.Bone"));
		}
		
		return	
			SNew( STableRow< TSharedPtr<FSocketInfo> >, OwnerTable )
			. Content()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2)
				[
					SNew(SImage)
					.Image(Brush)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(FText::FromName(SocketInfo->Description.Name))
				]
			];
	}

	/** Called when item is selected */
	void SelectedSocket(TSharedPtr<FSocketInfo> SocketInfo, ESelectInfo::Type /*SelectType*/)
	{
		const FName SocketName = SocketInfo->Description.Name;

		FSlateApplication::Get().DismissAllMenus();

		if(OnSocketChosen.IsBound())
		{
			OnSocketChosen.Execute(SocketName);
		}
	}

	void Construct( const FArguments& InArgs )
	{
		OnSocketChosen = InArgs._OnSocketChosen;
		SceneComponent = InArgs._SceneComponent;

		// Add in 'No Socket' selection if required
		if (InArgs._ProvideNoSocketOption)
		{
			Sockets.Add( SSocketChooserPopup::FSocketInfo::Make(FComponentSocketDescription(NAME_None, EComponentSocketType::Invalid)) );
		}

		// Build set of sockets
		if (SceneComponent != NULL)
		{
			TArray<FComponentSocketDescription> Descriptions;
			SceneComponent->QuerySupportedSockets(/*out*/ Descriptions);
			for (auto DescriptionIt = Descriptions.CreateConstIterator(); DescriptionIt; ++DescriptionIt)
			{
				Sockets.Add( SSocketChooserPopup::FSocketInfo::Make(*DescriptionIt) );
			}
		}

		// Then make widget
		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
			.Padding(5)
			.Content()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle(TEXT("SocketChooser.TitleFont")) )
					.Text( NSLOCTEXT("SocketChooser", "ChooseSocketOrBoneLabel", "Choose Socket or Bone:") )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(512)
				[
					SNew(SBox)
					.WidthOverride(256)
					.Content()
					[
						SAssignNew(SocketListView, SListView< TSharedPtr<FSocketInfo> >)
						.ListItemsSource( &Sockets )
						.OnGenerateRow( this, &SSocketChooserPopup::MakeItemWidget )
						.OnSelectionChanged( this, &SSocketChooserPopup::SelectedSocket )
					]
				]
			]
		];
	}
	
	/** SWidget interface */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override
	{		
		// Make sure that my window is on-screen
		TSharedPtr<SWindow> Window = CheckAndGetWindowPtr();
		if(Window.IsValid())
		{
			FVector2D WindowLocation = Window->IsMorphing() ? Window->GetMorphTargetPosition() : Window->GetPositionInScreen();
			FVector2D WindowSize = Window->GetDesiredSize();
			FSlateRect Anchor(WindowLocation.X, WindowLocation.Y, WindowLocation.X, WindowLocation.Y);
			WindowLocation = FSlateApplication::Get().CalculatePopupWindowPosition( Anchor, WindowSize );

			// Update the window's position!
			if( Window->IsMorphing() )
			{
				if( WindowLocation != Window->GetMorphTargetPosition() )
				{
					Window->UpdateMorphTargetShape( FSlateRect(WindowLocation.X, WindowLocation.Y, WindowLocation.X + WindowSize.X, WindowLocation.Y + WindowSize.Y) );
				}
			}
			else
			{
				if( WindowLocation != Window->GetPositionInScreen() )
				{
					Window->MoveWindowTo(WindowLocation);
				}
			}
		}
	}

private:
	/** Check and get the cached window pointer so that we don't need to find it if we already have it */
	TSharedPtr<SWindow> CheckAndGetWindowPtr()
	{
		if(WidgetWindow.IsValid())
		{
			return WidgetWindow.Pin();
		}
		else
		{
			TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
			WidgetWindow = Window;
			return Window;
		}
	}
};
