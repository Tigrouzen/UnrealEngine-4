// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TileRendering.h: Simple tile rendering implementation.
=============================================================================*/

#ifndef _INC_TILERENDERING
#define _INC_TILERENDERING

class FTileRenderer
{
public:

	/**
	 * Draw a tile at the given location and size, using the given UVs
	 * (UV = [0..1]
	 */
	ENGINE_API static void DrawTile(const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, bool bIsHitTesting=false, const FHitProxyId HitProxyId=FHitProxyId());

private:

	/** This class never needs to be instantiated. */
	FTileRenderer() {}
};

#endif
