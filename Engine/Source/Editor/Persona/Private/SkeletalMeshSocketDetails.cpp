// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "SkeletalMeshSocketDetails.h"
#include "SAssetSearchBox.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "Persona.h"
#include "AssetSearchBoxUtilPersona.h"
#include "Engine/SkeletalMeshSocket.h"

TSharedRef<IDetailCustomization> FSkeletalMeshSocketDetails::MakeInstance()
{
	return MakeShareable( new FSkeletalMeshSocketDetails );
}

void FSkeletalMeshSocketDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TargetSocket = NULL;

	IDetailCategoryBuilder& SocketCategory = DetailBuilder.EditCategory( TEXT("SkeletalMeshSocket"), FText::GetEmpty(), ECategoryPriority::TypeSpecific );

	SocketNameProperty = DetailBuilder.GetProperty( TEXT("SocketName") );
	if( SocketNameProperty.IsValid() && SocketNameProperty->GetProperty() )
	{
		const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailBuilder.GetDetailsView().GetSelectedObjects();
		if( SelectedObjects.Num() == 1 )
		{
			const TWeakObjectPtr<UObject> BaseClass = SelectedObjects[0];
			const USkeletalMeshSocket* TargetSocketConst = Cast<const USkeletalMeshSocket> (BaseClass.Get());
			if( TargetSocketConst )
			{
				TargetSocket = const_cast<USkeletalMeshSocket*> (TargetSocketConst);
			}
		}

		// get the currently chosen bone/socket, if any
		const FText PropertyName = SocketNameProperty->GetPropertyDisplayName();
		FString CurValue;
		SocketNameProperty->GetValue(CurValue);
		if( CurValue == FString("None") )
		{
			CurValue.Empty();
		}
		PreEditSocketName = FText::FromString(CurValue);

		// create the editable text box
		SocketCategory.AddProperty( SocketNameProperty.ToSharedRef() )
		.CustomWidget()
		.NameContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding( FMargin( 2, 1, 0, 1 ) )
			[
				SNew(STextBlock)
				.Text(PropertyName)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
		.ValueContent()
		[
			SAssignNew(SocketNameTextBox, SEditableTextBox)
			.Text(FText::FromString(CurValue))
			.HintText( NSLOCTEXT( "SkeletalMeshSocketDetails", "SkeletalMeshSocketDetailsHintTextSocketName", "Socket Name..." ) )
			.OnTextCommitted( this, &FSkeletalMeshSocketDetails::OnSocketNameCommitted )
			.OnTextChanged( this, &FSkeletalMeshSocketDetails::OnSocketNameChanged )
			.ClearKeyboardFocusOnCommit(false)
		];
	}

	ParentBoneProperty = DetailBuilder.GetProperty( TEXT("BoneName") );
	if( TargetSocket && ParentBoneProperty.IsValid() && ParentBoneProperty->GetProperty() )
	{
		if( const UObject* Outer = TargetSocket->GetOuter() )
		{
			SocketCategory.AddProperty( ParentBoneProperty.ToSharedRef() )
			.CustomWidget()
			.NameContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding( FMargin( 2, 1, 0, 1 ) )
				[
					SNew(STextBlock)
					.Text(ParentBoneProperty->GetPropertyDisplayName())
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			]
			.ValueContent()
			[
				SNew(SAssetSearchBoxForBones, Outer, ParentBoneProperty)
				.IncludeSocketsForSuggestions(false)
				.MustMatchPossibleSuggestions(true)
				.HintText(NSLOCTEXT( "SkeletalMeshSocketDetails", "SkeletalMeshSocketDetailsHintTextBoneName", "Bone Name..." ))
				.OnTextCommitted( this, &FSkeletalMeshSocketDetails::OnParentBoneNameCommitted )
			];
		}
	}
}

