// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FLiveEditorListenServer;

namespace nLiveEditorListenServer
{

class FCreateListener : public FUObjectArray::FUObjectCreateListener
{
public:
	FCreateListener( FLiveEditorListenServer *_Owner );
	~FCreateListener();
	virtual void NotifyUObjectCreated(const class UObjectBase *Object, int32 Index);

private:
	FLiveEditorListenServer *Owner;
};

class FDeleteListener : public FUObjectArray::FUObjectDeleteListener
{
public:
	FDeleteListener( FLiveEditorListenServer *_Owner );
	~FDeleteListener();
	virtual void NotifyUObjectDeleted(const class UObjectBase *Object, int32 Index);

private:
	FLiveEditorListenServer *Owner;
};

class FTickObject : FTickableGameObject
{
public:
	FTickObject( FLiveEditorListenServer *_Owner );

	virtual void Tick(float DeltaTime);

	virtual bool IsTickable() const
	{
		return true;
	}
	virtual bool IsTickableWhenPaused() const
	{
		return true;
	}
	virtual bool IsTickableInEditor() const
	{
		return false;
	}
	virtual TStatId GetStatId() const OVERRIDE;

private:
	FLiveEditorListenServer *Owner;
};

class FMapLoadObserver : public TSharedFromThis<FMapLoadObserver>
{
public:
	FMapLoadObserver( FLiveEditorListenServer *_Owner );
	~FMapLoadObserver();

	void OnPreLoadMap();
	void OnPostLoadMap();

private:
	FLiveEditorListenServer *Owner;
};

}

