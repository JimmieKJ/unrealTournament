// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FNameTableArchiveReader : public FArchive
{
public:
	FNameTableArchiveReader();

	bool LoadFile(const TCHAR* Filename, int32 SerializationVersion);

	/** Serializers for different package maps */
	bool SerializeNameMap();
	
	// Farchive implementation to redirect requests to the Loader
	void Serialize( void* V, int64 Length );
	bool Precache( int64 PrecacheOffset, int64 PrecacheSize );
	void Seek( int64 InPos );
	int64 Tell();
	int64 TotalSize();
	FArchive& operator<<( FName& Name );

private:
	FArrayReader Reader;
	TArray<FName> NameMap;
};

class FNameTableArchiveWriter : public FArchive
{
public:
	FNameTableArchiveWriter(int32 SerializationVersion);

	bool SaveToFile(const TCHAR* Filename);

	/** Serializers for different package maps */
	void SerializeNameMap();

	// Farchive implementation to redirect requests to the Loader
	void Serialize( void* V, int64 Length );
	bool Precache( int64 PrecacheOffset, int64 PrecacheSize );
	void Seek( int64 InPos );
	int64 Tell();
	int64 TotalSize();
	FArchive& operator<<( FName& Name );

private:
	FArrayWriter Writer;
	TArray<FName> NameMap;
	TMap<FName, int32, FDefaultSetAllocator, TLinkerNameMapKeyFuncs<int32>> NameMapLookup;
	int64 NameOffsetLoc;
};