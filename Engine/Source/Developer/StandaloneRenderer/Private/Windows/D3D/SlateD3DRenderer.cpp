// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "StandaloneRendererPrivate.h"

#include "Windows/D3D/SlateD3DRenderer.h"
#include "Windows/D3D/SlateD3DTextureManager.h"
#include "Windows/D3D/SlateD3DRenderingPolicy.h"
#include "ElementBatcher.h"
#include "FontCache.h"

TRefCountPtr<ID3D11Device> GD3DDevice;
TRefCountPtr<ID3D11DeviceContext> GD3DDeviceContext;

static FMatrix CreateProjectionMatrixD3D( uint32 Width, uint32 Height )
{
	// Create ortho projection matrix
	const float Left = 0.0f;//0.5f
	const float Right = Left+Width;
	const float Top = 0.0f;//0.5f
	const float Bottom = Top+Height;
	const float ZNear = 0;
	const float ZFar = 1;

	return	FMatrix( FPlane(2.0f/(Right-Left),			0,							0,					0 ),
					 FPlane(0,							2.0f/(Top-Bottom),			0,					0 ),
					 FPlane(0,							0,							1/(ZNear-ZFar),		0 ),
					 FPlane((Left+Right)/(Left-Right),	(Top+Bottom)/(Bottom-Top),	ZNear/(ZNear-ZFar), 1 ) );

}


FSlateD3DRenderer::FSlateD3DRenderer( const ISlateStyle& InStyle )
{

	ViewMatrix = FMatrix(	FPlane(1,	0,	0,	0),
							FPlane(0,	1,	0,	0),
							FPlane(0,	0,	1,  0),
							FPlane(0,	0,	0,	1));

}

FSlateD3DRenderer::~FSlateD3DRenderer()
{
}

class FSlateD3DFontAtlasFactory : public ISlateFontAtlasFactory
{
public:

	virtual ~FSlateD3DFontAtlasFactory()
	{
	}

	virtual TSharedRef<FSlateFontAtlas> CreateFontAtlas() const OVERRIDE
	{
		/** Size of each font texture, width and height */
		const uint32 TextureSize = 1024;

		return MakeShareable( new FSlateFontAtlasD3D( TextureSize, TextureSize ) );
	}
};

void FSlateD3DRenderer::Initialize()
{
	CreateDevice();


	TextureManager = MakeShareable( new FSlateD3DTextureManager );
	TextureManager->LoadUsedTextures();

	FontCache = MakeShareable( new FSlateFontCache( MakeShareable( new FSlateD3DFontAtlasFactory ) ) );
	FontMeasure = FSlateFontMeasure::Create( FontCache.ToSharedRef() );

	RenderingPolicy = MakeShareable( new FSlateD3D11RenderingPolicy( FontCache, TextureManager.ToSharedRef() ) );

	ElementBatcher = MakeShareable( new FSlateElementBatcher( RenderingPolicy.ToSharedRef() ) );
}

void FSlateD3DRenderer::Destroy()
{
	FSlateShaderParameterMap::Get().Shutdown();
	ElementBatcher.Reset();
	RenderingPolicy.Reset();
	TextureManager.Reset();
	GD3DDevice.SafeRelease();
	GD3DDeviceContext.SafeRelease();
}
void FSlateD3DRenderer::CreateDevice()
{
	if( !IsValidRef( GD3DDevice ) || !IsValidRef( GD3DDeviceContext ) )
	{
		// Init D3D
		uint32 DeviceCreationFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
		D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_HARDWARE;

		if( FParse::Param( FCommandLine::Get(), TEXT("d3ddebug") ) )
		{
			DeviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
		}

		const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3 };
		D3D_FEATURE_LEVEL CreatedFeatureLevel;
		HRESULT Hr = D3D11CreateDevice( NULL, DriverType, NULL, DeviceCreationFlags, FeatureLevels, sizeof(FeatureLevels)/sizeof(D3D_FEATURE_LEVEL), D3D11_SDK_VERSION, GD3DDevice.GetInitReference(), &CreatedFeatureLevel, GD3DDeviceContext.GetInitReference() );
		check( SUCCEEDED(Hr) );
	}
}

FSlateDrawBuffer& FSlateD3DRenderer::GetDrawBuffer()
{
	DrawBuffer.ClearBuffer();
	return DrawBuffer;
}

void FSlateD3DRenderer::LoadStyleResources( const ISlateStyle& Style )
{
	if ( TextureManager.IsValid() )
	{
		TextureManager->LoadStyleResources( Style );
	}
}

