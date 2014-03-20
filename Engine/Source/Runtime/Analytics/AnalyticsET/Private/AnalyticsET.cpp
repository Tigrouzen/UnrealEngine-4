// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Runtime/Analytics/AnalyticsET/Private/AnalyticsETPrivatePCH.h"

#include "Core.h"
#include "Guid.h"
#include "Json.h"
#include "SecureHash.h"
#include "Runtime/Analytics/AnalyticsET/Public/AnalyticsET.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "EngineVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnalytics, Log, All);

IMPLEMENT_MODULE( FAnalyticsET, AnalyticsET );

class FAnalyticsProviderET : 
	public IAnalyticsProvider,
	public FTickerObjectBase
{
public:
	FAnalyticsProviderET(const FAnalyticsET::Config& ConfigValues);

	// FTickerObjectBase

	bool Tick(float DeltaSeconds) OVERRIDE;

	// IAnalyticsProvider

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) OVERRIDE;
	virtual void EndSession() OVERRIDE;
	/** ET PC implementation doesn't cache events */
	virtual void FlushEvents() OVERRIDE;

	virtual void SetUserID(const FString& InUserID) OVERRIDE;
	virtual FString GetUserID() const OVERRIDE;

	virtual FString GetSessionID() const OVERRIDE;
	virtual bool SetSessionID(const FString& InSessionID) OVERRIDE;

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) OVERRIDE;

	virtual ~FAnalyticsProviderET();

	FString GetAPIKey() const { return APIKey; }

private:
	bool bSessionInProgress;
	/** ET Game API Key - Get from your account manager */
	FString APIKey;
	/** ET API Server */
	FString APIServer;
	/** the unique UserID as passed to ET. */
	FString UserID;
	/** The session ID */
	FString SessionID;
	/** Cached build type as a string */
	FString BuildType;
	/** The AppVersion passed to ET. */
	FString AppVersion;

	/** Max number of analytics events to cache before pushing to server */
	const int32 MaxCachedNumEvents;
	/** Max time that can elapse before pushing cached events to server */
	const float MaxCachedElapsedTime;
	/** Current countdown timer to keep track of MaxCachedElapsedTime push */
	float FlushEventsCountdown;
	/**
	 * Analytics event entry to be cached
	 */
	struct FAnalyticsEventEntry
	{
		/** name of event */
		FString EventName;
		/** optional list of attributes */
		TArray<FAnalyticsEventAttribute> Attributes;
		/** local time when event was triggered */
		FDateTime TimeStamp;
		/**
		 * Constructor
		 */
		FAnalyticsEventEntry(const FString& InEventName, const TArray<FAnalyticsEventAttribute>& InAttributes, FDateTime InTimeStamp=FDateTime::UtcNow())
			: EventName(InEventName)
			, Attributes(InAttributes)
			, TimeStamp(InTimeStamp)
		{}
	};
	/** List of analytic events pending a server update */
	TArray<FAnalyticsEventEntry> CachedEvents;

	/**
	 * Delegate called when an event Http request completes
	 */
	void EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
};

void FAnalyticsET::StartupModule()
{
}

void FAnalyticsET::ShutdownModule()
{
}

TSharedPtr<IAnalyticsProvider> FAnalyticsET::CreateAnalyticsProvider(const FAnalytics::FProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		Config ConfigValues;
		ConfigValues.APIKeyET = GetConfigValue.Execute(Config::GetKeyNameForAPIKey(), true);
		ConfigValues.APIServerET = GetConfigValue.Execute(Config::GetKeyNameForAPIServer(), false);
		ConfigValues.AppVersionET = GetConfigValue.Execute(Config::GetKeyNameForAppVersion(), false);
		return CreateAnalyticsProvider(ConfigValues);
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider called with an unbound delegate"));
	}
	return NULL;
}

