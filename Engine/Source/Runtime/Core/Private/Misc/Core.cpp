// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Core.cpp: Unreal core.
=============================================================================*/

#include "CorePrivate.h"
#include "ModuleManager.h"

#define LOCTEXT_NAMESPACE "Core"

class FCoreModule : public FDefaultModuleImpl
{
public:
	virtual bool SupportsDynamicReloading() OVERRIDE
	{
		// Core cannot be unloaded or reloaded
		return false;
	}
};
IMPLEMENT_MODULE( FCoreModule, Core );

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

CORE_API FFeedbackContext*	GWarn						= NULL;						/* User interaction and non critical warnings */
FConfigCacheIni*		GConfig							= NULL;						/* Configuration database cache */
ITransaction*		GUndo							= NULL;						/* Transaction tracker, non-NULL when a transaction is in progress */
FOutputDeviceConsole*	GLogConsole						= NULL;						/* Console log hook */
CORE_API FMalloc*		GMalloc							= NULL;						/* Memory allocator */

class UPropertyWindowManager*	GPropertyWindowManager	= NULL;						/* Manages and tracks property editing windows */
TCHAR					GErrorHist[16384]				= TEXT("");					/* For building call stack text dump in guard/unguard mechanism */
TCHAR					GErrorExceptionDescription[1024] = TEXT("");				/* For building exception description text dump in guard/unguard mechanism */

CORE_API const FText GYes	= LOCTEXT("Yes",	"Yes");
CORE_API const FText GNo	= LOCTEXT("No",		"No");
CORE_API const FText GTrue	= LOCTEXT("True",	"True");
CORE_API const FText GFalse	= LOCTEXT("False",	"False");
CORE_API const FText GNone	= LOCTEXT("None",	"None");

/** If true, this executable is able to run all games (which are loaded as DLL's) **/
#if UE_GAME || UE_SERVER
	// In monolithic builds, implemented by the IMPLEMENT_GAME_MODULE macro or by UE4Game module.
	#if !IS_MONOLITHIC
		bool GIsGameAgnosticExe = true;
	#endif
#else
	// Otherwise only modular editors are game agnostic.
	#if IS_PROGRAM || IS_MONOLITHIC
		bool GIsGameAgnosticExe = false;
	#else
		bool GIsGameAgnosticExe = true;
	#endif
#endif

/** When saving out of the game, this override allows the game to load editor only properties **/
bool GForceLoadEditorOnly = false;

/** Name of the core package					**/
FName GLongCorePackageName(TEXT("/Script/Core"));
/** Name of the core package					**/
FName GLongCoreUObjectPackageName(TEXT("/Script/CoreUObject"));

/** Disable loading of objects not contained within script files; used during script compilation */
bool GVerifyObjectReferencesOnly = false;

/** when constructing objects, use the fast path on consoles... */
bool GFastPathUniqueNameGeneration = false;

/** allow AActor object to execute script in the editor from specific entry points, such as when running a construction script */
bool GAllowActorScriptExecutionInEditor = false;

/** Forces use of template names for newly instanced components in a CDO */
bool GCompilingBlueprint = false;

/** Force blueprints to not compile on load */
bool GForceDisableBlueprintCompileOnLoad = false;


float					GVolumeMultiplier				= 1.0f;						/* Use to silence the app when it loses focus */

#if WITH_EDITORONLY_DATA
bool					GIsEditor						= false;					/* Whether engine was launched for editing */
bool					GIsImportingT3D					= false;					/* Whether editor is importing T3D */
bool					PRIVATE_GIsRunningCommandlet	= false;					/* Whether this executable is running a commandlet (custom command-line processing code in an editor-like environment) */
bool					GIsUCCMakeStandaloneHeaderGenerator = false;					/* Are we rebuilding script via the standalone header generator? */
bool					GIsTransacting					= false;					/* true if there is an undo/redo operation in progress. */
bool					GIntraFrameDebuggingGameThread	= false;					/* Indicates that the game thread is currently paused deep in a call stack; do not process any game thread tasks */
bool					GFirstFrameIntraFrameDebugging	= false;					/* Indicates that we're currently processing the first frame of intra-frame debugging */
#endif // !WITH_EDITORONLY_DATA

