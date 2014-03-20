// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "MinidumpDiagnosticsApp.h"
#include "ModuleManager.h"
#include "CrashDebugHelperModule.h"

int32 RunMinidumpDiagnostics( int32 ArgC, ANSICHAR* Argv[] )
{
	// Make sure we have at least a single parameter
	if( ArgC < 2 )
	{
		UE_LOG( LogInit, Error, TEXT( "MinidumpDiagnostics - not enough parameters." ) );
		UE_LOG( LogInit, Error, TEXT( " ... usage: MinidumpDiagnostics.exe <Crash.dmp> [-Annotate] [-SyncSymbols] [-SyncMicrosoftSymbols]" ) );
		UE_LOG( LogInit, Error, TEXT( " ..." ) );
		UE_LOG( LogInit, Error, TEXT( " ... -Annotate: Use Perforce annotation to decorate the source context" ) );
		UE_LOG( LogInit, Error, TEXT( " ... -SyncSymbols: Sync symbols to the revision specified by the engine version" ) );
		UE_LOG( LogInit, Error, TEXT( " ... -SyncMicrosoftSymbols: Sync symbols from the Microsoft Symbol Server" ) );
		return 1;
	}

	// Load in the stack walking module
	FCrashDebugHelperModule& CrashHelperModule = FModuleManager::LoadModuleChecked<FCrashDebugHelperModule>( FName( "CrashDebugHelper" ) );
	ICrashDebugHelper* CrashDebugHelper = CrashHelperModule.Get();
	if( CrashDebugHelper == NULL )
	{
		UE_LOG( LogInit, Error, TEXT( " ... failed to load CrashDebugHelper module; unsupported platform?" ) );
		return 1;
	}

	// Load in the perforce source control plugin, as standalone programs don't currently support plugins and
	// we don't support any other provider apart from Perforce in this module.
	IModuleInterface& PerforceSourceControlModule = FModuleManager::LoadModuleChecked<IModuleInterface>( FName( "PerforceSourceControl" ) );

	// Create a report for the minidump passed in on the command line
	FString MinidumpName = ANSI_TO_TCHAR( Argv[1] );
	CrashDebugHelper->CreateMinidumpDiagnosticReport( MinidumpName );

	// Write a report next to the original minidump
	FString DiagnosticsPath = FPaths::GetPath(MinidumpName);
	DiagnosticsPath /= FString( TEXT( "Diagnostics.txt" ) );
	CrashDebugHelper->CrashInfo.GenerateReport( DiagnosticsPath );

	// Cleanup
	PerforceSourceControlModule.ShutdownModule();
	CrashHelperModule.ShutdownModule();

	UE_LOG( LogInit, Warning, TEXT( "MinidumpDiagnostics completed successfully!" ) );
	return 0;
}

