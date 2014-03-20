// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once 

/** 
 * Debug state of the audio system
 */
enum EDebugState
{
	// No debug sounds
	DEBUGSTATE_None,
	// No reverb sounds
	DEBUGSTATE_IsolateDryAudio,
	// Only reverb sounds
	DEBUGSTATE_IsolateReverb,
	// Force LPF on all sources
	DEBUGSTATE_TestLPF,
	// Bleed stereo sounds fully to the rear speakers
	DEBUGSTATE_TestStereoBleed,
	// Bleed all sounds to the LFE speaker
	DEBUGSTATE_TestLFEBleed,
	// Disable any LPF filter effects
	DEBUGSTATE_DisableLPF,
	// Disable any radio filter effects
	DEBUGSTATE_DisableRadio,
	DEBUGSTATE_MAX,
};

/**
 * Current state of a SoundMix
 */
namespace ESoundMixState
{
	enum Type
	{
		// Waiting to fade in
		Inactive,
		// Fading in
		FadingIn,
		// Fully active
		Active,
		// Fading out
		FadingOut,
		// Time elapsed, just about to be removed
		AwaitingRemoval,
	};
}

namespace ESortedActiveWaveGetType
{
	enum Type
	{
		FullUpdate,
		PausedUpdate,
		QueryOnly,
	};
}

/** 
 * Defines the properties of the listener
 */
struct FListener
{
	FTransform Transform;
	FVector Velocity;

	struct FInteriorSettings InteriorSettings;

	/** The volume the listener resides in */
	class AReverbVolume* Volume;

	/** The times of interior volumes fading in and out */
	double InteriorStartTime;
	double InteriorEndTime;
	double ExteriorEndTime;
	double InteriorLPFEndTime;
	double ExteriorLPFEndTime;
	float InteriorVolumeInterp;
	float InteriorLPFInterp;
	float ExteriorVolumeInterp;
	float ExteriorLPFInterp;

	FVector GetUp() const		{ return Transform.GetUnitAxis(EAxis::Z); }
	FVector GetFront() const	{ return Transform.GetUnitAxis(EAxis::Y); }
	FVector GetRight() const	{ return Transform.GetUnitAxis(EAxis::X); }

	/**
	 * Works out the interp value between source and end
	 */
	float Interpolate( const double EndTime );

	/**
	 * Gets the current state of the interior settings for the listener
	 */
	void UpdateCurrentInteriorSettings();

	/** 
	 * Apply the interior settings to ambient sounds
	 */
	void ApplyInteriorSettings( class AReverbVolume* Volume, const FInteriorSettings& Settings );

	FListener()
		: Transform(FTransform::Identity)
		, Velocity(ForceInit)
		, Volume(NULL)
		, InteriorStartTime(0.0)
		, InteriorEndTime(0.0)
		, ExteriorEndTime(0.0)
		, InteriorLPFEndTime(0.0)
		, ExteriorLPFEndTime(0.0)
		, InteriorVolumeInterp(0.f)
		, InteriorLPFInterp(0.f)
		, ExteriorVolumeInterp(0.f)
		, ExteriorLPFInterp(0.f)
	{
	}
};

/** 
 * Structure for collating info about sound classes
 */
struct FAudioClassInfo
{
	int32 NumResident;
	int32 SizeResident;
	int32 NumRealTime;
	int32 SizeRealTime;

	FAudioClassInfo()
		: NumResident(0)
		, SizeResident(0)
		, NumRealTime(0)
		, SizeRealTime(0)
	{
	}
};

struct FSoundMixState
{
	bool IsBaseSoundMix;
	uint32 ActiveRefCount;
	uint32 PassiveRefCount;
	double StartTime;
	double FadeInStartTime;
	double FadeInEndTime;
	double FadeOutStartTime;
	double EndTime;
	float InterpValue;
	ESoundMixState::Type CurrentState;
};