bool					GEdSelectionLock				= false;					/* Are selections locked? (you can't select/deselect additional actors) */
bool					GIsClient						= false;					/* Whether engine was launched as a client */
bool					GIsServer						= false;					/* Whether engine was launched as a server, true if GIsClient */
bool					GIsCriticalError				= false;					/* An appError() has occured */
bool					GIsGuarded						= false;					/* Whether execution is happening within main()/WinMain()'s try/catch handler */
bool					GIsRunning						= false;					/* Whether execution is happening within MainLoop() */
bool					GIsGarbageCollecting			= false;					/* Whether we are inside garbage collection */
bool					GIsDuplicatingClassForReinstancing = false;					/* Whether we are currently using SDO on a UClass or CDO for live reinstancing */
/** This specifies whether the engine was launched as a build machine process								*/
bool					GIsBuildMachine					= false;
/** This determines if we should output any log text.  If Yes then no log text should be emitted.			*/
bool					GIsSilent						= false;
bool					GIsSlowTask						= false;					/* Whether there is a slow task in progress */
bool					GSlowTaskOccurred				= false;					/* Whether a slow task began last tick*/
bool					GIsRequestingExit				= false;					/* Indicates that MainLoop() should be exited at the end of the current iteration */
/** Archive for serializing arbitrary data to and from memory												*/
FReloadObjectArc*		GMemoryArchive					= NULL;
bool					GIsBenchmarking					= 0;						/* Whether we are in benchmark mode or not */
bool					GAreScreenMessagesEnabled		= true;						/* Whether onscreen warnings/messages are enabled */
bool					GScreenMessagesRestoreState		= false;					/* Used to restore state after a screenshot */
int32					GIsDumpingMovie					= 0;						/* Whether we are dumping screenshots (!= 0), exposed as console variable r.DumpingMovie */
bool					GIsHighResScreenshot			= false;					/* Whether we're capturing a high resolution shot */
uint32					GScreenshotResolutionX			= 0;						/* X Resolution for high res shots */
uint32					GScreenshotResolutionY			= 0;						/* Y Resolution for high res shots */
uint64					GMakeCacheIDIndex				= 0;						/* Cache ID */

FString				GEngineIni;													/* Engine ini filename */
FString				GEditorIni;													/* Editor ini filename */
FString				GEditorKeyBindingsIni;										/* Editor Key Bindings ini file */
FString				GEditorUserSettingsIni;										/* Editor User Settings ini filename */
FString				GEditorGameAgnosticIni;										/* Editor Settings (shared between games) ini filename */
FString				GCompatIni;
FString				GLightmassIni;												/* Lightmass settings ini filename */
FString				GScalabilityIni;											/* Scalability settings ini filename */
FString				GInputIni;													/* Input ini filename */
FString				GGameIni;													/* Game ini filename */
FString				GGameUserSettingsIni;										/* User Game Settings ini filename */

float					GNearClippingPlane				= 10.0f;					/* Near clipping plane */
/** Timestep if a fixed delta time is wanted.																*/
double					GFixedDeltaTime					= 1 / 30.f;
/** Current delta time in seconds.																			*/
double					GDeltaTime						= 1 / 30.f;
/** Current unclamped delta time in seconds.																*/
double					GUnclampedDeltaTime				= 1 / 30.f;
/* Current time																								*/
double					GCurrentTime					= 0;						
/* Last GCurrentTime																						*/
double					GLastTime						= 0;						

bool					GExitPurge						= false;

/** Game name, used for base game directory and ini among other things										*/
#if (!IS_MONOLITHIC && !IS_PROGRAM)
// In modular game builds, the game name will be set when the application launches
TCHAR					GGameName[64]					= TEXT("None");
#elif !IS_MONOLITHIC && IS_PROGRAM
// In non-monolithic programs builds, the game name will be set by the module, but not just yet, so we need to NOT initialize it!
TCHAR					GGameName[64];
#else
// For monolithic builds, the game name variable definition will be set by the IMPLEMENT_GAME_MODULE
// macro for the game's main game module.
#endif

