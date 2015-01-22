// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Delegate type for handling file requests from a network client.
 *
 * The first parameter is the name of the requested file.
 * The second parameter will hold the list of unsolicited files to send back.
 */
DECLARE_DELEGATE_ThreeParams(FFileRequestDelegate, const FString&, const FString&, TArray<FString>&);

struct FShaderRecompileData
{
	FString PlatformName;
	/** The platform to compile shaders for, corresponds to EShaderPlatform, but a value of -1 indicates to compile for all target shader platforms. */
	int32 ShaderPlatform;
	TArray<FString>* ModifiedFiles;
	TArray<uint8>* MeshMaterialMaps;
	TArray<FString> MaterialsToLoad;
	TArray<uint8> SerializedShaderResources;
	bool bCompileChangedShaders;

	FShaderRecompileData() :
		ShaderPlatform(-1),
		bCompileChangedShaders(true)
	{}

	FShaderRecompileData& operator=(const FShaderRecompileData& Other)
	{
		PlatformName = Other.PlatformName;
		ShaderPlatform = Other.ShaderPlatform;
		ModifiedFiles = Other.ModifiedFiles;
		MeshMaterialMaps = Other.MeshMaterialMaps;
		MaterialsToLoad = Other.MaterialsToLoad;
		SerializedShaderResources = Other.SerializedShaderResources;
		bCompileChangedShaders = Other.bCompileChangedShaders;

		return *this;
	}
};


/**
 * Delegate type for handling shader recompilation requests from a network client.
 */
DECLARE_DELEGATE_OneParam(FRecompileShadersDelegate, const FShaderRecompileData&);

enum ENetworkFileServerProtocol
{
	NFSP_Tcp,
	NFSP_Http,
};

/**
 * Interface for network file system modules.
 */
class INetworkFileSystemModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a new network file server.
	 *
	 * @param InPort The port number to bind to (-1 = default port, 0 = any available port).
	 * @param Streaming Whether it should be a streaming server.
	 * @param InFileRequestDelegate An optional delegate to be invoked when a file is requested by a client.
	 * @param InRecompileShadersDelegate An optional delegate to be invoked when shaders need to be recompiled.
	 *
	 * @return The new file server, or nullptr if creation failed.
	 */
	virtual INetworkFileServer* CreateNetworkFileServer( bool bLoadTargetPlatforms, int32 Port = -1, const FFileRequestDelegate* InFileRequestDelegate = nullptr, const FRecompileShadersDelegate* InRecompileShadersDelegate = nullptr, const ENetworkFileServerProtocol Protocol = NFSP_Tcp ) const = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~INetworkFileSystemModule( ) { }
};