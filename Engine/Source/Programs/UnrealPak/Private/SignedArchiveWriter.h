// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * Wrapper for writing and signing an archive
 */
class FSignedArchiveWriter : public FArchive
{
	/** Buffer to sign */
	TArray<uint8> Buffer;
	/** Buffer writer */
	FMemoryWriter BufferArchive;
	/** The actuall pak archive */
	FArchive& PakWriter;
	/** Size of the archive on disk (including signatures) */
	int64 SizeOnDisk;
	/** Data size (excluding signatures) */
	int64 PakSize;
	/** Decryption key */
	FEncryptionKey PublicKey;
	/** Encryption key */
	FEncryptionKey PrivateKey;

	/** 
	 * Encrypts a signature 
	 */
	void Encrypt(int256* EncryptedData, const uint8* Data, const int64 DataLength, const FEncryptionKey EncryptionKey);
	/** 
	 * Serializes and signs a buffer
	 */
	void SerializeBufferAndSign();

public:

	FSignedArchiveWriter(FArchive& Pak, const FEncryptionKey& InPublicKey, const FEncryptionKey& InPrivateKey);
	virtual ~FSignedArchiveWriter();

	// FArchive interface
	virtual bool Close() override;
	virtual void Serialize(void* Data, int64 Length) override;
	virtual int64 Tell() override;
	virtual int64 TotalSize() override;
	virtual void Seek(int64 InPos) override;
};