struct FActivatedReverb
{
	FReverbSettings ReverbSettings;
	float Priority;

	FActivatedReverb()
		: Priority(0.f)
	{
	}
};

class ENGINE_API FAudioDevice : public FExec
{
public:

	//Begin FExec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar = *GLog ) OVERRIDE;
	//End FExec Interface

#if !UE_BUILD_SHIPPING
	/**
	 * Exec command handlers
	 */
	bool HandleDumpSoundInfoCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	/**
	 * Lists all the loaded sounds and their memory footprint
	 */
	bool HandleListSoundsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	/**
	 * Lists all the playing waveinstances and their associated source
	 */
	bool HandleListWavesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	/**
	 * Lists a summary of loaded sound collated by class
	 */
	bool HandleListSoundClassesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	/**
	 * shows sound class hierarchy
	 */
	bool HandleShowSoundClassHierarchyCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListSoundClassVolumesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListAudioComponentsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListSoundDurationsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleSoundTemplateInfoCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandlePlaySoundCueCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandlePlaySoundWaveCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleSetBaseSoundMixCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleIsolateDryAudioCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleIsolateReverbCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTestLPFCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTestStereoBleedCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTestLFEBleedCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisableLPFCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisableRadioCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleEnableRadioCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleResetSoundStateCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleModifySoundClassCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif

	/**
	 * Constructor
	 */
	FAudioDevice();

	virtual ~FAudioDevice()
	{
	}

	/**
	 * Basic initialisation of the platform agnostic layer of the audio system
	 */
	bool Init( void );

	/**
	 * Tears down the audio device
	 */
	void Teardown();

	/**
	 * The audio system's main "Tick" function
	 */
	void Update(bool bGameTicking);

	/**
	 * Counts the bytes for the structures used in this class
	 */
	virtual void CountBytes(FArchive& Ar);

	/**
	 * Track references to UObjects
	 */
	void AddReferencedObjects(FReferenceCollector& Collector);

	/**
	 * Iterate over the active AudioComponents for wave instances that could be playing.
	 *
	 * @return Index of first wave instance that can have a source attached
	 */
	int32 GetSortedActiveWaveInstances(TArray<FWaveInstance*>& WaveInstances, const ESortedActiveWaveGetType::Type GetType);

	/**
	 * Stop all the audio components and sources attached to the world. NULL world means all components.
	 */
	void Flush( class UWorld* WorldToFlush, bool bClearActivatedReverb = true );

#if WITH_EDITOR
	/* Stop any playing sounds so that we can reimport a specific sound wave */
	void StopSoundsForReimport(USoundWave* ReimportedSoundWave, TArray<UAudioComponent*>& ComponentsToRestart);
#endif

	/**
	 * Precaches the passed in sound node wave object.
	 *
	 * @param	SoundWave		Resource to be precached.
	 * @param	bSynchronous	If true, this function will block until a vorbis decompression is complete
	 * @param	bTrackMemory	If true, the audio mem stats will be updated
	 */
	virtual void Precache(USoundWave* SoundWave, bool bSynchronous=false, bool bTrackMemory=true);

	/**
	 * Precaches all existing sounds. Called when audio setup is complete
	 */
	virtual void PrecacheStartupSounds();

	/** 
	 * Sets the maximum number of channels dynamically. Can't raise the cap over the initial value but can lower it 
	 */
	virtual void SetMaxChannels(int32 InMaxChannels);

	/** 
	 * Links up the resource data indices for looking up and cleaning up
	 */
	void TrackResource(USoundWave* Wave, FSoundBuffer* Buffer);

	/**
	 * Frees the bulk resource data assocated with this SoundWave.
	 *
	 * @param	SoundWave	wave object to free associated bulk data
	 */
	void FreeResource(USoundWave* SoundWave);

	/**
	 * Frees the resources associated with this buffer
	 *
	 * @param	FSoundBuffer	Buffer to clean up
	 */
	void FreeBufferResource(FSoundBuffer* Buffer);

