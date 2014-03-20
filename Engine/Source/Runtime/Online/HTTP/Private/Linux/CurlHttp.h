// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "curl/curl.h"

/**
 * Curl implementation of an HTTP request
 */
class FCurlHttpRequest : public IHttpRequest
{
public:

	// implementation friends
	friend class FCurlHttpResponse;

	// Begin IHttpBase interface
	virtual FString GetURL() OVERRIDE;
	virtual FString GetURLParameter(const FString& ParameterName) OVERRIDE;
	virtual FString GetHeader(const FString& HeaderName) OVERRIDE;
	virtual TArray<FString> GetAllHeaders() OVERRIDE;	
	virtual FString GetContentType() OVERRIDE;
	virtual int32 GetContentLength() OVERRIDE;
	virtual const TArray<uint8>& GetContent() OVERRIDE;
	// End IHttpBase interface

	// Begin IHttpRequest interface
	virtual FString GetVerb() OVERRIDE;
	virtual void SetVerb(const FString& InVerb) OVERRIDE;
	virtual void SetURL(const FString& InURL) OVERRIDE;
	virtual void SetContent(const TArray<uint8>& ContentPayload) OVERRIDE;
	virtual void SetContentAsString(const FString& ContentString) OVERRIDE;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) OVERRIDE;
	virtual bool ProcessRequest() OVERRIDE;
	virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() OVERRIDE;
	virtual FHttpRequestProgressDelegate& OnRequestProgress() OVERRIDE;
	virtual void CancelRequest() OVERRIDE;
	virtual EHttpRequestStatus::Type GetStatus() OVERRIDE;
	virtual void Tick(float DeltaSeconds) OVERRIDE;
	// End IHttpRequest interface

	/**
	 * Returns libcurl's easy handle - needed for HTTP manager.
	 *
	 * @return libcurl's easy handle
	 */
	inline CURL * GetEasyHandle() const
	{
		return EasyHandle;
	}

	/**
	 * Marks request as completed (set by HTTP manager).
	 *
	 * Note that this method is intended to be lightweight,
	 * more processing will be done in Tick()
	 *
	 * @param CurlCompletionResult Operation result code as returned by libcurl
	 */
	inline void MarkAsCompleted(CURLcode InCurlCompletionResult)
	{
		bCompleted = true;
		CurlCompletionResult = InCurlCompletionResult;
	}

	/**
	 * Constructor
	 */
	FCurlHttpRequest(CURLM * InMultiHandle);

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FCurlHttpRequest();