void FSlateD3DRenderer::Private_CreateViewport( TSharedRef<SWindow> InWindow, const FVector2D &WindowSize )
{
	TSharedRef< FGenericWindow > NativeWindow = InWindow->GetNativeWindow().ToSharedRef();

	bool bFullscreen = IsViewportFullscreen( *InWindow );
	bool bWindowed = true;//@todo implement fullscreen: !bFullscreen;

	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	FMemory::Memzero(&SwapChainDesc, sizeof(SwapChainDesc) );
	SwapChainDesc.BufferCount = 1;
	SwapChainDesc.BufferDesc.Width = FMath::Trunc(WindowSize.X);
	SwapChainDesc.BufferDesc.Height = FMath::Trunc(WindowSize.Y);
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SwapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
	SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.OutputWindow = (HWND)NativeWindow->GetOSWindowHandle();
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.Windowed = bWindowed;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	TRefCountPtr<IDXGIDevice> DXGIDevice;
	HRESULT Hr = GD3DDevice->QueryInterface( __uuidof(IDXGIDevice), (void**)DXGIDevice.GetInitReference() );
	check( SUCCEEDED(Hr) );

	TRefCountPtr<IDXGIAdapter> DXGIAdapter;
	Hr = DXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)DXGIAdapter.GetInitReference() );
	check( SUCCEEDED(Hr) );

	TRefCountPtr<IDXGIFactory> DXGIFactory;
	DXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)DXGIFactory.GetInitReference() );
	check( SUCCEEDED(Hr) );

	FSlateD3DViewport Viewport;

	Hr = DXGIFactory->CreateSwapChain(DXGIDevice.GetReference(), &SwapChainDesc, Viewport.D3DSwapChain.GetInitReference() );
	check( SUCCEEDED(Hr) );

	uint32 Width = FMath::Trunc(WindowSize.X);
	uint32 Height = FMath::Trunc(WindowSize.Y);

	Viewport.ViewportInfo.MaxDepth = 1.0f;
	Viewport.ViewportInfo.MinDepth = 0.0f;
	Viewport.ViewportInfo.Width = Width;
	Viewport.ViewportInfo.Height = Height;
	Viewport.ViewportInfo.TopLeftX = 0;
	Viewport.ViewportInfo.TopLeftY = 0;
	
	CreateBackBufferResources( Viewport.D3DSwapChain, Viewport.BackBufferTexture, Viewport.RenderTargetView );

	Viewport.ProjectionMatrix = CreateProjectionMatrixD3D( Width, Height );

	WindowToViewportMap.Add( &InWindow.Get(), Viewport );

}

void FSlateD3DRenderer::RequestResize( const TSharedPtr<SWindow>& InWindow, uint32 NewSizeX, uint32 NewSizeY )
{
	const bool bFullscreen = IsViewportFullscreen( *InWindow );
	Private_ResizeViewport( InWindow.ToSharedRef(), NewSizeX, NewSizeY, bFullscreen );
}

void FSlateD3DRenderer::UpdateFullscreenState( const TSharedRef<SWindow> InWindow, uint32 OverrideResX, uint32 OverrideResY )
{
	//@todo not implemented
}

void FSlateD3DRenderer::ReleaseDynamicResource( const FSlateBrush& Brush )
{
	TextureManager->ReleaseDynamicTextureResource( Brush );
}

bool FSlateD3DRenderer::GenerateDynamicImageResource(FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes)
{
	return TextureManager->CreateDynamicTextureResource(ResourceName, Width, Height, Bytes) != NULL;
}

void FSlateD3DRenderer::Private_ResizeViewport( const TSharedRef<SWindow> InWindow, uint32 Width, uint32 Height, bool bFullscreen )
{
	FSlateD3DViewport* Viewport = WindowToViewportMap.Find( &InWindow.Get() );

	if( Viewport && ( Width != Viewport->ViewportInfo.Width || Height != Viewport->ViewportInfo.Height || bFullscreen != Viewport->bFullscreen ) )
	{
		GD3DDeviceContext->OMSetRenderTargets(0,NULL,NULL);

		Viewport->BackBufferTexture.SafeRelease();
		Viewport->RenderTargetView.SafeRelease();
		Viewport->DepthStencilView.SafeRelease();

		Viewport->ViewportInfo.Width = Width;
		Viewport->ViewportInfo.Height = Height;
		Viewport->bFullscreen = bFullscreen;
		Viewport->ProjectionMatrix = CreateProjectionMatrixD3D( Width, Height );

		DXGI_SWAP_CHAIN_DESC Desc;
		Viewport->D3DSwapChain->GetDesc( &Desc );
		HRESULT Hr = Viewport->D3DSwapChain->ResizeBuffers( Desc.BufferCount, Viewport->ViewportInfo.Width, Viewport->ViewportInfo.Height, Desc.BufferDesc.Format, Desc.Flags );
		check( SUCCEEDED(Hr) );

		CreateBackBufferResources( Viewport->D3DSwapChain, Viewport->BackBufferTexture,Viewport->RenderTargetView );
	}
}

