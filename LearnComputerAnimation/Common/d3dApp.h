#pragma once
// Debug
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif


#include "d3dUtil.h"
#include "GameTimer.h"



// 链接d3d12库
#pragma  comment(lib ,"d3dcompiler.lib")
#pragma  comment(lib ,"D3D12.lib")
#pragma  comment(lib ,"dxgi.lib")
// debug dx12

class D3DApp
{
protected:
    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();
protected:
    virtual void OnResize();
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;

    // 鼠标事件回调
    virtual void OnMouseDown(WPARAM btnState,int x,int y){}
    virtual void OnMouseUp(WPARAM btnState,int x,int y){}
    virtual void OnMouseMove(WPARAM btnState,int x,int y){}
    
public:
    static D3DApp* GetApp();

    HINSTANCE AppInst() const;
    HWND MainWnd() const;
    float AspectRatio() const;

    bool Get4xMsaaState() const;
    void Set4xMsaaState(bool value);

    int Run();
    
    virtual bool Initialize();
    // Windows消息处理函数。鼠标事件、键盘事件、退出与resize等.
    virtual LRESULT MsgProc(HWND hwnd,UINT msg,WPARAM wParam, LPARAM lParam);

    // 打印显示设备
    void LogAdapters();

    // 当前SwapChain中的back buffer view.(返回的是描述符)
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferDescriptor() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilDescriptor() const;

    ID3D12Resource* CurrentBackBuffer() const;
protected:
    bool InitMainWindow();
    bool InitDirect3D();

    // 等待CommandQueue执行完毕.
    void FlushCommandQueue();

    
protected:
    static D3DApp* mApp;
    HINSTANCE mhAppInst;    // 应用程序实例

    HWND mhMainWnd;         // Windows窗口实例
    


    // D3D 属性
    Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;             // dxgi工厂
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;              // dxgi 交换链
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;     // dxgi Backbuffer format.
    static const int SwapChainBufferCount = 2;                      // 交换链缓冲数
    int mCurrBackBuffer = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource>  mSwapChainBuffer[SwapChainBufferCount];      // RT buffer.
    Microsoft::WRL::ComPtr<ID3D12Resource>  mDepthStencilBuffer;    // ds buffer
    Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;                // d
    // 命令相关
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;       
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    UINT64 mCurrentFence;   // 当前Fence值.

    // 描述符大小
    UINT mRtvDescriptorSize;
    UINT mDsvDescriptorSize;
    UINT mCbvUavDescriptorSize;

    // 描述符堆
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    // 视口和裁剪区域
    D3D12_VIEWPORT      mScreenViewport;
    D3D12_RECT          mScissorRect;

    int mClientWidth = 800;
    int mClientHeight = 600;
    bool mAppPaused;        // 是否暂停
    bool mMinimized=false;  // 是否最小化
    bool mMaximized=false;  // 是否最大化
    bool mResizing;

    bool m4xMsaaState;      // 切换4xMsaa状态
    DXGI_FORMAT mDepthStencilFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;    
    GameTimer mTimer;       // 计时器
    
    std::wstring mMainWndCaption = L"LearnDx12";  //Windows窗口标题
};
