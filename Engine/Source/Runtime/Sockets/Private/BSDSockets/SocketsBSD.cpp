// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SocketsBSD.cpp: Implements the FSocketBSD class.
=============================================================================*/

#include "SocketsPrivatePCH.h"

#if PLATFORM_HAS_BSD_SOCKETS

#include "SocketSubsystemBSD.h"
#include "SocketsBSD.h"
//#include "Net/NetworkProfiler.h"

#if PLATFORM_HTML5 
typedef unsigned long   u_long;
#endif 

/* FSocket interface
 *****************************************************************************/

bool FSocketBSD::Close(void)
{
	if (Socket != INVALID_SOCKET)
	{
		int32 error = closesocket(Socket);
		Socket = INVALID_SOCKET;
		return error == 0;
	}
	return false;
}


bool FSocketBSD::Bind(const FInternetAddr& Addr)
{
	return bind(Socket, (sockaddr*)(FInternetAddrBSD&)Addr, sizeof(sockaddr_in)) == 0;
}


bool FSocketBSD::Connect(const FInternetAddr& Addr)
{
	int32 Return = connect(Socket, (sockaddr*)(FInternetAddrBSD&)Addr, sizeof(sockaddr_in));
	
	check(SocketSubsystem);
	ESocketErrors Error = SocketSubsystem->TranslateErrorCode(Return);

	// "would block" is not an error
	return ((Error == SE_NO_ERROR) || (Error == SE_EWOULDBLOCK));
}


bool FSocketBSD::Listen(int32 MaxBacklog)
{
	return listen(Socket, MaxBacklog) == 0;
}

ESocketInternalState::Return FSocketBSD::HasState(ESocketInternalState::Param State, FTimespan WaitTime)
{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_SELECT
	// convert WaitTime to a timeval
	timeval Time;
	Time.tv_sec = (int32)WaitTime.GetTotalSeconds();
	Time.tv_usec = WaitTime.GetMilliseconds() * 1000;

	fd_set SocketSet;

	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket, &SocketSet);

	// Check the status of the state
	int32 SelectStatus = 0;
	switch (State)
	{
		case ESocketInternalState::CanRead:
			SelectStatus = select(Socket + 1, &SocketSet, NULL, NULL, &Time);
			break;
		case ESocketInternalState::CanWrite:
			SelectStatus = select(Socket + 1, NULL, &SocketSet, NULL, &Time);
			break;
		case ESocketInternalState::HasError:
			SelectStatus = select(Socket + 1, NULL, NULL, &SocketSet, &Time);
			break;
	}

	// if the select returns a positive number, the socket had the state, 0 means didn't have it, and negative is API error condition (not socket's error state)
	return SelectStatus > 0  ? ESocketInternalState::Yes : 
		   SelectStatus == 0 ? ESocketInternalState::No : 
		   ESocketInternalState::EncounteredError;
#else
	UE_LOG(LogSockets, Fatal, TEXT("This platform doesn't support select(), but FSocketBSD::HasState was not overridden"));
	return ESocketInternalState::EncounteredError;
#endif
}

bool FSocketBSD::HasPendingConnection(bool& bHasPendingConnection)
{
	bool bHasSucceeded = false;
	bHasPendingConnection = false;

	// make sure socket has no error state
	if (HasState(ESocketInternalState::HasError) == ESocketInternalState::No)
	{
		// get the read state
		ESocketInternalState::Return State = HasState(ESocketInternalState::CanRead);
		
		// turn the result into the outputs
		bHasSucceeded = State != ESocketInternalState::EncounteredError;
		bHasPendingConnection = State == ESocketInternalState::Yes;
	}

	return bHasSucceeded;
}


bool FSocketBSD::HasPendingData(uint32& PendingDataSize)
{
	bool bHasSucceeded = false;
	PendingDataSize = 0;

	// make sure socket has no error state
	if (HasState(ESocketInternalState::CanRead) == ESocketInternalState::Yes)
	{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_IOCTL
		// See if there is any pending data on the read socket
		if(ioctlsocket(Socket, FIONREAD, (u_long*)(&PendingDataSize)) == 0)
#endif
		{
			bHasSucceeded = true;
		}
	}

	return bHasSucceeded;
}


