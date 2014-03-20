// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class IFuncTestManager
{
public:
	/** Triggers in sequence all functional tests found on the level.*/
	virtual void RunAllTestsOnMap(bool bClearLog, bool bRunLooped) = 0;

	virtual bool IsRunning() const = 0;
	
	virtual void SetScript(class UFunctionalTestingManager* NewScript) = 0;

	virtual void SetLooping(const bool bLoop) = 0;
};