TSharedPtr<IAnalyticsProvider> FAnalyticsET::CreateAnalyticsProvider(const Config& ConfigValues) const
{
	// If we didn't have a proper APIKey, return NULL
	if (ConfigValues.APIKeyET.IsEmpty())
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider config not contain required parameter %s"), *Config::GetKeyNameForAPIKey());
		return NULL;
	}
	return TSharedPtr<IAnalyticsProvider>(new FAnalyticsProviderET(ConfigValues));
}

/**
 * Perform any initialization.
 */
FAnalyticsProviderET::FAnalyticsProviderET(const FAnalyticsET::Config& ConfigValues)
	:bSessionInProgress(false)
	, MaxCachedNumEvents(20)
	, MaxCachedElapsedTime(60.0f)
	, FlushEventsCountdown(MaxCachedElapsedTime)
{
	UE_LOG(LogAnalytics, Verbose, TEXT("Initializing ET Analytics provider"));

	APIKey = ConfigValues.APIKeyET;
	if (APIKey.IsEmpty())
	{
		UE_LOG(LogAnalytics, Warning, TEXT("AnalyticsET missing APIKey. No uploads will be processed."));
	}
	// allow the APIServer value to be empty and use defaults.
	APIServer = ConfigValues.APIServerET.IsEmpty() 
		? FAnalyticsET::Config::GetDefaultAPIServer()
		: ConfigValues.APIServerET;

	// default to GEngineVersion if one is not provided, append GEngineVersion otherwise.
	AppVersion = ConfigValues.AppVersionET.IsEmpty()
		? FString::Printf(TEXT("%s"), *GEngineVersion.ToString())
		: FString::Printf(TEXT("%s"), *ConfigValues.AppVersionET);

	UE_LOG(LogAnalytics, Log, TEXT("ET APIKey = %s. APIServer = %s. AppVersion = %s"), *APIKey, *APIServer, *AppVersion);

	// cache the build type string
	FAnalytics::BuildType BuildTypeEnum = FAnalytics::Get().GetBuildType();
	BuildType = 
		BuildTypeEnum == FAnalytics::Debug 
			? "Debug"
			: BuildTypeEnum == FAnalytics::Development
				? "Development"
				: BuildTypeEnum == FAnalytics::Release
					? "Release"
					: BuildTypeEnum == FAnalytics::Test
						? "Test"
						: "UNKNOWN";

	// see if there is a cmdline supplied UserID.
#if !UE_BUILD_SHIPPING
	FString ConfigUserID;
	if (FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSUSERID="), ConfigUserID, false))
	{
		SetUserID(ConfigUserID);
	}
#endif // !UE_BUILD_SHIPPING
}

bool FAnalyticsProviderET::Tick(float DeltaSeconds)
{
	if (CachedEvents.Num() > 0)
	{
		// Countdown to flush
		FlushEventsCountdown -= DeltaSeconds;
		// If reached countdown or already at max cached events then flush
		if (FlushEventsCountdown <= 0 ||
			CachedEvents.Num() >= MaxCachedNumEvents)
		{
			FlushEvents();
		}
	}
	return true;
}

FAnalyticsProviderET::~FAnalyticsProviderET()
{
	UE_LOG(LogAnalytics, Verbose, TEXT("Destroying ET Analytics provider"));
	EndSession();
}

/**
 * Start capturing stats for upload
 * Uses the unique ApiKey associated with your app
 */
bool FAnalyticsProviderET::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	UE_LOG(LogAnalytics, Log, TEXT("AnalyticsET::StartSession [%s]"),*APIKey);

	// end/flush previous session before staring new one
	if (bSessionInProgress)
	{
		EndSession();
	}

	FGuid SessionGUID;
	FPlatformMisc::CreateGuid(SessionGUID);
	SessionID = SessionGUID.ToString(EGuidFormats::DigitsWithHyphensInBraces);

	RecordEvent(TEXT("SessionStart"), Attributes);
	bSessionInProgress = !UserID.IsEmpty();
	return bSessionInProgress;
}

/**
 * End capturing stats and queue the upload 
 */