FSocket* FSocketBSD::Accept(const FString& SocketDescription)
{
	SOCKET NewSocket = accept(Socket,NULL,NULL);

	if (NewSocket != INVALID_SOCKET)
	{
		// we need the subclass to create the actual FSocket object
		check(SocketSubsystem);
		FSocketSubsystemBSD* BSDSystem = static_cast<FSocketSubsystemBSD*>(SocketSubsystem);
		return BSDSystem->InternalBSDSocketFactory(NewSocket, SocketType, SocketDescription);
	}

	return NULL;
}


FSocket* FSocketBSD::Accept(FInternetAddr& OutAddr, const FString& SocketDescription)
{
	SOCKLEN SizeOf = sizeof(sockaddr_in);
	SOCKET NewSocket = accept(Socket, *(FInternetAddrBSD*)(&OutAddr), &SizeOf);

	if (NewSocket != INVALID_SOCKET)
	{
		// we need the subclass to create the actual FSocket object
		check(SocketSubsystem);
		FSocketSubsystemBSD* BSDSystem = static_cast<FSocketSubsystemBSD*>(SocketSubsystem);
		return BSDSystem->InternalBSDSocketFactory(NewSocket, SocketType, SocketDescription);
	}

	return NULL;
}


bool FSocketBSD::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	// Write the data and see how much was written
	BytesSent = sendto(Socket, (const char*)Data, Count, 0, (FInternetAddrBSD&)Destination, sizeof(sockaddr_in));

//	NETWORK_PROFILER(FSocket::SendTo(Data,Count,BytesSent,Destination));

	bool Result = BytesSent >= 0;
	if (Result)
	{
		LastActivityTime = FDateTime::UtcNow();
	}
	return Result;
}


bool FSocketBSD::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	BytesSent = send(Socket,(const char*)Data,Count,0);

//	NETWORK_PROFILER(FSocket::Send(Data,Count,BytesSent));

	bool Result = BytesSent >= 0;
	if (Result)
	{
		LastActivityTime = FDateTime::UtcNow();
	}
	return Result;
}


bool FSocketBSD::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	SOCKLEN Size = sizeof(sockaddr_in);
	sockaddr& Addr = *(FInternetAddrBSD&)Source;

	// Read into the buffer and set the source address
	BytesRead = recvfrom(Socket, (char*)Data, BufferSize, Flags, &Addr, &Size);
//	NETWORK_PROFILER(FSocket::RecvFrom(Data,BufferSize,BytesRead,Source));

	bool Result = BytesRead >= 0;
	if (Result)
	{
		LastActivityTime = FDateTime::UtcNow();
	}

	return Result;
}


bool FSocketBSD::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	BytesRead = recv(Socket, (char*)Data, BufferSize, Flags);

//	NETWORK_PROFILER(FSocket::Recv(Data,BufferSize,BytesRead));

	bool Result = BytesRead >= 0;
	if (Result)
	{
		LastActivityTime = FDateTime::UtcNow();
	}
	return Result;
}


bool FSocketBSD::Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime)
{
	if ((Condition == ESocketWaitConditions::WaitForRead) || (Condition == ESocketWaitConditions::WaitForReadOrWrite))
	{
		if (HasState(ESocketInternalState::CanRead, WaitTime) == ESocketInternalState::Yes)
		{
			return true;
		}
	}

	if ((Condition == ESocketWaitConditions::WaitForWrite) || (Condition == ESocketWaitConditions::WaitForReadOrWrite))
	{
		if (HasState(ESocketInternalState::CanWrite, WaitTime) == ESocketInternalState::Yes)
		{
			return true;
		}
	}

	return false;
}


ESocketConnectionState FSocketBSD::GetConnectionState(void)
{
	ESocketConnectionState CurrentState = SCS_ConnectionError;

	// look for an existing error
	if (HasState(ESocketInternalState::HasError) == ESocketInternalState::No)
	{
		if (FDateTime::UtcNow() - LastActivityTime > FTimespan::FromSeconds(5))
		{
			// get the write state
			ESocketInternalState::Return WriteState = HasState(ESocketInternalState::CanWrite, FTimespan::FromMilliseconds(1));
			ESocketInternalState::Return ReadState = HasState(ESocketInternalState::CanRead, FTimespan::FromMilliseconds(1));
		
			// translate yes or no (error is already set)
			if (WriteState == ESocketInternalState::Yes || ReadState == ESocketInternalState::Yes)
			{
				CurrentState = SCS_Connected;
				LastActivityTime = FDateTime::UtcNow();
			}
			else if (WriteState == ESocketInternalState::No && ReadState == ESocketInternalState::No)
			{
				CurrentState = SCS_NotConnected;
			}
		}
		else
		{
			CurrentState = SCS_Connected;
		}
	}

	return CurrentState;
}


