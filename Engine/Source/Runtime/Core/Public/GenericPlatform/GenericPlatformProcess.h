// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformProcess.h: Generic platform Process classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

#include "Platform.h"

namespace EProcessResource
{
	enum Type
	{
		/** 
		 * Limits address space - basically limits the largest address the process can get. Affects mmap() (won't be able to map files larger than that) among others.
		 * May also limit automatic stack expansion, depending on platform (e.g. Linux)
		 */
		VirtualMemory
	};
}

struct FBinaryFileVersion
{
	FBinaryFileVersion(int32 InA, int32 InB, int32 InC, int32 InD)
		: A(InA), B(InB), C(InC), D(InD)
	{}

	bool operator==(const FBinaryFileVersion& Other)
	{
		return A == Other.A && B == Other.B && C == Other.C && D == Other.D;
	}

	bool IsValid() const
	{
		return (A | B | C | D) != 0;
	}

	/**
	 * Print as formatted string
	 */
	void ToString(class FString& OutString) const;

private:
	int32 A, B, C, D;
};

/** Generic implementation for the process handle. */
template< typename T, T InvalidHandleValue >
struct TProcHandle
{
	typedef T HandleType;
public:
	/** Default constructor. */
	FORCEINLINE TProcHandle()
		: Handle( InvalidHandleValue )
	{}

	/** Initialization constructor. */
	FORCEINLINE explicit TProcHandle( T Other )
		: Handle( Other )
	{}

	/** Assignment operator. */
	FORCEINLINE TProcHandle& operator=( const TProcHandle& Other )
	{
		if( this != &Other )
		{
			Handle = Other.Handle;
		}
		return *this;
	}

	/** Accessors. */
	FORCEINLINE T Get() const
	{
		return Handle;
	}

	FORCEINLINE void Reset()
	{
		Handle = InvalidHandleValue;
	}

	FORCEINLINE bool IsValid() const
	{
		return Handle != InvalidHandleValue;
	}

	/**
	 * Closes handle and frees this resource to the operating system.
	 * @return true, if this handle was valid before closing it
	 */
	FORCEINLINE bool Close()
	{
		return true;
	}

protected:
	/** Platform specific handle. */
	T Handle;
};

struct FProcHandle;

/**
* Generic implementation for most platforms, these tend to be unused and unimplemented
**/
struct CORE_API FGenericPlatformProcess
{
	/** Load a DLL. **/
	static void* GetDllHandle( const TCHAR* Filename );

	/** Free a DLL. **/
	static void FreeDllHandle( void* DllHandle );

	/** Lookup the address of a DLL function. **/
	static void* GetDllExport( void* DllHandle, const TCHAR* ProcName );

	/** Gets a version number from the specified DLL or EXE **/
	static FBinaryFileVersion GetBinaryFileVersion( const TCHAR* Filename );

	/** Set a directory to look for DLL files. NEEDS to have a Pop call when complete */
	FORCEINLINE static void PushDllDirectory(const TCHAR* Directory)
	{
	
	}

	/** Unsets a directory to look for DLL files. The same directory must be passed in as the Push call to validate */
	FORCEINLINE static void PopDllDirectory(const TCHAR* Directory)
	{

	}

	/** Deletes 1) all temporary files; 2) all cache files that are no longer wanted. **/
	FORCEINLINE static void CleanFileCache()
	{

	}

	/** Retrieves the ProcessId of this process
	* @return the ProcessId of this process
	*/
	static uint32 GetCurrentProcessId();
	/** Get startup directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* BaseDir();
	/** Get user directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* UserDir();
	/** Get the user settings directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR *UserSettingsDir();
	/** Get application settings directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* ApplicationSettingsDir();
	/** Get computer name.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* ComputerName();
	/** Get user name.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* UserName(bool bOnlyAlphaNumeric = true);
	static const TCHAR* ShaderDir();
	static void SetShaderDir(const TCHAR*Where);
	static void SetCurrentWorkingDirectoryToBaseDir()
	{
	}

	/**
	 * Sets the process limits.
	 *
	 * @param Resource one of process resources
	 * @param Limit the maximum amount of the resource (for some OS, this means both hard and soft limits)
	 *
	 * @return true if succeeded
	 */
	static bool SetProcessLimits(EProcessResource::Type Resource, uint64 Limit)
	{
		return true;	// return fake success by default, that way the game won't early quit on platforms that don't implement this
	}

	/**
	 *	Get the shader working directory
	 */
	static const FString ShaderWorkingDir();

	/**
	 *	Clean the shader working directory
	 */
	static void CleanShaderWorkingDir();

