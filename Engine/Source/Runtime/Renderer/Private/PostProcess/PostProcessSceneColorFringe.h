// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessSceneColorFringe.h: Post processing scene color fringe implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessSceneColorFringe : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;
};
