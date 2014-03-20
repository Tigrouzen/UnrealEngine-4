// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CollectionManagerPrivatePCH.h"
#include "CollectionManagerModule.h"
#include "CollectionManagerConsoleCommands.h"

IMPLEMENT_MODULE( FCollectionManagerModule, CollectionManager );
DEFINE_LOG_CATEGORY(LogCollectionManager);

void FCollectionManagerModule::StartupModule()
{
	CollectionManager = new FCollectionManager();
	ConsoleCommands = new FCollectionManagerConsoleCommands(*this);
}

void FCollectionManagerModule::ShutdownModule()
{
	if (CollectionManager != NULL)
	{
		delete CollectionManager;
		CollectionManager = NULL;
	}

	if (ConsoleCommands != NULL)
	{
		delete ConsoleCommands;
		ConsoleCommands = NULL;
	}
}

ICollectionManager& FCollectionManagerModule::Get() const
{
	check(CollectionManager);
	return *CollectionManager;
}