// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidProcess.cpp: Android implementations of Process functions
=============================================================================*/

#include "CorePrivate.h"
#include "AndroidPlatformRunnableThread.h"

const TCHAR* FAndroidPlatformProcess::ComputerName()
{
	return TEXT("Android Device"); 
}

const TCHAR* FAndroidPlatformProcess::BaseDir()
{
	return TEXT("");
}

const TCHAR* FAndroidPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static FString CachedExeName;
	if (CachedExeName.Len() == 0)
	{
		extern FString GPackageName;
		int32 LastDot;
		GPackageName.FindLastChar('.', LastDot);

		// get just the last bit after all dots
		CachedExeName = GPackageName.Mid(LastDot + 1);
	}

	// the string is static, so we can return the characters directly
	return *CachedExeName;
}

FRunnableThread* FAndroidPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadAndroid();
}

DECLARE_DELEGATE_OneParam(FAndroidLaunchURLDelegate, const FString&);

CORE_API FAndroidLaunchURLDelegate OnAndroidLaunchURL;

void FAndroidPlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	check(URL);
	const FString URLWithParams = FString::Printf(TEXT("%s %s"), URL, Parms ? Parms : TEXT("")).TrimTrailing();

	OnAndroidLaunchURL.ExecuteIfBound(URLWithParams);

	if (Error != nullptr)
	{
		*Error = TEXT("");
	}
}
