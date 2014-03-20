// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace EHttpRequestStatus
{
	/**
	 * Enumerates the current state of an Http request
	 */
	enum Type
	{
		/** Has not been started via ProcessRequest() */
		NotStarted,
		/** Currently being ticked and processed */
		Processing,
		/** Finished but failed */
		Failed,
		/** Finished and was successful */
		Succeeded
	};
}

typedef TSharedPtr<class IHttpRequest> FHttpRequestPtr;
typedef TSharedPtr<class IHttpResponse,ESPMode::ThreadSafe> FHttpResponsePtr;

/**
 * Delegate called when an Http request completes
 *
 * @param first parameter - original Http request that started things
 * @param second parameter - response received from the server if a successful connection was established
 * @param third parameter - indicates whether or not the request was able to connect successfully
 */
DECLARE_DELEGATE_ThreeParams(FHttpRequestCompleteDelegate, FHttpRequestPtr, FHttpResponsePtr, bool);

/**
 * Delegate called per tick to update an Http request download size progress
 *
 * @param first parameter - original Http request that started things
 * @param second parameter - the number of bytes downloaded so far.
 */
DECLARE_DELEGATE_TwoParams(FHttpRequestProgressDelegate, FHttpRequestPtr, int32);

/**
 * Interface for Http requests (created using FHttpFactory)
 */
class IHttpRequest : 
	public IHttpBase, public TSharedFromThis<IHttpRequest>
{
public:

	/**
	 * Gets the verb (GET, PUT, POST) used by the request.
	 * 
	 * @return the verb string
	 */
	virtual FString GetVerb() = 0;

	/**
	 * Sets the verb used by the request.
	 * Eg. (GET, PUT, POST)
	 * Should be set before calling ProcessRequest.
	 * If not specified then a GET is assumed.
	 *
	 * @param Verb - verb to use.
	 */
	virtual void SetVerb(const FString& Verb) = 0;

	/**
	 * Sets the URL for the request 
	 * Eg. (http://my.domain.com/something.ext?key=value&key2=value).
	 * Must be set before calling ProcessRequest.
	 *
	 * @param URL - URL to use.
	 */
	virtual void SetURL(const FString& URL) = 0;

	/**
	 * Sets the content of the request (optional data).
	 * Usually only set for POST requests.
	 *
	 * @param ContentPayload - payload to set.
	 */
	virtual void SetContent(const TArray<uint8>& ContentPayload) = 0;

	/**
	 * Sets the content of the request as a string encoded as UTF8.
	 *
	 * @param ContentString - payload to set.
	 */
	virtual void SetContentAsString(const FString& ContentString) = 0;

	/**
	 * Sets optional header info.
	 * Content-Length is the only header set for you.
	 * Required headers depends on the request itself.
	 * Eg. "multipart/form-data" needed for a form post
	 *
	 * @param HeaderName - Name of the header (ie, Content-Type)
	 * @param HeaderValue - Value of the header
	 */
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) = 0;

	/**
	 * Called to begin processing the request.
	 * OnProcessRequestComplete delegate is always called when the request completes or on error if it is bound.
	 * A request can be re-used but not while still being processed.
	 *
	 * @return if the request was successfully started.
	 */
	virtual bool ProcessRequest() = 0;

	/**
	 * Delegate called when the request is complete. See FHttpRequestCompleteDelegate
	 */
	virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() = 0;

	/**
	 * Delegate called to update the request progress. See FHttpRequestProgressDelegate
	 */
	virtual FHttpRequestProgressDelegate& OnRequestProgress() = 0;

	/**
	 * Called to cancel a request that is still being processed
	 */
	virtual void CancelRequest() = 0;

	/**
	 * Get the current status of the request being processed
	 *
	 * @return the current status
	 */
	virtual EHttpRequestStatus::Type GetStatus() = 0;

	/**
	 * Used to tick the request
	 *
	 * @param DeltaSeconds - seconds since last ticked
	 */
	virtual void Tick(float DeltaSeconds) = 0;

	/** 
	 * Destructor for overrides 
	 */
	virtual ~IHttpRequest() {};
};

