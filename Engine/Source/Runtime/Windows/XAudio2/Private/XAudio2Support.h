// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	XAudio2Support.h: XAudio2 specific structures.
=============================================================================*/

#pragma once

#ifndef XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	#define XAUDIO_SUPPORTS_XMA2WAVEFORMATEX	1
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
#ifndef XAUDIO_SUPPORTS_DEVICE_DETAILS
	#define XAUDIO_SUPPORTS_DEVICE_DETAILS		1
#endif	//XAUDIO_SUPPORTS_DEVICE_DETAILS
#ifndef XAUDIO2_SUPPORTS_MUSIC
	#define XAUDIO2_SUPPORTS_MUSIC				1
#endif	//XAUDIO2_SUPPORTS_MUSIC
#ifndef X3DAUDIO_VECTOR_IS_A_D3DVECTOR
	#define X3DAUDIO_VECTOR_IS_A_D3DVECTOR		1
#endif	//X3DAUDIO_VECTOR_IS_A_D3DVECTOR
#ifndef XAUDIO2_SUPPORTS_SENDLIST
	#define XAUDIO2_SUPPORTS_SENDLIST			1
#endif	//XAUDIO2_SUPPORTS_SENDLIST

/*------------------------------------------------------------------------------------
	XAudio2 system headers
------------------------------------------------------------------------------------*/
#include "Engine.h"
#include "SoundDefinitions.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "AllowWindowsPlatformTypes.h"
	#include <xaudio2.h>
	#include <X3Daudio.h>
#include "HideWindowsPlatformTypes.h"

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

#define AUDIO_HWTHREAD			XAUDIO2_DEFAULT_PROCESSOR

#define SPEAKER_5POINT0          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT )
#define SPEAKER_6POINT1          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT | SPEAKER_BACK_CENTER )

struct FPCMBufferInfo
{
	/** Format of the source PCM data */
	WAVEFORMATEX				PCMFormat;
	/** Address of PCM data in physical memory */
	uint8*						PCMData;
	/** Size of PCM data in physical memory */
	UINT32						PCMDataSize;
};

#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
struct FXMA2BufferInfo
{
	/** Format of the source XMA2 data */
	XMA2WAVEFORMATEX			XMA2Format;
	/** Address of XMA2 data in physical memory */
	uint8*						XMA2Data;
	/** Size of XMA2 data in physical memory */
	UINT32						XMA2DataSize;
};
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX

struct FXWMABufferInfo
{
	/** Format of the source XWMA data */
	WAVEFORMATEXTENSIBLE		XWMAFormat;
	/** Additional info required for xwma */
	XAUDIO2_BUFFER_WMA			XWMABufferData;
	/** Address of XWMA data in physical memory */
	uint8*						XWMAData;
	/** Size of XWMA data in physical memory */
	UINT32						XWMADataSize;
	/** Address of XWMA seek data in physical memory */
	UINT32*						XWMASeekData;
	/** Size of XWMA seek data */
	UINT32						XWMASeekDataSize;
};

/**
 * XAudio2 implementation of FSoundBuffer, containing the wave data and format information.
 */
class FXAudio2SoundBuffer : public FSoundBuffer
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FXAudio2SoundBuffer(FAudioDevice* AudioDevice, ESoundFormat SoundFormat);
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
	 */
	~FXAudio2SoundBuffer( void );

	/** 
	 * Set up this buffer to contain and play XMA2 data
	 */
	void InitXMA2( FXAudio2Device* XAudio2Device, USoundWave* Wave, struct FXMAInfo* XMAInfo );

	/** 
	 * Set up this buffer to contain and play XWMA data
	 */
	void InitXWMA( USoundWave* Wave, struct FXMAInfo* XMAInfo );

	/** 
	 * Setup a WAVEFORMATEX structure
	 */
	void InitWaveFormatEx( uint16 Format, USoundWave* Wave, bool bCheckPCMData );

	/**
	 * Decompresses a chunk of compressed audio to the destination memory
	 *
	 * @param Destination		Memory to decompress to
	 * @param bLooping			Whether to loop the sound seamlessly, or pad with zeroes
	 * @return					Whether the sound looped or not
	 */
	bool ReadCompressedData( uint8* Destination, bool bLooping );

	/**
	 * Sets the point in time within the buffer to the specified time
	 * If the time specified is beyond the end of the sound, it will be set to the end
	 *
	 * @param SeekTime		Time in seconds from the beginning of sound to seek to
	 */
	void Seek( const float SeekTime );

	/**
	 * Static function used to create an OpenAL buffer and dynamically upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateQueuedBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave );

	/**
	 * Static function used to create an OpenAL buffer and dynamically upload procedural data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateProceduralBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave );

	/**
	 * Static function used to create an OpenAL buffer and upload raw PCM data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreatePreviewBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave, FXAudio2SoundBuffer* Buffer );

	/**
	 * Static function used to create an OpenAL buffer and upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateNativeBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave );

	/**
	 * Static function used to create a buffer.
	 *
	 * @param InWave USoundWave to use as template and wave source
	 * @param AudioDevice audio device to attach created buffer to
	 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* Init( FAudioDevice* AudioDevice, USoundWave* InWave, bool bForceRealtime );

	/**
	 * Returns the size of this buffer in bytes.
	 *
	 * @return Size in bytes
	 */
	int32 GetSize( void );

	/** Audio device this buffer is attached to	*/
	FAudioDevice*				AudioDevice;

	/** Format of the sound referenced by this buffer */
	int32							SoundFormat;

	union
	{
		FPCMBufferInfo			PCM;		
#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		FXMA2BufferInfo			XMA2;			// Xenon only
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		FXWMABufferInfo			XWMA;			// Xenon only
	};

	/** Wrapper to handle the decompression of vorbis code */
	class FVorbisAudioInfo*		DecompressionState;
	/** Set to true when the PCM data should be freed when the buffer is destroyed */
	bool						bDynamicResource;
};