void FAnalyticsProviderET::EndSession()
{
	if (bSessionInProgress)
	{
		RecordEvent(TEXT("SessionEnd"), TArray<FAnalyticsEventAttribute>());
		FlushEvents();
		SessionID.Empty();
	}
	bSessionInProgress = false;
}

void FAnalyticsProviderET::FlushEvents()
{
	FString Payload;

	FDateTime CurrentTime = FDateTime::UtcNow();

	TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&Payload);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteArrayStart(TEXT("Events"));
	for (int32 EventIdx = 0; EventIdx < CachedEvents.Num(); EventIdx++)
	{
		const FAnalyticsEventEntry& Entry = CachedEvents[EventIdx];
		// event entry
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("EventName"), Entry.EventName);
		FString DateOffset = (CurrentTime - Entry.TimeStamp).ToString();
		JsonWriter->WriteValue(TEXT("DateOffset"), DateOffset);
		if (Entry.Attributes.Num() > 0)
		{
			// optional attributes for this event
			for (int32 AttrIdx = 0; AttrIdx < Entry.Attributes.Num(); AttrIdx++)
			{
				const FAnalyticsEventAttribute& Attr = Entry.Attributes[AttrIdx];				
				JsonWriter->WriteValue(Attr.AttrName, Attr.AttrValue);
			}
		}
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteArrayEnd();
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	UE_LOG(LogAnalytics, Verbose, TEXT("ET Flush: Payload:\n%s"), *Payload);

	// Create/send Http request for an event
 	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
 	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
 	HttpRequest->SetURL(
		FString::Printf(TEXT("%sCollectData.1?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s"),
		*APIServer, 
		*FGenericPlatformHttp::UrlEncode(SessionID),
		*FGenericPlatformHttp::UrlEncode(APIKey), 
		*FGenericPlatformHttp::UrlEncode(AppVersion),
		*FGenericPlatformHttp::UrlEncode(UserID)
		));
 	HttpRequest->SetVerb(TEXT("POST"));
 	HttpRequest->SetContentAsString(Payload);
 	HttpRequest->OnProcessRequestComplete().BindRaw(this, &FAnalyticsProviderET::EventRequestComplete);
 	HttpRequest->ProcessRequest();

	FlushEventsCountdown = MaxCachedElapsedTime;
	CachedEvents.Empty();
}

void FAnalyticsProviderET::SetUserID(const FString& InUserID)
{
	// command-line specified user ID overrides all attempts to reset it.
	if (!FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSUSERID="), UserID, false))
	{
		UE_LOG(LogAnalytics, Log, TEXT("SetUserId %s"), *InUserID);
		UserID = InUserID;
	}
	else if (UserID != InUserID)
	{
		UE_LOG(LogAnalytics, Log, TEXT("Overriding SetUserId %s with cmdline UserId of %s."), *InUserID, *UserID);
	}
}

FString FAnalyticsProviderET::GetUserID() const
{
	return UserID;
}

FString FAnalyticsProviderET::GetSessionID() const
{
	return SessionID;
}

bool FAnalyticsProviderET::SetSessionID(const FString& InSessionID)
{
	if (bSessionInProgress)
	{
		SessionID = InSessionID;
		UE_LOG(LogAnalytics, Log, TEXT("AnalyticsET: Forcing SessionID to %s."), *SessionID);
		return true;
	}
	return false;
}

/** Helper to log any ET event. Used by all the LogXXX functions. */
void FAnalyticsProviderET::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (Attributes.Num() > 40)
	{
		UE_LOG(LogAnalytics, Log, TEXT("Event %s has too many attributes (%d). May be truncated at the collector."), *EventName, Attributes.Num());
	}

	CachedEvents.Add(FAnalyticsEventEntry(EventName, Attributes));
}


void FAnalyticsProviderET::EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid())
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("ET response for [%s]. Code: %d. Payload: %s"), *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
		FString PayloadStr = HttpResponse->GetContentAsString();
		bool test = false;
	}
	else
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("ET response for [%s]. No response"), *HttpRequest->GetURL());
	}
}
