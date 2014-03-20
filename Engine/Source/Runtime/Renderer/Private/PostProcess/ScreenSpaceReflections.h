// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenSpaceReflections.h: Post processing Screen Space Reflections implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDepthDownSample : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;
};

// ePId_Input0: half res scene color
// ePId_Input1: half res scene depth
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessScreenSpaceReflections : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCPassPostProcessScreenSpaceReflections( bool InPrevFrame )
	: bPrevFrame( InPrevFrame )
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;

private:
	bool bPrevFrame;
};


// ePId_Input0: half res scene color
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessApplyScreenSpaceReflections : public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;
};

void ScreenSpaceReflections( const FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& SSROutput );

bool DoScreenSpaceReflections(const FViewInfo& View);