	/**
	 * Stops all game sounds (and possibly UI) sounds
	 *
	 * @param bShouldStopUISounds If true, this function will stop UI sounds as well
	 */
	virtual void StopAllSounds( bool bShouldStopUISounds = false );

	/**
	 * Sets the details about the listener
	 * @param   ListenerIndex		The index of the listener
	 * @param   ListenerTransform   The listener's world transform
	 * @param   DeltaSeconds		The amount of time over which velocity should be calculated.  If 0, then velocity will not be calculated.
	 * @param   Volume				The reverb volume this listener is in
	 * @param	ReverbSettings		The reverb settings for this user to use.
	 * @param	InteriorSettings	The interior settings for this user to use.
	 */
	void SetListener( const int32 InListenerIndex, const FTransform& ListenerTransform, const float InDeltaSeconds, class AReverbVolume* Volume, const FInteriorSettings& InteriorSettings );

	/**
	 * Starts a transition to new reverb and interior settings
	 *
	 */
	void SetReverbSettings( class AReverbVolume* Volume, const FReverbSettings& ReverbSettings );

	/**
	 * Creates an audio component to handle playing a sound cue
	 */
	static class UAudioComponent* CreateComponent( class USoundBase* Sound, class UWorld* World, AActor*  AActor  = NULL, bool Play = true, bool bStopWhenOwnerDestroyed = false, const FVector* Location = NULL );

	/**
	 * Adds an active sound to the audio device
	 */
	void AddNewActiveSound( const FActiveSound& ActiveSound );

	/**
	 * Removes the active sound for the specified audio component
	 */
	void StopActiveSound( class UAudioComponent* AudioComponent );

	/**
	 * Finds the active sound for the specified audio component
	 */
	FActiveSound* FindActiveSound( class UAudioComponent* AudioComponent );

	/**
	 * Removes an active sound from the active sounds array
	 */
	void RemoveActiveSound( FActiveSound* ActiveSound );

	/** 
	 * Gets the current audio debug state
	 */
	EDebugState GetMixDebugState( void );

	/**
	 * Set up the sound class hierarchy
	 */
	void InitSoundClasses( void );

	/**
	 * Set up the initial sound sources
	 * Allows us to initialize sound source early on, allowing for render callback hookups for iOS Audio.
	 */
	void InitSoundSources( void );

	/** 
	 * Gets a summary of loaded sound collated by class
	 */
	void GetSoundClassInfo( TMap<FName, FAudioClassInfo>& AudioClassInfos );

	/**
	 * Returns the properties of the requested sound class modified to reflect the current state of the mix system 
	 *
	 * @param	SoundClassName	name of sound class to retrieve
	 * @return	sound class properties if it exists
	 */
	FSoundClassProperties* GetSoundClassCurrentProperties( class USoundClass* InSoundClass );

	/**
	 * Updates sound class volumes
	 */
	void SetClassVolume( class USoundClass* InSoundClass, const float Volume );

	/**
	 * Checks to see if a coordinate is within a distance of any listener
	 */
	bool LocationIsAudible( FVector Location, float MaxDistance );

	/**
	 * Removes a sound class
	 */
	void RemoveClass( class USoundClass* SoundClass );

	/**
	 * Sets the Sound Mix that should be active by default
	 */
	void SetDefaultBaseSoundMix( class USoundMix* SoundMix );

	/**
	 * Removes a sound mix - called when SoundMix is unloaded
	 */
	void RemoveSoundMix( class USoundMix* SoundMix );

	/** 
	 * Resets all interpolating values to defaults.
	 */
	void ResetInterpolation( void );

	/** Enables or Disables the radio effect. */
	void EnableRadioEffect( bool bEnable = false );

	friend class FAudioEffectsManager;
	/**
	 * Sets a new sound mix and applies it to all appropriate sound classes
	 */
	bool SetBaseSoundMix( class USoundMix* SoundMix );

