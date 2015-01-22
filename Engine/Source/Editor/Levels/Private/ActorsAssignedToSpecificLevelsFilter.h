// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once


class FActorsAssignedToSpecificLevelsFilter : public IFilter< const AActor* const >, public TSharedFromThis< FActorsAssignedToSpecificLevelsFilter >
{
public:

	DECLARE_DERIVED_EVENT(FActorsAssignedToSpecificLevelsFilter, IFilter< const AActor* const >::FChangedEvent, FChangedEvent);
	virtual FChangedEvent& OnChanged() override { return ChangedEvent; }

	/** 
	 * Returns whether the specified Item passes the Filter's text restrictions 
	 *
	 *	@param	InItem	The Item to check 
	 *	@return			Whether the specified Item passed the filter
	 */
	virtual bool PassesFilter( const AActor* const InActor ) const override
	{
		if( LevelNames.Num() == 0 )
		{
			return false;
		}

		bool bBelongsToLevels = true;
		for( auto LevelIter = LevelNames.CreateConstIterator(); bBelongsToLevels && LevelIter; ++LevelIter )
		{
			bBelongsToLevels = ( InActor->GetLevel()->GetFName() == *LevelIter);
		}

		return bBelongsToLevels;
	}

	/**
	 * Sets the list of levels which actors need to belong to
	 *
	 *	@param	InLevelNames	The new LevelNames Array
	 */
	void SetLevels( const TArray< FName >& InLevelNames )
	{
		LevelNames.Empty();

		for( auto LevelNameIter = InLevelNames.CreateConstIterator(); LevelNameIter; ++LevelNameIter )
		{
			LevelNames.AddUnique( *LevelNameIter );
		}

		ChangedEvent.Broadcast();
	}

	/**
	 * Adds the specified level to the list of levels which actors need to belong to
	 *
	 *	@param	LevelName	The LevelName to add
	 */
	void AddLevel( FName LevelName )
	{
		LevelNames.AddUnique( LevelName );
		ChangedEvent.Broadcast();
	}

	/**
	 * Removes the specified level from the list of levels which actors need to belong to
	 *
	 *	@param	LevelName	The LevelName to remove
	 */
	bool RemoveLevel( FName LevelName )
	{
		return LevelNames.Remove( LevelName ) > 0;
		ChangedEvent.Broadcast();
	}

	/**
	 *	Empties the list of levels which actors need to belong to
	 */
	void ClearLevels()
	{
		LevelNames.Empty();
		ChangedEvent.Broadcast();
	}


private:

	/**	The list of Level names which actors need to belong to */
	TArray< FName > LevelNames;

	/**	The event that broadcasts whenever a change occurs to the filter */
	FChangedEvent ChangedEvent;
};