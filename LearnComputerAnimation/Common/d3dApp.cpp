#include "d3dApp.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Forward hwnd on because we can get messages (e.g., WM_CREATE)
    // before CreateWindow returns, and thus before mhMainWnd is valid.
    return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp::D3DApp(HINSTANCE hInstance)
    :mhAppInst(hInstance)
{
    // 只能创建一次.
    assert(mApp==nullptr);
    mApp = this;
}

D3DApp::~D3DApp()
{
    
}

void D3DApp::OnResize()
{
}

void D3DApp::Update(const GameTimer& gt)
{
}

void D3DApp::Draw(const GameTimer& gt)
{
}

D3DApp* D3DApp::GetApp()
{
    return mApp;
}

HINSTANCE D3DApp::AppInst() const
{
    return mhAppInst;
}

HWND D3DApp::MainWnd() const
{
    return mhMainWnd;
}

float D3DApp::AspectRatio() const
{
    return static_cast<float>(mClientWidth)/mClientHeight;
}

bool D3DApp::Get4xMsaaState() const
{
    return true;
}

void D3DApp::Set4xMsaaState(bool value)
{
}

int D3DApp::Run()
{
    MSG msg = {0};
    mTimer.Reset();
    // 主循环
    while(msg.message!=WM_QUIT)
    {
        if(PeekMessage(&msg,0,0,0,PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            mTimer.Tick();
            if(!mAppPaused)
            {
                Update(mTimer);
                Draw(mTimer);
            }
            else
            {
                Sleep(100);
            }
        }
    }
    return (int)msg.wParam;
    
}

bool D3DApp::Initialize()
{
    // 初始化Windows
    if(!InitMainWindow())
    {
        return false;
    }
    // 初始化D3D
    if(!InitDirect3D())
    {
        return false;
    }
    OnResize();
    // Do the initial resize code .
    // OnResize();
    return true;
    
}
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // WM_ACTIVATE is sent when the window is activated or deactivated.
        case WM_ACTIVATE:
            if(LOWORD(wParam) == WA_INACTIVE)
            {
                mAppPaused = true;
                mTimer.Stop();
            }
            else
            {
                mAppPaused = false;
                mTimer.Start();
            }
        return 0;
    

        // WM_SIZE is sent when the user resize the windows.
        case WM_SIZE:
            mClientWidth = LOWORD(lParam);
        mClientHeight = HIWORD(lParam);
        if(md3dDevice)
        {
            if(wParam == SIZE_MINIMIZED)
            {
                mAppPaused = true;
                mMinimized = true;
                mMaximized = false;
            }
            else if(wParam == SIZE_MAXIMIZED)
            {
                mAppPaused = true;
                mMinimized = false;
                mMaximized = true;
            }
            else if( mResizing)
            {
                // If user is dragging the resize bars, we do not resize 
                // the buffers here because as the user continuously 
                // drags the resize bars, a stream of WM_SIZE messages are
                // sent to the window, and it would be pointless (and slow)
                // to resize for each WM_SIZE message received from dragging
                // the resize bars.  So instead, we reset after the user is 
                // done resizing the window and releases the resize bars, which 
                // sends a WM_EXITSIZEMOVE message.
            }
            else
            {
                OnResize();
            }
        }
        return 0;
        // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
        case WM_ENTERSIZEMOVE:
            mAppPaused = true;
        mResizing = true;
        mTimer.Stop();
        return 0;
    case WM_EXITSIZEMOVE:
        mAppPaused = false;
        mResizing = false;
        mTimer.Start();
        OnResize();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        OnMouseDown(wParam,GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        OnMouseUp(wParam,GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(wParam,GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
        return 0;
    case WM_KEYUP:
        if(wParam==VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        else if((int)wParam == VK_F2)
            Set4xMsaaState(!m4xMsaaState);
        return 0;
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

void D3DApp::LogAdapters()
{
    UINT i=0;
    IDXGIAdapter* adapter = nullptr;
    std::vector<IDXGIAdapter*> adapterList;
    while(mdxgiFactory->EnumAdapters(i,&adapter)!=DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";
        OutputDebugString(text.c_str());
        adapterList.push_back(adapter);
        i++;
    }

    for(size_t i = 0;i<adapterList.size();++i)
    {
        ReleaseCom(adapterList[i]);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferDescriptor() const
{
    // 根据偏移找到back buffer的rtv
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mCurrBackBuffer,
        mRtvDescriptorSize
    );
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilDescriptor() const
{
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

ID3D12Resource* D3DApp::CurrentBackBuffer() const
{
    return mSwapChainBuffer[mCurrBackBuffer].Get();
}

bool D3DApp::InitMainWindow()
{
    WNDCLASS wc;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc; 
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = mhAppInst;
    wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName  = 0;
    wc.lpszClassName = L"MainWnd";

    if( !RegisterClass(&wc) )
    {
        MessageBox(0, L"RegisterClass Failed.", 0, 0);
        return false;
    }

    // Compute window rectangle dimensions based on requested client area dimensions.
    RECT R = { 0, 0, mClientWidth, mClientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    int width  = R.right - R.left;
    int height = R.bottom - R.top;

    mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(), 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0); 
    if( !mhMainWnd )
    {
        MessageBox(0, L"CreateWindow Failed.", 0, 0);
        return false;
    }

    ShowWindow(mhMainWnd, SW_SHOW);
    UpdateWindow(mhMainWnd);

    return true;
}

bool D3DApp::InitDirect3D()
{
#if defined(DEBUG)|defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    ThrowIfFailed( D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)) );
    debugController->EnableDebugLayer();

    // 加载dll
    if (GetModuleHandle(L"WinPixGpuCapturer.dll") == 0)
    {
        LoadLibrary(GetLatestWinPixGpuCapturerPath().c_str());
    }
#endif
    // DXGI:DirectX Graphics Infrastructure.DX图形基础结构.
    // 基本理念是使得多种图形API所共用的底层任务能通过其来统一处理，如2D与3D动画统一使用的交换链、页面翻转等功能.
    // 其他常用功能：切换全屏与窗口，枚举显卡、显示设备、显示模式等
    // 定义了D3D支持的各种表现格式信息(DXGI_FORMAT)
    ThrowIfFailed( CreateDXGIFactory(IID_PPV_ARGS(&mdxgiFactory)) );

#ifdef _DEBUG
    LogAdapters();
#endif

    
    
    
    // 尝试创建硬件.nullptr表示使用默认设备
    HRESULT hardResult =  D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&md3dDevice)
    );
    // 如果失败直接false.
    // todo 参考龙书逻辑，使用dxgi创建软光栅.
    if(FAILED(hardResult))
    {
        return false;
    }

    // GPU创建命令队列.
    // 每个GPU至少维护着一个命令队列，是个RingBuffer.
    // CPU通过CommandList 把命令到提交 GPU的CommandQueue中
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT ;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        ThrowIfFailed( md3dDevice->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&mCommandQueue)));
        
    }
    // CPU创建命令分配器
    // CommandList内部的命令实际存储在命令分配器上
    // (思考：看上去我们是直接调用CmdList中的函数来执行命令，实际上应该是通过某种方式把调用的函数以及参数序列化到Allocator中，GPU读取这段数据然后来还原.)
    {
        md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&mDirectCmdListAlloc));
    }
    // CPU创建命令列表
    {
        md3dDevice->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,mDirectCmdListAlloc.Get(),nullptr,IID_PPV_ARGS(&mCommandList));
        // 调用关闭。第一次引用命令列表时，要进行reset。reset要在关闭后才能执行。
        mCommandList->Close();
    }
    
    // 创建Fence对象.用来同步.
    {
        md3dDevice->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&mFence)
        );
    }

    // 下面创建的内容需要先重置下CommandList来打开.
    //mDirectCmdListAlloc.Reset();
    mCommandList->Reset(mDirectCmdListAlloc.Get(),nullptr);

    // dxgi内容，创建交换链。交换链依赖CommandQueue.
    // 由于每次窗口大小改变后都需要改变缓冲区大小，因此RTV、DSV相关的内容都应该放到OnResize回调中.
    {
        // 释放之前的Swapchain，重新创建
        mSwapChain.Reset();
        DXGI_SWAP_CHAIN_DESC sd;

        // 这里制定了rtv的格式。
        sd.BufferDesc.Width = mClientWidth;
        sd.BufferDesc.Height = mClientHeight;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = mBackBufferFormat;
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        // 4x msaa 配置.先填成固定值
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;

        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = SwapChainBufferCount;
        sd.OutputWindow = mhMainWnd;
        sd.Windowed = true;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        // 交换链由通过命令队列直接管理
        ThrowIfFailed( mdxgiFactory->CreateSwapChain(
            mCommandQueue.Get(),
            &sd,
            mSwapChain.GetAddressOf()
        ));
    }

    // 获取描述符大小.不同GPU平台各异，但是获取一次即可。
    {
        mRtvDescriptorSize =   md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        mDsvDescriptorSize =  md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        mCbvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // 创建描述符堆.
    {
        // 渲染目标视图需要的数目为交换链的数目.因为要交换
        // 深度模板视图只需要一个.
        // Cbv目前不需要

        // rtv堆里只存储了2个rtv表舒服
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NodeMask = 0;
        rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
        md3dDevice->CreateDescriptorHeap(
            &rtvHeapDesc,
            IID_PPV_ARGS(mRtvHeap.GetAddressOf())
        );

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NodeMask = 0;
        dsvHeapDesc.NumDescriptors = 1;
        md3dDevice->CreateDescriptorHeap(
            &dsvHeapDesc,
            IID_PPV_ARGS(mDsvHeap.GetAddressOf())
        );
    }
    
    // 创建BufferView.
    {
        // 思考 为什么同样是view，RenderTargetView不需要resource，而dsv需要？
        // 这里的view其实也是对resource的描述符
        // 创建流程应该是现有资源然后创建描述符.
        // 其中RenderTarget由于资源是Swapchain的Buffer，所以不需要pDesc就可以直接创建
        // 而DepthStencil的buffer还没有资源，因此需要先创建资源再创建view.
        // Render target view.
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for(UINT i=0;i<SwapChainBufferCount;++i)
        {
            mSwapChain->GetBuffer(
                i,
                IID_PPV_ARGS(&mSwapChainBuffer[i])
            );

            // 由于已经制定了后台缓冲区的格式，rtv的desc可以为nullptr.
            md3dDevice->CreateRenderTargetView(
                mSwapChainBuffer[i].Get(),
                nullptr,
                rtvHandle
            );
            rtvHandle.Offset(1,mRtvDescriptorSize);
        }

        // Depth/stencil buffer and view.
        // DS Buffer是一种2D纹理
        D3D12_RESOURCE_DESC dsDesc;
        dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dsDesc.Alignment = 0;
        // Texture2D类型的width和height就是图像的高度和宽度
        dsDesc.Width = mClientWidth;
        dsDesc.Height = mClientHeight;
        // 对于1D和2D来说，是纹理数组的大小。只有一张纹理，所以是1.
        dsDesc.DepthOrArraySize = 1;
        // 对于DepthStencil来说，只有一个Mip级别.
        dsDesc.MipLevels = 1 ;
        // D是指Depth,S是指Stencil.这里的意思是无符号24位深度缓冲区，并映射到[0,1]区间，8位uint分配给模板缓冲区.
        dsDesc.Format = mDepthStencilFormat;;
        dsDesc.SampleDesc.Count = 1;
        dsDesc.SampleDesc.Quality = 0;
        // 暂时不处理.
        dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        // 根据描述创建GPU资源.Gpu资源都存在堆中.使用CreateCommittedResource.创建一个资源与一个堆，并上传到堆中.
        // 深度缓冲区应该放到默认堆中，因为CPU不需要访问.
        
        // 创建资源时可以指定清除资源时的优化值.
        D3D12_CLEAR_VALUE dsClearValue;
        dsClearValue.Format = mDepthStencilFormat;
        dsClearValue.DepthStencil.Depth = 1.0f;
        dsClearValue.DepthStencil.Stencil = 0;

        D3D12_HEAP_PROPERTIES dsHeapProperties;
        dsHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        dsHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        dsHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        dsHeapProperties.VisibleNodeMask = 1;
        dsHeapProperties.CreationNodeMask = 1;
                
        
        md3dDevice->CreateCommittedResource(
            &dsHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &dsDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &dsClearValue,
            IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())
        );

        // Buffer创建好了，绑定Descriptor.
        md3dDevice->CreateDepthStencilView(
            mDepthStencilBuffer.Get(),
            nullptr,
            DepthStencilDescriptor()
        );

        // DSV 创建好后要从初始状态转成DepthWrite状态
        // 思考，为什么创建的时候要指定成Common而不是Write？
        // 这里还是mCommandList的第一条语句.
        mCommandList->ResourceBarrier(
            1,
            &CD3DX12_RESOURCE_BARRIER::Transition(
                mDepthStencilBuffer.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_DEPTH_WRITE
            )
        );
    }

    // 设置视口和裁剪矩形
    {
        mScreenViewport.TopLeftX = 0;
        mScreenViewport.TopLeftY = 0;
        mScreenViewport.Width = mClientWidth;
        mScreenViewport.Height = mClientHeight;
        mScreenViewport.MinDepth = 0;
        mScreenViewport.MaxDepth = 1;
        mCommandList->RSSetViewports(1,&mScreenViewport);

        mScissorRect = {0,0,mClientWidth,mClientHeight};
        mCommandList->RSSetScissorRects(1,&mScissorRect);
    }
    mCommandList->Close();

    ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists),cmdLists);
    FlushCommandQueue();
    return true;
}

void D3DApp::FlushCommandQueue()
{
    // 原理是向CommandQueue添加一个设置新Fence值的命令(GPU并不会马上执行，而是会等当前命令执行完毕)。然后监听当前CommandQueue的值直到等于这个新设置的值.
    mCurrentFence++;
    // 思考，这个Signal命令为什么不需要CommandList来提交?
    mCommandQueue->Signal(mFence.Get(),mCurrentFence);
    if(mFence->GetCompletedValue()<mCurrentFence)
    {
        // 创建一个空的handle回调，用来阻塞程序直到触发fence点
        HANDLE eventHandle = CreateEventEx(nullptr,false,false,EVENT_ALL_ACCESS);
        mFence->SetEventOnCompletion(mCurrentFence,eventHandle);
        WaitForSingleObject(eventHandle,INFINITE);
        CloseHandle(eventHandle);
    }
}