	/**
	 * Push a SoundMix onto the Audio Device's list.
	 *
	 * @param SoundMix The SoundMix to push.
	 * @param bIsPassive Whether this is a passive push from a playing sound.
	 */
	void PushSoundMixModifier(class USoundMix* SoundMix, bool bIsPassive = false);

	/**
	 * Pop a SoundMix from the Audio Device's list.
	 *
	 * @param SoundMix The SoundMix to pop.
	 * @param bIsPassive Whether this is a passive pop from a sound finishing.
	 */
	void PopSoundMixModifier(class USoundMix* SoundMix, bool bIsPassive = false);

	/**
	 * Clear the effect of one SoundMix completely.
	 *
	 * @param SoundMix The SoundMix to clear.
	 */
	void ClearSoundMixModifier(class USoundMix* SoundMix);

	/**
	 * Clear the effect of all SoundMix modifiers.
	 */
	void ClearSoundMixModifiers();

	/** Activates a Reverb Effect without the need for a volume
	 * @param ReverbEffect Reverb Effect to use
	 * @param TagName Tag to associate with Reverb Effect
	 * @param Priority Priority of the Reverb Effect
	 * @param Volume Volume level of Reverb Effect
	 * @param FadeTime Time before Reverb Effect is fully active
	 */
	void ActivateReverbEffect(class UReverbEffect* ReverbEffect, FName TagName, float Priority, float Volume, float FadeTime);
	
	/**
	 * Deactivates a Reverb Effect not applied by a volume
	 *
	 * @param TagName Tag associated with Reverb Effect to remove
	 */
	void DeactivateReverbEffect(FName TagName);

	virtual FName GetRuntimeFormat() PURE_VIRTUAL(FAudioDevice::GetRuntimeFormat,return NAME_None;);

	/**
	 * Check for errors and output a human readable string
	 */
	virtual bool ValidateAPICall( const TCHAR* Function, int32 ErrorCode )
	{
		return( true );
	}

	const TArray<FActiveSound*>& GetActiveSounds() const { return ActiveSounds; }

	/* When the set of Reverb volumes have changed invalidate the cached values of active sounds */
	void InvalidateCachedInteriorVolumes() const;

protected:
	friend class FSoundSource;

	/**
	 * Handle pausing/unpausing of sources when entering or leaving pause mode
	 */
	void HandlePause( bool bGameTicking );

	/**
	 * Stop sources that need to be stopped, and touch the ones that need to be kept alive
	 * Stop sounds that are too low in priority to be played
	 */
	void StopSources( TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex );

	/**
	 * Start and/or update any sources that have a high enough priority to play
	 */
	void StartSources( TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex, bool bGameTicking );

	

	/**
	 * Sets the 'pause' state of sounds which are always loaded.
	 *
	 * @param	bPaused			Pause sounds if true, play paused sounds if false.
	 */
	virtual void PauseAlwaysLoadedSounds(bool bPaused)
	{
	}

	/**
	 * Lists a summary of loaded sound collated by class
	 */
	void ShowSoundClassHierarchy( FOutputDevice& Ar, class USoundClass* SoundClass = NULL, int32 Indent = 0 ) const;
	
	/**
	 * Parses the sound classes and propagates multiplicative properties down the tree.
	 */
	void ParseSoundClasses();

	/**
	 * Construct the CurrentSoundClassProperties map
	 *
	 * This contains the original sound class properties propagated properly, and all adjustments due to the sound mixes
	 */
	void UpdateSoundClassProperties();

	/**
	 * Set the mix for altering sound class properties
	 *
	 * @param NewMix The SoundMix to apply
	 * @param SoundMixState The State associated with this SoundMix
	 */
	bool ApplySoundMix( class USoundMix* NewMix, FSoundMixState* SoundMixState );