/** Exec handler for game debugging tool, allowing commands like "editactor", ...							*/
FExec*					GDebugToolExec					= NULL;
/** Whether we're currently in the async loading codepath or not											*/
bool					GIsAsyncLoading					= false;
/** Whether the editor is currently loading a package or not												*/
bool					GIsEditorLoadingPackage				= false;
/** Whether GWorld points to the play in editor world														*/
bool					GIsPlayInEditorWorld			= false;
/** Unique ID for multiple PIE instances running in one process */
int32					GPlayInEditorID					= -1;
/** Whether or not PIE was attempting to play from PlayerStart							*/
bool					GIsPIEUsingPlayerStart			= false;
/** true if the runtime needs textures to be powers of two													*/
bool					GPlatformNeedsPowerOfTwoTextures = false;
/** Time at which FPlatformTime::Seconds() was first initialized (before main)											*/
double					GStartTime						= FPlatformTime::InitTiming();
/** System time at engine init.																				*/
FString					GSystemStartTime;
/** Whether we are still in the initial loading proces.														*/
bool					GIsInitialLoad					= true;
/** true when we are routing ConditionalPostLoad/PostLoad to objects										*/
bool					GIsRoutingPostLoad				= false;
/** Steadily increasing frame counter.																		*/
uint64					GFrameCounter					= 0;
/** Incremented once per frame before the scene is being rendered. In split screen mode this is incremented once for all views (not for each view). */
uint32					GFrameNumber					= 1;
/** Render Thread copy of the frame number. */
uint32					GFrameNumberRenderThread		= 1;
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
// We cannot count on this variable to be accurate in a shipped game, so make sure no code tries to use it
/** Whether we are the first instance of the game running.													*/
bool					GIsFirstInstance				= true;
#endif
/** Threshold for a frame to be considered a hitch (in seconds. */
float GHitchThreshold = 0.075f;
/** Whether to forcefully enable capturing of stats due to a profiler attached								*/
bool					GProfilerAttached = false;
/** Size to break up data into when saving compressed data													*/
int32					GSavingCompressionChunkSize		= SAVING_COMPRESSION_CHUNK_SIZE;
/** Whether we are using the seekfree/ cooked loading codepath.												*/
bool					GUseSeekFreeLoading				= false;
/** Thread ID of the main/game thread																		*/
uint32					GGameThreadId					= 0;
uint32					GRenderThreadId					= 0;
uint32					GSlateLoadingThreadId			= 0;
/** Has GGameThreadId been set yet?																			*/
bool					GIsGameThreadIdInitialized		= false;

/** A function that does nothing. Allows for a default behavior for callback function pointers. */
static void appNoop()
{
}

/** Helper function to flush resource streaming.															*/
void					(*GFlushStreamingFunc)(void)	  = &appNoop;
/** Whether to emit begin/ end draw events.																	*/
bool					GEmitDrawEvents					= false;
/** Whether we want the rendering thread to be suspended, used e.g. for tracing.							*/
bool					GShouldSuspendRenderingThread	= false;
/** Whether we want to use a fixed time step or not.														*/
bool					GUseFixedTimeStep				= false;
/** Determines what kind of trace should occur, NAME_None for none.											*/
FName					GCurrentTraceName				= NAME_None;
/** How to print the time in log output																		*/
ELogTimes::Type			GPrintLogTimes					= ELogTimes::None;
/** Global screen shot index, which is a way to make it so we don't have overwriting ScreenShots			*/
int32                     GScreenshotBitmapIndex           = -1;
/** Whether stats should emit named events for e.g. PIX.													*/
bool					GCycleStatsShouldEmitNamedEvents= false;
/** Disables some warnings and minor features that would interrupt a demo presentation						*/
bool					GIsDemoMode						= false;
/*
 * Whether to show slate batches 
 * @todo UE4: This does not belong here but is needed at the core level so slate will compile
*/
#if STATS
bool					GShowSlateBatches			= false;
#endif

/** Whether or not a unit test is currently being run														*/
bool					GIsAutomationTesting					= false;
/** Whether or not messages are being pumped outside of the main loop										*/
bool					GPumpingMessagesOutsideOfMainLoop = false;

/** Total number of calls to Malloc, if implemented by derived class.										*/
uint64 FMalloc::TotalMallocCalls;
/** Total number of calls to Free, if implemented by derived class.											*/
uint64 FMalloc::TotalFreeCalls;
/** Total number of calls to Realloc, if implemented by derived class.										*/
uint64 FMalloc::TotalReallocCalls;

/** Total blueprint compile time.																			*/
double GBlueprintCompileTime = 0.0;


/** Memory stats objects 
 *
 * If you add new Stat Memory stats please update:  FMemoryChartEntry so the automated memory chart has the info
 */