/**
 * Source callback class for handling loops
 */
class FXAudio2SoundSourceCallback : public IXAudio2VoiceCallback
{
public:
	FXAudio2SoundSourceCallback( void )
	{
	}

	virtual ~FXAudio2SoundSourceCallback( void )
	{ 
	}

	virtual void STDCALL OnStreamEnd( void ) 
	{ 
	}

	virtual void STDCALL OnVoiceProcessingPassEnd( void ) 
	{
	}

	virtual void STDCALL OnVoiceProcessingPassStart( UINT32 SamplesRequired )
	{
	}

	virtual void STDCALL OnBufferEnd( void* BufferContext )
	{
	}

	virtual void STDCALL OnBufferStart( void* BufferContext )
	{
	}

	virtual void STDCALL OnLoopEnd( void* BufferContext );

	virtual void STDCALL OnVoiceError( void* BufferContext, HRESULT Error )
	{
	}

	friend class FXAudio2SoundSource;
};

/**
 * XAudio2 implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FXAudio2SoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FXAudio2SoundSource(FAudioDevice* InAudioDevice);

	/**
	 * Destructor, cleaning up voice
	 */
	virtual ~FXAudio2SoundSource( void );

	/**
	 * Frees existing resources. Called from destructor and therefore not virtual.
	 */
	void FreeResources( void );

	/**
	 * Initializes a source with a given wave instance and prepares it for playback.
	 *
	 * @param	WaveInstance	wave instance being primed for playback
	 * @return	true if initialization was successful, false otherwise
	 */
	virtual bool Init( FWaveInstance* WaveInstance );

	/**
	 * Updates the source specific parameter like e.g. volume and pitch based on the associated
	 * wave instance.	
	 */
	virtual void Update( void );

	/**
	 * Plays the current wave instance.	
	 */
	virtual void Play( void );

	/**
	 * Stops the current wave instance and detaches it from the source.	
	 */
	virtual void Stop( void );

	/**
	 * Pauses playback of current wave instance.
	 */
	virtual void Pause( void );

	/**
	 * Handles feeding new data to a real time decompressed sound
	 */
	void HandleRealTimeSource( void );

	/**
	 * Queries the status of the currently associated wave instance.
	 *
	 * @return	true if the wave instance/ source has finished playback and false if it is 
	 *			currently playing or paused.
	 */
	virtual bool IsFinished( void );

	/**
	 * Create a new source voice
	 */
	bool CreateSource( void );

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMBuffers( void );

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMRTBuffers( void );

	/** 
	 * Submit the relevant audio buffers to the system, accounting for looping modes
	 */
	void SubmitXMA2Buffers( void );

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitXWMABuffers( void );

	/**
	 * Calculates the volume for each channel
	 */
	void GetChannelVolumes( float ChannelVolumes[CHANNELOUT_COUNT], float AttenuatedVolume );

	/**
	 * Returns a string describing the source
	 */
	virtual FString Describe(bool bUseLongName);

	/** 
	 * Maps a sound with a given number of channels to to expected speakers
	 */
	void RouteDryToSpeakers( float ChannelVolumes[CHANNELOUT_COUNT] );

	/** 
	 * Maps the sound to the relevant reverb effect
	 */
	void RouteToReverb( float ChannelVolumes[CHANNELOUT_COUNT] );

	/** 
	 * Maps the sound to the relevant radio effect.
	 *
	 * @param	ChannelVolumes	The volumes associated to each channel. 
	 *							Note: Not all channels are mapped directly to a speaker.
	 */
	void RouteToRadio( float ChannelVolumes[CHANNELOUT_COUNT] );

