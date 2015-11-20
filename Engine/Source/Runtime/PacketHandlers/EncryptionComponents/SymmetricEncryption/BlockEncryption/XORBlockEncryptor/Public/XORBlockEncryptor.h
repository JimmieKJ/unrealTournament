// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlockEncryptionHandlerComponent.h"

/* XOR Block Encryptor Module Interface */
class FXORBlockEncryptorModuleInterface : public FBlockEncryptorModuleInterface
{
	virtual BlockEncryptor* CreateBlockEncryptorInstance() override;
};

/*
* XOR Block encryption
*/
class XORBLOCKENCRYPTOR_API XORBlockEncryptor : public BlockEncryptor
{
public:
	/* Initialized the encryptor */
	void Initialize(TArray<byte>* Key) override;

	/* Encrypts outgoing packets */
	void EncryptBlock(byte* Block) override;

	/* Decrypts incoming packets */
	void DecryptBlock(byte* Block) override;

	/* Get the default key size for this encryptor */
	uint32 GetDefaultKeySize() { return 4; }
};
