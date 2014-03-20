// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LaunchPrivatePCH.h"
#include "ExceptionHandling.h"

#if WITH_EDITOR
#include "Editor/MainFrame/Private/MainFrameActions.h"
#endif

#include <locale.h>
#include <sys/resource.h>

static FString GSavedCommandLine;
extern int32 GuardedMain( const TCHAR* CmdLine );
extern void LaunchStaticShutdownAfterError();

// FIXME: handle expose it someplace else?
extern int32 ReportCrash(const FLinuxCrashContext & Context);
extern void GenerateCrashInfoAndLaunchReporter(const FLinuxCrashContext & Context);

/**
 * Game-specific crash reporter
 */
void EngineCrashHandler(const FGenericCrashContext & GenericContext)
{
	const FLinuxCrashContext& Context = static_cast< const FLinuxCrashContext& >( GenericContext );

	printf("EngineCrashHandler: Signal=%d\n", Context.Signal);
	ReportCrash(Context);
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
		GError->HandleError();
	}
	//LaunchStaticShutdownAfterError();	// this tends to crash itself, so while it should be here, it needs to be commented
	return GenerateCrashInfoAndLaunchReporter(Context);
}


/**
 * Increases (soft) limit on a specific resource
 *
 * @param DesiredLimit - max number of open files desired.
 */
static bool IncreaseLimit(int Resource, rlim_t DesiredLimit)
{
	rlimit Limit;
	if (getrlimit(Resource, &Limit) != 0)
	{
		fprintf(stderr, "getrlimit() failed with error %d (%s)\n", errno, strerror(errno));
		return false;
	}

	if (Limit.rlim_cur == RLIM_INFINITY || Limit.rlim_cur >= DesiredLimit)
	{
#if !UE_BUILD_SHIPPING
		printf("- Existing per-process limit (soft=%lu, hard=%lu) is enough for us (need only %lu)\n", Limit.rlim_cur, Limit.rlim_max, DesiredLimit);
#endif // !UE_BUILD_SHIPPING
		return true;
	}

	Limit.rlim_cur = DesiredLimit;
	if (setrlimit(Resource, &Limit) != 0)
	{
		fprintf(stderr, "setrlimit() failed with error %d (%s)\n", errno, strerror(errno));

		if (errno == EINVAL)
		{
			if (DesiredLimit == RLIM_INFINITY)
			{
				fprintf(stderr, "- Max per-process value allowed is %lu (we wanted infinity).\n", Limit.rlim_max);
			}
			else
			{
				fprintf(stderr, "- Max per-process value allowed is %lu (we wanted %lu).\n", Limit.rlim_max, DesiredLimit);
			}
		}
		return false;
	}

	return true;
}

/**
 * Increases limit on 
 *  - number of open files to be no less than desired, so we can handle loading un-pak-ed builds
 *  - size of core file, so core gets dumped and we can debug crashed builds
 *
 * @param DesiredLimit - max number of open files desired.
 */
static bool IncreasePerProcessLimits(int32 DesiredNumberOfFiles)
{
#if !UE_BUILD_SHIPPING
	printf("Increasing per-process limit of open file handles to %d\n", DesiredNumberOfFiles);
#endif // !UE_BUILD_SHIPPING
	if ( !IncreaseLimit(RLIMIT_NOFILE, DesiredNumberOfFiles) )
	{
		fprintf(stderr, "Could not adjust number of file handles, consider changing \"nofile\" in /etc/security/limits.conf and relogin.\n", errno, strerror(errno));
		return false;
	}

#if !UE_BUILD_SHIPPING
	printf("Increasing per-process limit of core file size to infinity.\n");
#endif // !UE_BUILD_SHIPPING
	if ( !IncreaseLimit(RLIMIT_CORE, RLIM_INFINITY))
	{
		fprintf(stderr, "Could not adjust core file size, consider changing \"core\" in /etc/security/limits.conf and relogin.\n", errno, strerror(errno));
		return false;
	}

	return true;
}


int main(int argc, char *argv[])
{
	FPlatformMisc::SetGracefulTerminationHandler();

#if UE_BUILD_SHIPPING 
	// only printed in shipping
	printf("%s %d %d %d %d\n", StringCast<ANSICHAR>(*GEngineVersion.ToString()).Get(), GEngineMinNetVersion, GEngineNegotiationVersion, GPackageFileUE4Version, GPackageFileLicenseeUE4Version);
#endif // UE_BUILD_SHIPPING
	int ErrorLevel = 0;

	setlocale(LC_CTYPE, "");
	for (int32 Option = 1; Option < argc; Option++)
	{
		GSavedCommandLine += TEXT(" ");
		GSavedCommandLine += UTF8_TO_TCHAR(argv[Option]);	// note: technically it depends on locale
	}

#if !UE_BUILD_SHIPPING
	GAlwaysReportCrash = true;	// set by default and reverse the behavior
	if ( FParse::Param( *GSavedCommandLine,TEXT("nocrashreports") ) || FParse::Param( *GSavedCommandLine,TEXT("no-crashreports") ) )
	{
		GAlwaysReportCrash = false;
	}
#endif

	int32 FileHandlesToReserve = 10000, OverrideFileHandlesToReserve;
	if (FParse::Param( *GSavedCommandLine,TEXT("pak")))
	{
		FileHandlesToReserve = 256;	// we need significantly fewer file handles when opening paks 
	}

	// allow to override
	if (FParse::Value( *GSavedCommandLine, TEXT("numopenfiles"), OverrideFileHandlesToReserve) && OverrideFileHandlesToReserve > 0)
	{
		FileHandlesToReserve = OverrideFileHandlesToReserve;
	}

	if (!IncreasePerProcessLimits( FileHandlesToReserve ))
	{
		fprintf(stderr, "Could not set desired per-process limits, consider changing system limits.\n");
		ErrorLevel = 1;
	}
	else
	{
#if UE_BUILD_DEBUG
		if( true && !GAlwaysReportCrash )
#else
		if( FPlatformMisc::IsDebuggerPresent() && !GAlwaysReportCrash )
#endif
		{
			// Don't use exception handling when a debugger is attached to exactly trap the crash. This does NOT check
			// whether we are the first instance or not!
			ErrorLevel = GuardedMain( *GSavedCommandLine );
		}
		else
		{
			FPlatformMisc::SetCrashHandler(EngineCrashHandler);
			GIsGuarded = 1;
			// Run the guarded code.
			ErrorLevel = GuardedMain( *GSavedCommandLine );
			GIsGuarded = 0;
		}
	}

	return ErrorLevel;
}
