// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FChatSettingsService
	: public IChatSettingsService
	, public TSharedFromThis<FChatSettingsService>
{
public:
	virtual ~FChatSettingsService() {}
};

/**
 * Creates the implementation for an FChatSettingsService.
 *
 * @return the newly created FChatSettingsService implementation.
 */
FACTORY(TSharedRef< FChatSettingsService >, FChatSettingsService);