DEFINE_STAT(STAT_PhysicalAllocSize);
DEFINE_STAT(STAT_VirtualAllocSize);
DEFINE_STAT(STAT_AudioMemory);
DEFINE_STAT(STAT_TextureMemory);
DEFINE_STAT(STAT_MemoryPhysXTotalAllocationSize);
DEFINE_STAT(STAT_MemoryICUTotalAllocationSize);
DEFINE_STAT(STAT_AnimationMemory);
DEFINE_STAT(STAT_PrecomputedVisibilityMemory);
DEFINE_STAT(STAT_PrecomputedLightVolumeMemory);
DEFINE_STAT(STAT_StaticMeshTotalMemory);
DEFINE_STAT(STAT_SkeletalMeshVertexMemory);
DEFINE_STAT(STAT_SkeletalMeshIndexMemory);
DEFINE_STAT(STAT_SkeletalMeshMotionBlurSkinningMemory);
DEFINE_STAT(STAT_VertexShaderMemory);
DEFINE_STAT(STAT_PixelShaderMemory);
DEFINE_STAT(STAT_NavigationMemory);

DEFINE_STAT(STAT_ReflectionCaptureTextureMemory);
DEFINE_STAT(STAT_ReflectionCaptureMemory);

DEFINE_STAT(STAT_StaticMeshTotalMemory2);
DEFINE_STAT(STAT_StaticMeshVertexMemory);
DEFINE_STAT(STAT_ResourceVertexColorMemory);
DEFINE_STAT(STAT_InstVertexColorMemory);
DEFINE_STAT(STAT_StaticMeshIndexMemory);

DEFINE_STAT(STAT_MallocCalls);
DEFINE_STAT(STAT_ReallocCalls);
DEFINE_STAT(STAT_FreeCalls);
DEFINE_STAT(STAT_TotalAllocatorCalls);

/** Threading stats objects */

DEFINE_STAT(STAT_RenderingIdleTime_WaitingForGPUQuery);
DEFINE_STAT(STAT_RenderingIdleTime_WaitingForGPUPresent);
DEFINE_STAT(STAT_RenderingIdleTime_WaitingForRenderCommands);

DEFINE_STAT(STAT_RenderingIdleTime);
DEFINE_STAT(STAT_RenderingBusyTime);
DEFINE_STAT(STAT_GameIdleTime);
DEFINE_STAT(STAT_GameTickWaitTime);
DEFINE_STAT(STAT_GameTickWantedWaitTime);
DEFINE_STAT(STAT_GameTickAdditionalWaitTime);

DEFINE_STAT(STAT_TaskGraph_OtherTasks);
DEFINE_STAT(STAT_TaskGraph_RenderIdles);

DEFINE_STAT(STAT_TaskGraph_GameTasks);
DEFINE_STAT(STAT_TaskGraph_GameIdles);
DEFINE_STAT(STAT_FlushThreadedLogs);
DEFINE_STAT(STAT_PumpMessages);

DEFINE_STAT(STAT_CPUTimePct);
DEFINE_STAT(STAT_CPUTimePctRelative);

DEFINE_STAT(STAT_AsyncIO_FulfilledReadCount);
DEFINE_STAT(STAT_AsyncIO_FulfilledReadSize);
DEFINE_STAT(STAT_AsyncIO_CanceledReadCount);
DEFINE_STAT(STAT_AsyncIO_CanceledReadSize);
DEFINE_STAT(STAT_AsyncIO_OutstandingReadCount);
DEFINE_STAT(STAT_AsyncIO_OutstandingReadSize);
DEFINE_STAT(STAT_AsyncIO_UncompressorWaitTime);
DEFINE_STAT(STAT_AsyncIO_MainThreadBlockTime);
DEFINE_STAT(STAT_AsyncIO_AsyncPackagePrecacheWaitTime);
DEFINE_STAT(STAT_AsyncIO_Bandwidth);
DEFINE_STAT(STAT_AsyncIO_PlatformReadTime);

DEFINE_LOG_CATEGORY(LogHAL);
DEFINE_LOG_CATEGORY(LogMac);
DEFINE_LOG_CATEGORY(LogIOS);
DEFINE_LOG_CATEGORY(LogAndroid);
DEFINE_LOG_CATEGORY(LogWindows);
DEFINE_LOG_CATEGORY(LogSerialization);
DEFINE_LOG_CATEGORY(LogContentComparisonCommandlet);
DEFINE_LOG_CATEGORY(LogNetPackageMap);
DEFINE_LOG_CATEGORY(LogNetSerialization);

DEFINE_LOG_CATEGORY(LogTemp);

#undef LOCTEXT_NAMESPACE