void FSkeletalMeshSocketDetails::OnParentBoneNameCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo)
{
	UObject* Outer = TargetSocket->GetOuter();
	USkeleton* Skeleton = Cast<USkeleton>(Outer);
	if( Skeleton == NULL )
	{
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Outer);
		if( SkeletalMesh )
		{
			Skeleton = SkeletalMesh->Skeleton;
		}
	}
	if( Skeleton )
	{
		if( Skeleton->GetReferenceSkeleton().FindBoneIndex( *InSearchText.ToString() ) != INDEX_NONE )
		{
			IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(Outer, false);
			if( EditorInstance->GetEditorName() == TEXT("Persona") )
			{
				FPersona* PersonaInstance = static_cast<FPersona*> (EditorInstance);
				PersonaInstance->ChangeSocketParent( TargetSocket, *InSearchText.ToString() );
				PersonaInstance->SetSelectedSocket( FSelectedSocketInfo( TargetSocket, true ) );
			}

			ParentBoneProperty->SetValue( InSearchText.ToString() );
		}
	}
}

void FSkeletalMeshSocketDetails::OnSocketNameChanged(const FText& InSearchText)
{
	if( UObject* Outer = TargetSocket->GetOuter() )
	{
		IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(Outer, false);
		if( EditorInstance->GetEditorName() == TEXT("Persona") )
		{
			FPersona* PersonaInstance = static_cast<FPersona*> (EditorInstance);
			FText OutErrorMessage;
			if( VerifySocketName(PersonaInstance, TargetSocket, InSearchText, OutErrorMessage) )
			{
				SocketNameTextBox->SetError( FText::GetEmpty() );
			}
			else
			{
				SocketNameTextBox->SetError( OutErrorMessage );
			}
		}
	}
}

void FSkeletalMeshSocketDetails::OnSocketNameCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo)
{
	if( UObject* Outer = TargetSocket->GetOuter() )
	{
		IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(Outer, false);
		if( EditorInstance->GetEditorName() == TEXT("Persona") )
		{
			FPersona* PersonaInstance = static_cast<FPersona*> (EditorInstance);

			FText NewText = FText::TrimPrecedingAndTrailing(InSearchText);

			FText OutErrorMessage;
			if (VerifySocketName(PersonaInstance, TargetSocket, NewText, OutErrorMessage))
			{
				// tell persona to rename the socket.
				PersonaInstance->RenameSocket(TargetSocket, *NewText.ToString());
				PersonaInstance->SetSelectedSocket( FSelectedSocketInfo( TargetSocket, true ) );
				
				// update the pre-edit socket name and the property
				PreEditSocketName = NewText;
				SocketNameTextBox->SetText(NewText);
			}
			else
			{
				// restore the pre-edit name to the socket text box.
				SocketNameTextBox->SetText(PreEditSocketName);
			}
		}
	}

	if( CommitInfo == ETextCommit::OnUserMovedFocus )
	{
		SocketNameTextBox->SetError( FText::GetEmpty() );
	}
}

bool FSkeletalMeshSocketDetails::VerifySocketName( FPersona* PersonaPtr, const USkeletalMeshSocket* Socket, const FText& InText, FText& OutErrorMessage ) const
{
	// You can't have two sockets with the same name on the mesh, nor on the skeleton,
	// but you can have a socket with the same name on the mesh *and* the skeleton.
	bool bVerifyName = true;

	FText NewText = FText::TrimPrecedingAndTrailing(InText);

	if (NewText.IsEmpty())
	{
		OutErrorMessage = NSLOCTEXT( "SkeletalMeshSocketDetails", "EmptySocketName_Error", "Sockets must have a name!");
		bVerifyName = false;
	}
	else
	{
		if (PersonaPtr->DoesSocketAlreadyExist(Socket, NewText, PersonaPtr->GetSkeleton()->Sockets))
		{
			bVerifyName = false;
		}

		if (bVerifyName && PersonaPtr->DoesSocketAlreadyExist(Socket, NewText, PersonaPtr->GetMesh()->GetMeshOnlySocketList()))
		{
			bVerifyName = false;
		}

		// Needs to be checked on verify.
		if ( !bVerifyName )
		{
			// Tell the user that the socket is a duplicate
			OutErrorMessage = NSLOCTEXT( "SkeletalMeshSocketDetails", "DuplicateSocket_Error", "Socket name in use!");
		}
	}
	return bVerifyName;
}