protected:
	/** Decompress through XAudio2Buffer, or call USoundWave procedure to generate more PCM data. Returns true/false: did audio loop? */
	bool ReadMorePCMData(const int32 BufferIndex);

	/** Handle obtaining more data for procedural USoundWaves. Always returns false for convenience. */
	bool ReadProceduralData(const int32 BufferIndex);

	/**
	 * Utility function for determining the proper index of an effect. Certain effects (such as: reverb and radio distortion) 
	 * are optional. Thus, they may be NULL, yet XAudio2 cannot have a NULL output voice in the send list for this source voice.
	 *
	 * @return	The index of the destination XAudio2 submix voice for the given effect; -1 if effect not in destination array. 
	 *
	 * @param	Effect	The effect type's (Reverb, Radio Distoriton, etc) index to find. 
	 */
	int32 GetDestinationVoiceIndexForEffect( SourceDestinations Effect );

	/** Owning classes */
	FXAudio2Device*				AudioDevice;
	FXAudio2EffectsManager*		Effects;

	/** Cached subclass version of Buffer (which the base class has) */
	FXAudio2SoundBuffer*		XAudio2Buffer;
	/** XAudio2 source voice associated with this source. */
	IXAudio2SourceVoice*		Source;
	/** Structure to handle looping sound callbacks */
	FXAudio2SoundSourceCallback	SourceCallback;
	/** Destination voices */
	XAUDIO2_SEND_DESCRIPTOR		Destinations[DEST_COUNT];
	/** Which sound buffer should be written to next - used for double buffering. */
	int32							CurrentBuffer;
	/** A pair of sound buffers to allow notification when a sound loops. */
	XAUDIO2_BUFFER				XAudio2Buffers[2];
	/** Additional buffer info for XWMA sounds */
	XAUDIO2_BUFFER_WMA			XAudio2BufferXWMA[1];
	/** Set when we wish to let the buffers play themselves out */
	uint32						bBuffersToFlush:1;
	/** Set to true when the loop end callback is hit */
	uint32						bLoopCallback:1;
	/** Set to true when we've allocated resources that need to be freed */
	uint32						bResourcesNeedFreeing:1;

	friend class FXAudio2Device;
	friend class FXAudio2SoundSourceCallback;
};

/**
 * Helper class for 5.1 spatialization.
 */
class FSpatializationHelper
{
	/** Instance of X3D used to calculate volume multipliers.	*/
	X3DAUDIO_HANDLE		          X3DInstance;
	
	X3DAUDIO_DSP_SETTINGS         DSPSettings;
	X3DAUDIO_LISTENER             Listener;
	X3DAUDIO_EMITTER              Emitter;
	X3DAUDIO_CONE                 Cone;
	
	X3DAUDIO_DISTANCE_CURVE_POINT VolumeCurvePoint[2];
	X3DAUDIO_DISTANCE_CURVE       VolumeCurve;
	
	X3DAUDIO_DISTANCE_CURVE_POINT ReverbVolumeCurvePoint[2];
	X3DAUDIO_DISTANCE_CURVE       ReverbVolumeCurve;

	float                         EmitterAzimuths;
	float					      MatrixCoefficients[SPEAKER_COUNT];
	
public:
	/**
	 * Constructor, initializing all member variables.
	 */
	FSpatializationHelper( void );

	/**
	 * Calculates the spatialized volumes for each channel.
	 *
	 * @param	OrientFront				The listener's facing direction.
	 * @param	ListenerPosition		The position of the listener.
	 * @param	EmitterPosition			The position of the emitter.
	 * @param	OutVolumes				An array of floats with one volume for each output channel.
	 * @param	OutReverbLevel			The reverb volume
	 */
	void CalculateDolbySurroundRate( const FVector& OrientFront, const FVector& ListenerPosition, const FVector& EmitterPosition, float* OutVolumes );
};

/** Variables required for the early init */
struct FXAudioDeviceProperties
{
	static int32							NumSpeakers;
	static struct IXAudio2*					XAudio2;
	static struct IXAudio2MasteringVoice*		MasteringVoice;
	static const float*					OutputMixMatrix;
#if XAUDIO_SUPPORTS_DEVICE_DETAILS
	static XAUDIO2_DEVICE_DETAILS		DeviceDetails;
#endif	//XAUDIO_SUPPORTS_DEVICE_DETAILS
};

#if XAUDIO_SUPPORTS_DEVICE_DETAILS
	#define UE4_XAUDIO2_NUMCHANNELS		FXAudioDeviceProperties::DeviceDetails.OutputFormat.Format.nChannels
	#define UE4_XAUDIO2_CHANNELMASK		FXAudioDeviceProperties::DeviceDetails.OutputFormat.dwChannelMask
	#define UE4_XAUDIO2_SAMPLERATE		FXAudioDeviceProperties::DeviceDetails.OutputFormat.Format.nSamplesPerSec
#else	//XAUDIO_SUPPORTS_DEVICE_DETAILS
	#define UE4_XAUDIO2_NUMCHANNELS		XAUDIO2_DEFAULT_CHANNELS
	#define UE4_XAUDIO2_CHANNELMASK		3		// Default to left and right speakers...
	#define UE4_XAUDIO2_SAMPLERATE		XAUDIO2_DEFAULT_SAMPLERATE
#endif	//XAUDIO_SUPPORTS_DEVICE_DETAILS
