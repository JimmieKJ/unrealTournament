// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


class FCookingStats : public ICookingStats
{
public:

	FCookingStats();
	virtual ~FCookingStats();


	/**
	* AddRunTag
	* Add a tag and associate it with this run of the editor 
	* this is for storing global information from this run
	*
	* @param Tag tag which we want to keep
	* @param Value for this tag FString() if we just want to store the tag
	*/
	virtual void AddRunTag(const FName& Tag, const FString& Value) override;

	/**
	* AddTag
	* add a tag to a key, the tags will be saved out with the key
	* tags can be timing information or anything
	*
	* @param Key key to add the tag to
	* @param Tag tag to add to the key
	*/
	virtual void AddTag(const FName& Key, const FName& Tag) override;

	/**
	* AddTag
	* add a tag to a key, the tags will be saved out with the key
	* tags can be timing information or anything
	*
	* @param Key key to add the tag to
	* @param Tag tag to add to the key
	*/
	virtual void AddTagValue(const FName& Key, const FName& Tag, const FString& Value) override;

	/**
	* GetTagValue
	* retrieve a previously added tag value
	*
	* @param Key to use to get the tag value
	* @param Tag to use to get the tag value
	* @param Value the value which will be returned fomr that tag key pair
	* @return if the key + tag exists will retrun true false otherwise
	*/
	virtual bool GetTagValue(const FName& Key, const FName& Tag, FString& Value) const override;

	/**
	* SaveStatsAsCSV
	* Save stats in a CSV file
	*
	* @param Filename file name to save comma delimited stats to
	* @return true if succeeded
	*/
	virtual bool SaveStatsAsCSV(const FString& Filename) const override;

private:
	FName RunGuid; // unique guid for this run of the editor

	TMap< FName, TMap<FName,FString> > KeyTags;
	TMap<FName, FString> GlobalTags; // tags to add to every key in this runthrough
	mutable FCriticalSection SyncObject;
};