void FSocketBSD::GetAddress(FInternetAddr& OutAddr)
{
	FInternetAddrBSD& Addr = (FInternetAddrBSD&)OutAddr;
	SOCKLEN Size = sizeof(sockaddr_in);

	// Figure out what ip/port we are bound to
	bool bOk = getsockname(Socket, Addr, &Size) == 0;

	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to read address for socket (%s)"), SocketSubsystem->GetSocketError());
	}
}


bool FSocketBSD::SetNonBlocking(bool bIsNonBlocking)
{
	u_long Value = bIsNonBlocking ? true : false;

#if PLATFORM_HTML5 
	// can't have blocking sockets.
	ensureMsgf(bIsNonBlocking, TEXT("Can't have blocking sockets on HTML5"));
    return true;
#else 

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
	return ioctlsocket(Socket,FIONBIO,&Value) == 0;
#else 
	int Flags = fcntl(Socket, F_GETFL, 0);
	//Set the flag or clear it, without destroying the other flags.
	Flags = bIsNonBlocking ? Flags | O_NONBLOCK : Flags ^ (Flags & O_NONBLOCK);
	int err = fcntl(Socket, F_SETFL, Flags);
	return (err == 0 ? true : false);
#endif
#endif 
}


bool FSocketBSD::SetBroadcast(bool bAllowBroadcast)
{
	int Param = bAllowBroadcast ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_BROADCAST,(char*)&Param,sizeof(Param)) == 0;
}


bool FSocketBSD::JoinMulticastGroup(const FInternetAddr& GroupAddress)
{
	ip_mreq imr;

	imr.imr_interface.s_addr = INADDR_ANY;
	imr.imr_multiaddr = ((sockaddr_in*)&**((FInternetAddrBSD*)(&GroupAddress)))->sin_addr;

	return (setsockopt(Socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSD::LeaveMulticastGroup(const FInternetAddr& GroupAddress)
{
	ip_mreq imr;

	imr.imr_interface.s_addr = INADDR_ANY;
	imr.imr_multiaddr = ((sockaddr_in*)&**((FInternetAddrBSD*)(&GroupAddress)))->sin_addr;

	return (setsockopt(Socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSD::SetMulticastLoopback(bool bLoopback)
{
	return (setsockopt(Socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&bLoopback, sizeof(bLoopback)) == 0);
}


bool FSocketBSD::SetMulticastTtl(uint8 TimeToLive)
{
	return (setsockopt(Socket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&TimeToLive, sizeof(TimeToLive)) == 0);
}


bool FSocketBSD::SetReuseAddr(bool bAllowReuse)
{
	int Param = bAllowReuse ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,(char*)&Param,sizeof(Param)) == 0;
}


bool FSocketBSD::SetLinger(bool bShouldLinger,int32 Timeout)
{
	linger ling;

	ling.l_onoff = bShouldLinger;
	ling.l_linger = Timeout;

	return setsockopt(Socket,SOL_SOCKET,SO_LINGER,(char*)&ling,sizeof(ling)) == 0;
}


bool FSocketBSD::SetRecvErr(bool bUseErrorQueue)
{
	// Not supported, but return true to avoid spurious log messages
	return true;
}


bool FSocketBSD::SetSendBufferSize(int32 Size,int32& NewSize)
{
	SOCKLEN SizeSize = sizeof(int32);
	bool bOk = setsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&Size,sizeof(int32)) == 0;

	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}


bool FSocketBSD::SetReceiveBufferSize(int32 Size,int32& NewSize)
{
	SOCKLEN SizeSize = sizeof(int32);
	bool bOk = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&Size,sizeof(int32)) == 0;

	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}


int32 FSocketBSD::GetPortNo(void)
{
	sockaddr_in Addr;

	SOCKLEN Size = sizeof(sockaddr_in);

	// Figure out what ip/port we are bound to
	bool bOk = getsockname(Socket, (sockaddr*)&Addr, &Size) == 0;
	
	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to read address for socket (%s)"), SocketSubsystem->GetSocketError());
	}

	// Read the port number
	return ntohs(Addr.sin_port);
}

#endif	//PLATFORM_HAS_BSD_SOCKETS