private:

	/**
	 * Static callback to be used as read function (CURLOPT_READFUNCTION), will dispatch the call to proper instance
	 *
	 * @param Ptr buffer to copy data to (allocated and managed by libcurl)
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @param UserData data we associated with request (will be a pointer to FCurlHttpRequest instance)
	 * @return number of bytes actually written to buffer, or CURL_READFUNC_ABORT to abort the operation
	 */
	static size_t StaticUploadCallback(void * Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void * UserData);

	/**
	 * Method called when libcurl wants us to supply more data (see CURLOPT_READFUNCTION)
	 *
	 * @param Ptr buffer to copy data to (allocated and managed by libcurl)
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @return number of bytes actually written to buffer, or CURL_READFUNC_ABORT to abort the operation
	 */
	size_t UploadCallback(void * Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes);

	/**
	 * Static callback to be used as header function (CURLOPT_HEADERFUNCTION), will dispatch the call to proper instance
	 *
	 * @param Ptr buffer to copy data to (allocated and managed by libcurl)
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @param UserData data we associated with request (will be a pointer to FCurlHttpRequest instance)
	 * @return number of bytes actually processed, error is triggered if it does not match number of bytes passed
	 */
	static size_t StaticReceiveResponseHeaderCallback(void * Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void * UserData);

	/**
	 * Method called when libcurl wants us to receive response header (see CURLOPT_HEADERFUNCTION). Headers will be passed
	 * line by line (i.e. this callback will be called with a full line), not necessarily zero-terminated. This callback will
	 * be also passed any intermediate headers, not only final response's ones.
	 *
	 * @param Ptr buffer to copy data to (allocated and managed by libcurl)
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @return number of bytes actually processed, error is triggered if it does not match number of bytes passed
	 */
	size_t ReceiveResponseHeaderCallback(void * Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes);

	/**
	 * Static callback to be used as write function (CURLOPT_WRITEFUNCTION), will dispatch the call to proper instance
	 *
	 * @param Ptr buffer to copy data to (allocated and managed by libcurl)
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @param UserData data we associated with request (will be a pointer to FCurlHttpRequest instance)
	 * @return number of bytes actually processed, error is triggered if it does not match number of bytes passed
	 */
	static size_t StaticReceiveResponseBodyCallback(void * Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void * UserData);

	/**
	 * Method called when libcurl wants us to receive response body (see CURLOPT_WRITEFUNCTION)
	 *
	 * @param Ptr buffer to copy data to (allocated and managed by libcurl)
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @return number of bytes actually processed, error is triggered if it does not match number of bytes passed
	 */
	size_t ReceiveResponseBodyCallback(void * Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	/**
	 * Static callback to be used as debug function (CURLOPT_DEBUGFUNCTION), will dispatch the call to proper instance
	 *
	 * @param Handle handle to which the debug information applies
	 * @param DebugInfoType type of information (CURLINFO_*) 
	 * @param DebugInfo debug information itself (may NOT be text, may NOT be zero-terminated)
	 * @param DebugInfoSize exact size of debug information
	 * @param UserData data we associated with request (will be a pointer to FCurlHttpRequest instance)
	 * @return must return 0
	 */
	static size_t StaticDebugCallback(CURL * Handle, curl_infotype DebugInfoType, char * DebugInfo, size_t DebugInfoSize, void * UserData);

	/**
	 * Method called with debug information about libcurl activities (see CURLOPT_DEBUGFUNCTION)
	 *
	 * @param Handle handle to which the debug information applies
	 * @param DebugInfoType type of information (CURLINFO_*) 
	 * @param DebugInfo debug information itself (may NOT be text, may NOT be zero-terminated)
	 * @param DebugInfoSize exact size of debug information
	 * @return must return 0
	 */
	size_t DebugCallback(CURL * Handle, curl_infotype DebugInfoType, char * DebugInfo, size_t DebugInfoSize);
#endif // !UE_BUILD_SHIPPING && !UE_BUILD_TEST

	/**
	 * Create the session connection and initiate the web request
	 *
	 * @return true if the request was started
	 */
	bool StartRequest();

	/**
	 * Process state for a finished request that no longer needs to be ticked
	 * Calls the completion delegate
	 */
	void FinishedRequest();

	/**
	 * Close session/request handles and unregister callbacks
	 */
	void CleanupRequest();

private:

	/** Pointer to parent multi handle that groups all individual easy handles */
	CURLM *			MultiHandle;
	/** Pointer to an easy handle specific to this request */
	CURL *			EasyHandle;	
	/** List of custom headers to be passed to CURL */
	curl_slist *	HeaderList;
	/** Cached URL */
	FString			URL;
	/** Cached verb */
	FString			Verb;
	/** Set to true if request has been canceled */
	bool			bCanceled;
	/** Set to true when request has been completed */
	bool			bCompleted;
	/** Operation result code as returned by libcurl */
	CURLcode		CurlCompletionResult;
	/** Set to true when easy handle has been added to a multi handle */
	bool			bEasyHandleAddedToMulti;
	/** Number of bytes sent already */
	uint32			BytesSent;
	/** The response object which we will use to pair with this request */
	TSharedPtr<class FCurlHttpResponse,ESPMode::ThreadSafe> Response;
	/** BYTE array payload to use with the request. Typically for a POST */
	TArray<uint8> RequestPayload;
	/** Delegate that will get called once request completes or on any error */
	FHttpRequestCompleteDelegate RequestCompleteDelegate;
	/** Delegate that will get called once per tick with bytes downloaded so far */
	FHttpRequestProgressDelegate RequestProgressDelegate;
	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus;
	/** Mapping of header section to values. */
	TMap<FString, FString> Headers;
	/** Total elapsed time in seconds since the start of the request */
	float ElapsedTime;
};



/**
 * Curl implementation of an HTTP response
 */
class FCurlHttpResponse : public IHttpResponse
{
private:

	/** Request that owns this response */
	FCurlHttpRequest& Request;


public:
	// implementation friends
	friend class FCurlHttpRequest;


	// Begin IHttpBase interface
	virtual FString GetURL() OVERRIDE;
	virtual FString GetURLParameter(const FString& ParameterName) OVERRIDE;
	virtual FString GetHeader(const FString& HeaderName) OVERRIDE;
	virtual TArray<FString> GetAllHeaders() OVERRIDE;	
	virtual FString GetContentType() OVERRIDE;
	virtual int32 GetContentLength() OVERRIDE;
	virtual const TArray<uint8>& GetContent() OVERRIDE;
	// End IHttpBase interface

	// Begin IHttpResponse interface
	virtual int32 GetResponseCode() OVERRIDE;
	virtual FString GetContentAsString() OVERRIDE;
	// End IHttpResponse interface

	/**
	 * Check whether a response is ready or not.
	 */
	bool IsReady();

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FCurlHttpResponse(FCurlHttpRequest& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FCurlHttpResponse();


private:

	/** BYTE array to fill in as the response is read via didReceiveData */
	TArray<uint8> Payload;
	/** Caches how many bytes of the response we've read so far */
	int32 TotalBytesRead;
	/** Cached key/value header pairs. Parsed once request completes */
	TMap<FString, FString> Headers;
	/** Cached code from completed response */
	int32 HttpCode;
	/** Cached content length from completed response */
	int32 ContentLength;
	/** True when the response has finished async processing */
	int32 volatile bIsReady;
	/** True if the response was successfully received/processed */
	int32 volatile bSucceeded;
};
