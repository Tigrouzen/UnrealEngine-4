// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FLevelEditorViewportClient;
struct FViewportClick;
struct HGeomEdgeProxy;
struct HGeomPolyProxy;
struct HGeomVertexProxy;

/**
 * A hit proxy class for sockets in the main editor viewports.
 */
struct HLevelSocketProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	AActor* Actor;
	USceneComponent* SceneComponent;
	FName SocketName;

	HLevelSocketProxy(AActor* InActor, USceneComponent* InSceneComponent, FName InSocketName)
		:	HHitProxy(HPP_UI)
		,   Actor( InActor )
		,   SceneComponent( InSceneComponent )
		,	SocketName( InSocketName )
	{}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) OVERRIDE
	{
		Collector.AddReferencedObject( Actor );
		Collector.AddReferencedObject( SceneComponent );
	}
};

namespace ClickHandlers
{
	bool ClickActor(FLevelEditorViewportClient* ViewportClient,AActor* Actor,const FViewportClick& Click,bool bAllowSelectionChange);

	void ClickBrushVertex(FLevelEditorViewportClient* ViewportClient,ABrush* InBrush,FVector* InVertex,const FViewportClick& Click);

	void ClickStaticMeshVertex(FLevelEditorViewportClient* ViewportClient,AActor* InActor,FVector& InVertex,const FViewportClick& Click);
	
	bool ClickGeomPoly(FLevelEditorViewportClient* ViewportClient, HGeomPolyProxy* InHitProxy, const FViewportClick& Click);

	bool ClickGeomEdge(FLevelEditorViewportClient* ViewportClient, HGeomEdgeProxy* InHitProxy, const FViewportClick& Click);

	bool ClickGeomVertex(FLevelEditorViewportClient* ViewportClient,HGeomVertexProxy* InHitProxy,const FViewportClick& Click);
	
	void ClickSurface(FLevelEditorViewportClient* ViewportClient,UModel* Model,int32 iSurf,const FViewportClick& Click);
	
	void ClickBackdrop(FLevelEditorViewportClient* ViewportClient,const FViewportClick& Click);

	void ClickLevelSocket(FLevelEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
};