	/**
	 * Updates the state of a sound mix if it is pushed more than once.
	 *
	 * @param SoundMix The SoundMix we are updating
	 * @param SoundMixState The State associated with this SoundMix
	 */
	void UpdateSoundMix(class USoundMix* SoundMix, FSoundMixState* SoundMixState);

	/**
	 * Updates list of SoundMixes that are applied passively, pushing and popping those that change
	 *
	 * @param WaveInstances Sorted list of active wave instances
	 * @param FirstActiveIndex Index of first wave instance that will be played.
	 */
	void UpdatePassiveSoundMixModifiers(TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex);

	/**
	 * Attempt to clear the effect of a particular SoundMix
	 *
	 * @param SoundMix The SoundMix we're attempting to clear
	 * @param SoundMixState The current state of this SoundMix
	 *
	 * @return Whether this SoundMix could be cleared (only true when both ref counts are zero).
	 */
	bool TryClearingSoundMix(class USoundMix* SoundMix, FSoundMixState* SoundMixState);

	/**
	 * Attempt to remove this SoundMix's EQ effect - it may not currently be active
	 *
	 * @param SoundMix The SoundMix we're attempting to clear
	 *
	 * @return Whether the effect of this SoundMix was cleared
	 */
	bool TryClearingEQSoundMix(class USoundMix* SoundMix);

	/**
	 * Find the SoundMix with the next highest EQ priority to the one passed in
	 *
	 * @param SoundMix The highest priority SoundMix, which will be ignored
	 *
	 * @return The next highest priority SoundMix or NULL if one cannot be found
	 */
	class USoundMix* FindNextHighestEQPrioritySoundMix(class USoundMix* IgnoredSoundMix);

	/**
	 * Clear the effect of a SoundMix completely - only called after checking it's safe to
	 */
	void ClearSoundMix(class USoundMix* SoundMix);

	/**
	 * Sets the sound class adjusters from a SoundMix.
	 *
	 * @param SoundMix		The SoundMix to apply adjusters from
	 * @param InterpValue	Proportion of adjuster to apply
	 */
	void ApplyClassAdjusters(class USoundMix* SoundMix, float InterpValue);

	/**
	 * Recursively apply an adjuster to the passed in sound class and all children of the sound class
	 * 
	 * @param InAdjuster		The adjuster to apply
	 * @param InSoundClassName	The name of the sound class to apply the adjuster to.  Also applies to all children of this class
	 */
	void RecursiveApplyAdjuster( const struct FSoundClassAdjuster& InAdjuster, USoundClass* InSoundClass );

	/**
	 * Takes an adjuster value and modifies it by the proportion that is currently in effect
	 */
	float InterpolateAdjuster(const float Adjuster, const float InterpValue) const
	{
		return Adjuster * InterpValue + 1.0f - InterpValue;
	}

	/**
	 * Platform dependent call to init effect data on a sound source
	 */
	void* InitEffect( class FSoundSource* Source );

	/**
	 * Platform dependent call to update the sound output with new parameters
	 * The audio system's main "Tick" function
	 */
	void* UpdateEffect( class FSoundSource* Source );

	/**
	 * Platform dependent call to destroy any effect related data
	 */
	void DestroyEffect( class FSoundSource* Source );

	/**
	 * Return the pointer to the sound effects handler
	 */
	class FAudioEffectsManager* GetEffects( void )
	{
		return( Effects );
	}

	/** Internal */
	void SortWaveInstances( int32 MaxChannels );

	/**
	 * Internal helper function used by ParseSoundClasses to traverse the tree.
	 *
	 * @param CurrentClass			Subtree to deal with
	 * @param ParentProperties		Propagated properties of parent node
	 */
	void RecurseIntoSoundClasses( class USoundClass* CurrentClass, FSoundClassProperties& ParentProperties );

