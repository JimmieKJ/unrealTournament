// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDocumentation.h"
#include "Persona.h"

#define LOCTEXT_NAMESPACE "SkeletonSmartNameManager"

struct FSkeletonCurveNameManagerSummoner : public FWorkflowTabFactory
{
	FSkeletonCurveNameManagerSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return IDocumentation::Get()->CreateToolTip(LOCTEXT("CurveNamesTooltip", "Allows management of curve names and metadata names stored on the current skeleton (Add / Remove / Rename)"), nullptr, TEXT("Shared/Editors/Persona"), TEXT("CurveNameManager_Window"));
	}
};

//////////////////////////////////////////////////////////////////////////
// Used to store display information for each name in the list.

class FDisplayedSmartNameInfo
{
public:
	FSmartNameMapping::UID					NameUid;		// The UID of the displayed name
	FName									ContainerName;	// The container in the skeleton this name resides in
	const class USkeleton*					Skeleton;		// The skeleton we're associated with
	TSharedPtr<SInlineEditableTextBlock>	EditableText;	// The editable text box in the list, used to focus from the context menu

	static TSharedRef<FDisplayedSmartNameInfo> Make(const class USkeleton* InSkeleton, const FName& InContainerName, const FSmartNameMapping::UID InNameUid)
	{
		return MakeShareable(new FDisplayedSmartNameInfo(InSkeleton, InContainerName, InNameUid));
	}

private:

	// Hidden constructors, use Make() to create an instance of this class
	FDisplayedSmartNameInfo();

	FDisplayedSmartNameInfo(const class USkeleton* InSkeleton, const FName& InContainerName, const FSmartNameMapping::UID InNameUid)
		: NameUid(InNameUid)
		, ContainerName(InContainerName)
		, Skeleton(InSkeleton)
	{}

};

//////////////////////////////////////////////////////////////////////////
// Generalized smart name list and search widget 

typedef SListView<TSharedPtr<FDisplayedSmartNameInfo>> SSmartNameListType;

class SSkeletonSmartNameManager : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSkeletonSmartNameManager)
		: _Persona()
		, _ContainerName()
		, _NameColumnDescription()
	{}

	SLATE_ARGUMENT(TWeakPtr<FPersona>, Persona)			// Our Persona instance
	SLATE_ARGUMENT(FName, ContainerName)				// The name of the smartname container to manage
	SLATE_ARGUMENT(FText, NameColumnDescription)		// Description text for the display column header

	SLATE_END_ARGS();

	virtual ~SSkeletonSmartNameManager();

	void Construct(const FArguments& InArgs);

	// Get the current filter text from the search box
	FText& GetNameFilterText();

	// When a name is committed after being edited in the list
	virtual void OnNameCommitted(const FText& NewName, ETextCommit::Type CommitType, TSharedPtr<FDisplayedSmartNameInfo> Item);

protected:

	// Updates the search filter
	void OnFilterTextChanged(const FText& FilterText);

	// Updates the search filter
	void OnFilterTextCommitted(const FText& FilterText, ETextCommit::Type CommitType);

	// Generates a table row for the provided info object
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FDisplayedSmartNameInfo> InInfo, const TSharedRef<STableViewBase>& OwningTable);

	// Creates a context menu
	TSharedPtr<SWidget> OnGetContextMenu() const;

	// Clears and regenerates the displayed info list for the window
	void GenerateDisplayedList(const FText& FilterText);

	// Attempt to delete a smartname. This can be overriden in derived classes to handle the
	// delete as this version will simply find the correct name container in the skeleton and
	// remove the name.
	virtual void OnDeleteNameClicked();

	// Rename clicked on the context menu, simply activates the editable text box in the
	// row. Can be overridden in derived classes to provide extra functionality if
	// necessary
	virtual void OnRenameClicked();

	// Add clicked in the context menu. Shows a text entry popup to add a new name to the
	// smartname container in the skeleton
	virtual void OnAddClicked();

	// Called by Persona after an undo operation, base version simply refreshes the
	// list to repopulate it if something had been deleted
	virtual void OnPostUndo();

	// Used to control delete availability. Checks to see if we have anything selected
	bool CanDelete();
	
	// Used to control rename availability. Checks to see if we have one item selected
	bool CanRename();
	
	// Adds a new smartname entry to the skeleton in the container we are managing
	void CreateNewNameEntry(const FText& CommittedText, ETextCommit::Type CommitType);

	TWeakPtr<FPersona> WeakPersona;										// Persona instance this manager is in
	class USkeleton* CurrentSkeleton;									// The current skeleton being edited by out Persona instance
	FName ContainerName;												// Name of the skeleton smart name container to display in the list
	TSharedPtr<SSearchBox> ListSearchBox;								// The search box at the top of the window
	TSharedPtr<SSmartNameListType> ListWidget;							// The list inside the window
	TArray<TSharedPtr<FDisplayedSmartNameInfo>> DisplayedInfoList;		// List of information created for the list
	FText CurrentFilterText;											// The text currently entered in the search box at the top of the window

};

//////////////////////////////////////////////////////////////////////////
// Specialised smart name manager to handle animation curves
class SCurveNameManager : public SSkeletonSmartNameManager
{
public:

	SLATE_BEGIN_ARGS(SCurveNameManager)
		: _Persona()
		, _ContainerName()
		, _NameColumnDescription()
		{}

		SLATE_ARGUMENT(TWeakPtr<FPersona>, Persona)			// Our Persona instance
		SLATE_ARGUMENT(FName, ContainerName)				// Name of the container to manage in the skeleton
		SLATE_ARGUMENT(FText, NameColumnDescription)		// Description to show in the name column
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		TSharedPtr<FPersona> SharedPersona = InArgs._Persona.Pin();
		if(SharedPersona.IsValid())
		{
			SharedPersona->RegisterOnChangeCurves(FPersona::FOnCurvesChanged::CreateSP(this, &SCurveNameManager::OnCurvesChanged));
		}

		SSkeletonSmartNameManager::Construct(SSkeletonSmartNameManager::FArguments()
											 .ContainerName(InArgs._ContainerName)
											 .Persona(InArgs._Persona)
											 .NameColumnDescription(InArgs._NameColumnDescription));
	}

	virtual ~SCurveNameManager()
	{
		TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
		if(SharedPersona.IsValid())
		{
			SharedPersona->UnregisterOnChangeCurves(this);
		}
	}

protected:

	// Checks dependant animations that could be using the requested curve name and
	// checks that the user wants to remove that curve.
	virtual void OnDeleteNameClicked() override;

	// Regenerates the list of curve names.
	void OnCurvesChanged()
	{
		GenerateDisplayedList(CurrentFilterText);
	}

};

#undef LOCTEXT_NAMESPACE