	/**
	 * Return the name of the currently running executable
	 *
	 * @param	bRemoveExtension	true to remove the extension of the executable name, false to leave it intact
	 * @return 	Name of the currently running executable
	 */
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);


	/**
	 * Generates the path to the specified application or game.
	 *
	 * The application must reside in the Engine's binaries directory. The returned path is relative to this
	 * executable's directory.For example, calling this method with "UE4" and EBuildConfigurations::Debug
	 * on Windows 64-bit will generate the path "../Win64/UE4Editor-Win64-Debug.exe"
	 *
	 * @param AppName - The name of the application or game.
	 * @param BuildConfiguration - The build configuration of the game.
	 *
	 * @return The generated application path.
	 */
	static FString GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration);

	/**
	 * Return the extension of dynamic library
	 *
	 * @return Extension of dynamic library
	 */
	static const TCHAR* GetModuleExtension();

	/**
	 * Used only by platforms with DLLs, this gives the subdirectory from binaries to find the executables
	 */
	static const TCHAR* GetBinariesSubdirectory();

	/**
	 * Used only by platforms with DLLs, this gives the full path to the main directory containing modules
	 */
	static const FString GetModulesDirectory();
	
	/**
	* Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
	* This is expected to return immediately as the URL is launched by another
	* task.
	**/
	static void LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error );

	/**
	 * Creates a new process and its primary thread. The new process runs the
	 * specified executable file in the security context of the calling process.
	 * @param URL					executable name
	 * @param Parms					command line arguments
	 * @param bLaunchDetached		if true, the new process will have its own window
	 * @param bLaunchHidded			if true, the new process will be minimized in the task bar
	 * @param bLaunchReallyHidden	if true, the new process will not have a window or be in the task bar
	 * @param OutProcessId			if non-NULL, this will be filled in with the ProcessId
	 * @param PriorityModifier		-2 idle, -1 low, 0 normal, 1 high, 2 higher
	 * @param OptionalWorkingDirectory		Directory to start in when running the program, or NULL to use the current working directory
	 * @param PipeWrite				Optional HANDLE to pipe for redirecting output
	 * @return	The process handle for use in other process functions
	 */
	static FProcHandle CreateProc( const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWrite );
	/** Returns true if the specified process is running 
	*
	* @param ProcessHandle handle returned from FPlatformProcess::CreateProc
	* @return true if the process is still running
	*/
	static bool IsProcRunning( FProcHandle & ProcessHandle );
	/** Waits for a process to stop
	*
	* @param ProcessHandle handle returned from FPlatformProcess::CreateProc
	*/
	static void WaitForProc( FProcHandle & ProcessHandle );
	/** Terminates a process
	*
	* @param ProcessHandle handle returned from FPlatformProcess::CreateProc
	* @param KillTree Whether the entire process tree should be terminated.
	*/
	static void TerminateProc( FProcHandle & ProcessHandle, bool KillTree = false );
	/** Retrieves the termination status of the specified process. **/
	static bool GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode );
	/** Returns true if the specified application is running */
	static bool IsApplicationRunning( uint32 ProcessId );
	/** Returns true if the specified application is running */
	static bool IsApplicationRunning( const TCHAR* ProcName );
	/** Returns true if the specified application has a visible window, and that window is active/has focus/is selected */
	static bool IsThisApplicationForeground();

	/**
	 * Executes a process, returning the return code, stdout, and stderr. This
	 * call blocks until the process has returned.
	 */
	static void ExecProcess( const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr );

	/**
	 * Attempt to launch the provided file name in its default external application. Similar to FPlatformProcess::LaunchURL,
	 * with the exception that if a default application isn't found for the file, the user will be prompted with
	 * an "Open With..." dialog.
	 *
	 * @param	FileName	Name of the file to attempt to launch in its default external application
	 * @param	Parms		Optional parameters to the default application
	 */
	static void LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms = NULL );

	/**
	 * Attempt to "explore" the folder specified by the provided file path
	 *
	 * @param	FilePath	File path specifying a folder to explore
	 */
	static void ExploreFolder( const TCHAR* FilePath );

#if PLATFORM_HAS_BSD_TIME 

	/** Sleep this thread for Seconds.  0.0 means release the current timeslice to let other threads get some attention. */
	static void Sleep( float Seconds );
	/** Sleep this thread infinitely. */
	static void SleepInfinite();

#endif // PLATFORM_HAS_BSD_TIME

	/**
	 * Creates a new event
	 *
	 * @param bIsManualReset Whether the event requires manual reseting or not
	 * @param InName Whether to use a commonly shared event or not. If so this
	 * is the name of the event to share.
	 *
	 * @return Returns the new event object if successful, NULL otherwise
	 */
	static class FEvent* CreateSynchEvent(bool bIsManualReset = 0);

	/**
	 * Creates the platform-specific runnable thread. This should only be called from FRunnableThread::Create
	 * @return The newly created thread
	 */
	static class FRunnableThread* CreateRunnableThread();

	/**
	 * Closes an anonymous pipe.
	 *
	 * @param ReadPipe - The handle to the read end of the pipe.
	 * @param WritePipe - The handle to the write end of the pipe.
	 *
	 * @see CreatePipe
	 * @see ReadPipe
	 */
	static void ClosePipe( void* ReadPipe, void* WritePipe );

	/**
	 * Creates a writable anonymous pipe.
	 *
	 * Anonymous pipes can be used to capture and/or redirect STDOUT and STDERROR of a process.
	 * The pipe created by this method can be passed into CreateProc as Write
	 *
	 * @param ReadPipe - Will hold the handle to the read end of the pipe.
	 * @param WritePipe - Will hold the handle to the write end of the pipe.
	 *
	 * @return true on success, false otherwise.
	 *
	 * @see ClosePipe
	 * @see ReadPipe
	 */
	static bool CreatePipe( void*& ReadPipe, void*& WritePipe );

	/**
	 * Reads all pending data from an anonymous pipe, such as STDOUT or STDERROR of a process.
	 *
	 * @param Pipe - The handle to the pipe to read from.
	 *
	 * @return A string containing the read data.
	 *
	 * @see ClosePipe
	 * @see CreatePipe
	 */
	static FString ReadPipe( void* ReadPipe );

	/**
	 * Gets whether this platform can use multiple threads.
	 *
	 * @return true if the platform can use multiple threads, false otherwise.
	 */
	static bool SupportsMultithreading();
    
    /**
     * Enables Real Time Mode on the current thread
     *
     */
    static void SetRealTimeMode()
    {
    }
};


#if PLATFORM_USE_PTHREADS
	// if we are using pthreads, we can set this up at the generic layer
	#include "PThreadCriticalSection.h"
	typedef FPThreadsCriticalSection FCriticalSection;
#endif