void FSlateD3DRenderer::CreateBackBufferResources( TRefCountPtr<IDXGISwapChain>& InSwapChain, TRefCountPtr<ID3D11Texture2D>& OutBackBuffer, TRefCountPtr<ID3D11RenderTargetView>& OutRTV )
{
	InSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void**)OutBackBuffer.GetInitReference() );

	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
	RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;
	HRESULT Hr = GD3DDevice->CreateRenderTargetView( OutBackBuffer, &RTVDesc, OutRTV.GetInitReference() );
	check( SUCCEEDED(Hr) );
}

void FSlateD3DRenderer::CreateViewport( const TSharedRef<SWindow> InWindow )
{
#if UE_BUILD_DEBUG
	// Ensure a viewport for this window doesnt already exist
	FSlateD3DViewport* Viewport = WindowToViewportMap.Find( &InWindow.Get() );
	check(!Viewport);
#endif

	FVector2D WindowSize = InWindow->GetSizeInScreen();

	Private_CreateViewport( InWindow, WindowSize );
}

void FSlateD3DRenderer::CreateDepthStencilBuffer( FSlateD3DViewport& Viewport )
{
	TRefCountPtr<ID3D11Texture2D> DepthStencil;
	D3D11_TEXTURE2D_DESC DescDepth;

	DescDepth.Width = Viewport.ViewportInfo.Width;
	DescDepth.Height = Viewport.ViewportInfo.Height;
	DescDepth.MipLevels = 1;
	DescDepth.ArraySize = 1;
#if DEPTH_32_BIT_CONVERSION
	DescDepth.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
#else
	DescDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif
	DescDepth.SampleDesc.Count = 1;
	DescDepth.SampleDesc.Quality = 0;
	DescDepth.Usage = D3D11_USAGE_DEFAULT;
	DescDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DescDepth.CPUAccessFlags = 0;
	DescDepth.MiscFlags = 0;
	HRESULT Hr = GD3DDevice->CreateTexture2D( &DescDepth, NULL, DepthStencil.GetInitReference() );
	check( SUCCEEDED(Hr) );

	D3D11_DEPTH_STENCIL_VIEW_DESC DescDSV;
#if DEPTH_32_BIT_CONVERSION
	DescDSV.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT ;
#else
	DescDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT ;
#endif
	DescDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DescDSV.Texture2D.MipSlice = 0;
	DescDSV.Flags = 0;

	// Create the depth stencil view
	Hr = GD3DDevice->CreateDepthStencilView( DepthStencil, &DescDSV, Viewport.DepthStencilView.GetInitReference() ); 
	check( SUCCEEDED(Hr) );
}

void FSlateD3DRenderer::DrawWindows( FSlateDrawBuffer& InWindowDrawBuffer )
{
	// Update the font cache with new text before elements are batched
	FontCache->UpdateCache();

	// Iterate through each element list and set up an RHI window for it if needed
	TArray<FSlateWindowElementList>& WindowElementLists = InWindowDrawBuffer.GetWindowElementLists();
	for( int32 ListIndex = 0; ListIndex < WindowElementLists.Num(); ++ListIndex )
	{
		FSlateWindowElementList& ElementList = WindowElementLists[ListIndex];

		if ( ElementList.GetWindow().IsValid() )
		{
			TSharedRef<SWindow> WindowToDraw = ElementList.GetWindow().ToSharedRef();

			// Add all elements for this window to the element batcher
			ElementBatcher->AddElements( ElementList.GetDrawElements() );

			bool bUnused = false;
			ElementBatcher->FillBatchBuffers( ElementList, bUnused );

			// All elements for this window have been batched and rendering data updated
			ElementBatcher->ResetBatches();

			FVector2D WindowSize = WindowToDraw->GetSizeInScreen();

			FSlateD3DViewport* Viewport = WindowToViewportMap.Find( &WindowToDraw.Get() );
			check(Viewport);

			RenderingPolicy->UpdateBuffers( ElementList );

			check(Viewport);
			GD3DDeviceContext->RSSetViewports(1, &Viewport->ViewportInfo );

			ID3D11RenderTargetView* RTV = Viewport->RenderTargetView;
			ID3D11DepthStencilView* DSV = Viewport->DepthStencilView;

			GD3DDeviceContext->OMSetRenderTargets( 1, &RTV, NULL );

			RenderingPolicy->DrawElements( ViewMatrix*Viewport->ProjectionMatrix, ElementList.GetRenderBatches() );

			GD3DDeviceContext->OMSetRenderTargets(0, NULL, NULL);


			const bool bUseVSync = false;
			Viewport->D3DSwapChain->Present( bUseVSync ? 1 : 0, 0);

			// Mark this window as 'drawn'
			if( !WindowToDraw->HasEverBeenDrawn() )
			{
				WindowToDraw->MarkAsDrawn();
			}

			// All elements have been drawn.  Reset all cached data
			ElementBatcher->ResetBatches();
		}

	}

	// flush the cache if needed
	FontCache->ConditionalFlushCache();

}

void FSlateD3DRenderer::OnWindowDestroyed( const TSharedRef<SWindow>& InWindow )
{
	WindowToViewportMap.Remove(&InWindow.Get());
}