	/**
	 * Find the current highest priority reverb after a change to the list of active ones.
	 */
	void UpdateHighestPriorityReverb();


// If we make FAudioDevice not be subclassable, then all the functions following would move to IAudioDeviceModule

	/** Starts up any platform specific hardware/APIs */
	virtual bool InitializeHardware()
	{
		return true;
	}

	/** Shuts down any platform specific hardware/APIs */
	virtual void TeardownHardware()
	{
	}

	/** Lets the platform any tick actions */
	virtual void UpdateHardware()
	{
	}

	/** Creates a new platform specific sound source */
	virtual class FAudioEffectsManager* CreateEffectsManager();

	/** Creates a new platform specific sound source */
	virtual class FSoundSource* CreateSoundSource() PURE_VIRTUAL(FAudioDevice::CreateSoundSource,return NULL;);

public:
	/** The maximum number of concurrent audible sounds */
	int32 MaxChannels;

	/** The amount of memory to reserve for always resident sounds */
	int32 CommonAudioPoolSize;

	/** Low pass filter OneOverQ value */
	float LowPassFilterResonance;

	/** Pointer to permanent memory allocation stack. */
	void* CommonAudioPool;

	/** Available size in permanent memory stack */
	int32 CommonAudioPoolFreeBytes;

	uint32 bGameWasTicking:1;

	/* HACK: Temporarily disable audio caching.  This will be done better by changing the decompression pool size in the future */
	uint32 bDisableAudioCaching:1;

	/* True once the startup sounds have been precached */
	uint32 bStartupSoundsPreCached:1;

	TArray<struct FListener> Listeners;

	uint64 CurrentTick;

	/** An AudioComponent to play test sounds on */
	class UAudioComponent* TestAudioComponent;

	/** The debug state of the audio device */
	TEnumAsByte<enum EDebugState> DebugState;

	/** transient master volume multiplier that can be modified at runtime without affecting user settings automatically reset to 1.0 on level change */
	float TransientMasterVolume;

	/** Timestamp of the last update */
	float LastUpdateTime;

	/** Next resource ID to assign out to a wave/buffer */
	int32 NextResourceID;

	/** Set of sources used to play sounds (platform will subclass these) */
	TArray<class FSoundSource*>				Sources;
	TArray<class FSoundSource*>				FreeSources;
	TMap<struct FWaveInstance*, class FSoundSource*>	WaveInstanceSourceMap;

	/** Array of all created buffers associated with this audio device */
	TArray<class FSoundBuffer*>				Buffers;
	/** Look up associating a USoundWave's resource ID with low level sound buffers	*/
	TMap<int32, class FSoundBuffer*>			WaveBufferMap;

	/** Current properties of all sound classes */
	TMap<class USoundClass*, FSoundClassProperties>	SoundClasses;

	/** The Base SoundMix that's currently active */
	class USoundMix* BaseSoundMix;

	/** The Base SoundMix that should be applied by default */
	class USoundMix* DefaultBaseSoundMix;

	/** Map of sound mixes currently affecting audio properties */
	TMap<class USoundMix*, FSoundMixState>		SoundMixModifiers;

	/** Interface to audio effects processing */
	class FAudioEffectsManager*						Effects;

	/** The volume the listener resides in */
	const class AReverbVolume*						CurrentReverbVolume;

	/** Reverb Effects activated without volumes */
	TMap<FName, FActivatedReverb>					ActivatedReverbs;

	/** The activated reverb that currently has the highest priority */
	const FActivatedReverb*								HighestPriorityReverb;

private:

	TArray<struct FActiveSound*> ActiveSounds;

	/** List of passive SoundMixes active last frame */
	TArray<class USoundMix*> PrevPassiveSoundMixModifiers;
};


/**
 * Interface for audio device modules
 */

/** Defines the interface of a module implementing an audio device and associated classes. */
class IAudioDeviceModule : public IModuleInterface
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual class FAudioDevice* CreateAudioDevice() = 0;
};
