// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLensFlares.h: Post processing lens flares implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// ePId_Input0: Bloom
// ePId_Input1: Lensflare image input
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessLensFlares : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	FRCPassPostProcessLensFlares(float InSizeScale);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;

private:
	float SizeScale;
};
