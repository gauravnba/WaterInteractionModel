//--------------------------------------------------------------------------------------
// File: main.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

/*
* Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/

#include "utilities.h"
#include <DXUT.h>
#include <DXUTgui.h>
#include <DXUTmisc.h>
#include <DXUTCamera.h>
#include <DXUTSettingsDlg.h>
#include <SDKmisc.h>

#include "ocean_simulator.h"
#include "skybox.h"

#pragma warning (disable:4005)

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager  g_DialogResourceManager;    // manager for shared resources of dialogs
CFirstPersonCamera          g_Camera;                   // A model viewing camera
CD3DSettingsDlg             g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                      // dialog for standard controls
CDXUTDialog                 g_SampleUI;                 // dialog for sample specific controls

CDXUTTextHelper*            g_pTxtHelper = NULL;

// Ocean simulation variables
ocean_simulator* g_pocean_simulator = NULL;

bool g_RenderWireframe = false;
bool g_PauseSimulation = false;
int g_BufferType = 0;

// Skybox
ID3D11Texture2D* g_pSkyCubeMap = NULL;
ID3D11ShaderResourceView* g_pSRV_SkyCube = NULL;

CSkybox11 g_Skybox;

// for time measurement
int __initialize_globals();

__int64 g_freq;
int __dummy_initialize_globals = __initialize_globals();

int __initialize_globals() {
    QueryPerformanceFrequency((LARGE_INTEGER*) &g_freq);
    return 0;
}

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_CHANGEDEVICE        4
#define IDC_WIREFRAME   5
#define IDC_PAUSE    6

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

// init & cleanup
void initRenderResource(const ocean_parameter& ocean_param, ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
void cleanupRenderResource();
// Rendering routines
void renderShaded(const CBaseCamera& camera, ID3D11ShaderResourceView* displacemnet_map, ID3D11ShaderResourceView* gradient_map, float time, ID3D11DeviceContext* pd3dContext);
void renderWireframe(const CBaseCamera& camera, ID3D11ShaderResourceView* displacemnet_map, float time, ID3D11DeviceContext* pd3dContext);
void renderLogo(ID3D11DeviceContext* pd3dContext);

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain ); 
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();

    // Force create a ref device so that feature level D3D_FEATURE_LEVEL_11_0 is guaranteed
    DXUTInit( true, true ); // Parse the command line, show msgboxes on error, no extra command line params

    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"C++ AMP Ocean Simulation" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1280, 720 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 32, iY, 140, 26 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 32, iY += 30, 140, 26, VK_F2 );
    g_HUD.AddButton( IDC_WIREFRAME, L"Toggle wireframe", 32, iY += 30, 140, 26, VK_F4 );
    g_HUD.AddButton( IDC_PAUSE, L"Pause", 32, iY += 30, 140, 26, VK_F5 );

    g_SampleUI.SetCallback( OnGUIEvent ); 

    g_Camera.SetRotateButtons( true, false, false );
    g_Camera.SetScalers(0.003f, 400.0f);
}

//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( pDeviceSettings->ver == DXUT_D3D11_DEVICE );

    // Disable vsync
    pDeviceSettings->d3d11.SyncInterval = 0;
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );    

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    // Update simulation
    static double app_time = 0;
    if (g_PauseSimulation == false)
    {
        app_time += (double)fElapsedTime;
        g_pocean_simulator->update_displacement_map((float)app_time);
    }
}

void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    LPCWSTR frame_stat = DXUTGetFrameStats( DXUTIsVsyncEnabled() );
    frame_stat += 6;
    g_pTxtHelper->DrawTextLine( frame_stat );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
    case IDC_TOGGLEFULLSCREEN:
        DXUTToggleFullScreen(); break;
    case IDC_CHANGEDEVICE:
        g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
    case IDC_WIREFRAME:
        g_RenderWireframe = !g_RenderWireframe;
        break;
    case IDC_PAUSE:
        if (g_PauseSimulation == false)
        {
            g_HUD.GetButton(IDC_PAUSE)->SetText(L"Resume");
            g_PauseSimulation = true;
        }
        else
        {
            g_HUD.GetButton(IDC_PAUSE)->SetText(L"Pause");
            g_PauseSimulation = false;
        }
        break;
    }

}

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, 
                                      const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, 
                                      bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnD3D11DestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options;
    hr = pd3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &options, sizeof(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS));
    if( !options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
    {
        MessageBox(NULL, L"Compute Shaders are not supported on your hardware", L"Unsupported HW", MB_OK);
        return E_FAIL;
    }

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );

    // Initialize the font
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye(1562.24f, 854.291f, -1224.99f);
    D3DXVECTOR3 vecAt (1562.91f, 854.113f, -1225.71f);
    g_Camera.SetViewParams(&vecEye, &vecAt);

    // Create ocean simulating object
    // Ocean object
    ocean_parameter ocean_param;

    g_pocean_simulator = new ocean_simulator(ocean_param, pd3dDevice);

    // Update the simulation for the first time.
    g_pocean_simulator->update_displacement_map(0);

    // D3D11 resources for rendering
    initRenderResource(ocean_param, pd3dDevice, pBackBufferSurfaceDesc);

    // Sky box
    D3DX11CreateShaderResourceViewFromFile(pd3dDevice, (WCHAR *)GetFilePath::GetFilePath(_T("Media\\sky_cube.dds")).c_str(), NULL, NULL, &g_pSRV_SkyCube, NULL);
    assert(g_pSRV_SkyCube);

    g_pSRV_SkyCube->GetResource((ID3D11Resource**)&g_pSkyCubeMap);
    assert(g_pSkyCubeMap);

    g_Skybox.OnD3D11CreateDevice(pd3dDevice, 50, g_pSkyCubeMap, g_pSRV_SkyCube);

    return S_OK;
}

HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams(D3DX_PI/4, fAspectRatio, 200.0f, 400000.0f);

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 180, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    // Sky box
    g_Skybox.OnD3D11ResizedSwapChain(pBackBufferSurfaceDesc);

    return S_OK;
}

void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    float ClearColor[4] = { 0.1f,0.2f,0.4f,0.0f };
    // Clear the main render target
    pd3dImmediateContext->ClearRenderTargetView( DXUTGetD3D11RenderTargetView(), ClearColor );
    // Clear the depth stencil
    pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D10_CLEAR_DEPTH, 1.0, 0 );

    // Sky box rendering
    D3DXMATRIXA16 mView = D3DXMATRIX(1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1) * *g_Camera.GetViewMatrix();
    D3DXMATRIXA16 mProj = *g_Camera.GetProjMatrix();
    D3DXMATRIXA16 mWorldViewProjection = mView * mProj;

    if (!g_RenderWireframe)
        g_Skybox.D3D11Render( &mWorldViewProjection, pd3dImmediateContext );

    // Time
    static double app_time = fTime;
    static double app_prev_time = fTime;
    if (g_PauseSimulation == false)
        app_time += fTime - app_prev_time;
    app_prev_time = fTime;

    // Ocean rendering
    ID3D11ShaderResourceView* tex_displacement = g_pocean_simulator->get_direct3d_displacement_map();
    ID3D11ShaderResourceView* tex_gradient = g_pocean_simulator->get_direct3d_gradient_map();
    if (g_RenderWireframe)
        renderWireframe(g_Camera, tex_displacement, (float)app_time, pd3dImmediateContext);
    else
        renderShaded(g_Camera, tex_displacement, tex_gradient, (float)app_time, pd3dImmediateContext);

    // HUD rendering
    g_HUD.OnRender( fElapsedTime ); 
    g_SampleUI.OnRender( fElapsedTime );

    // Logo
    renderLogo(pd3dImmediateContext);

    RenderText();
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnD3D11CreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    cleanupRenderResource();

    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    // Ocean object
    SAFE_DELETE(g_pocean_simulator);

    // Sky box
    g_Skybox.OnD3D11DestroyDevice();
}

void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

    g_Skybox.OnD3D11ReleasingSwapChain();
}