#pragma once

#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <stdint.h>
#include <sstream>
#include "d3dx12.h"


#include <fstream>
#include <unordered_map>
#include <shlobj.h>
#include <strsafe.h>
#include "MathHelper.h"

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP,0,str.c_str(),-1,buffer,512);
    return std::wstring(buffer);
}
// 调试用


static std::wstring GetLatestWinPixGpuCapturerPath()
{
    LPWSTR programFilesPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

    std::wstring pixSearchPath = programFilesPath + std::wstring(L"\\Microsoft PIX\\*");

    WIN32_FIND_DATA findData;
    bool foundPixInstallation = false;
    wchar_t newestVersionFound[MAX_PATH];

    HANDLE hFind = FindFirstFile(pixSearchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) &&
                (findData.cFileName[0] != '.'))
            {
                if (!foundPixInstallation || wcscmp(newestVersionFound, findData.cFileName) <= 0)
                {
                    foundPixInstallation = true;
                    StringCchCopy(newestVersionFound, _countof(newestVersionFound), findData.cFileName);
                }
            }
        } while (FindNextFile(hFind, &findData) != 0);
    }

    FindClose(hFind);

    if (!foundPixInstallation)
    {
        // TODO: Error, no PIX installation found
    }

    wchar_t output[MAX_PATH];
    StringCchCopy(output, pixSearchPath.length(), pixSearchPath.data());
    StringCchCat(output, MAX_PATH, &newestVersionFound[0]);
    StringCchCat(output, MAX_PATH, L"\\WinPixGpuCapturer.dll");

    return &output[0];
}

// 常用函数
class d3dUtil
{
public:
    // 计算常量缓冲区ByteSize，方便内存对齐.
    static UINT CalcConstantBufferByteSize(UINT byteSize);

    // 创建顶点缓冲区、索引缓冲区等默认buffer，同时创建uploadbuffer来初始化.
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
    // 编译Shader
	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);
};

// 子几何体
struct SubmeshGeometry
{
    // 只需要记录索引数、开始索引位置即可。DrawInstance函数可以自动加上顶点数的偏移.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    INT BaseVertexLocation = 0;

    // Bounding box.
};

// 辅助几何体.
struct MeshGeometry
{
    // 几何体的名称.
    std::string Name;

    // 内存的副本？由于顶点和索引是泛型，所以用blob，用户使用时再转换类型.
    // 思考，作用应该是同时保存CPU、GPU中的buffer.
    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    // 在GPU中的Buffer为资源.
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // 与缓冲区相关的数据.
    // 由于Buffer格式不确定,几何体要记录自己的大小、步长信息，才能根据偏移正确读取
    UINT VertexByteStride =0;
    UINT VertexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    UINT IndexBufferByteSize = 0;
    
    // 一个MeshGeometry可以存储一组缓冲区的多个几何体(相同顶点、索引类型)
    std::unordered_map<std::string,SubmeshGeometry> DrawArgs;
    
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.SizeInBytes = VertexBufferByteSize;
        vbv.StrideInBytes = VertexByteStride;
        return vbv;
    }
    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.SizeInBytes = IndexBufferByteSize ;
        ibv.Format = IndexFormat;
        return ibv;
    }
};

// 材质
struct Material 
{
    // 便于查找调试
    std::string Name;

    // 材质的常量缓冲区索引
    int MatCBIndex = -1;

    int NumFramesDirty = 0;

    // 材质参数
    DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f,1.0f,1.0f,1.0f};
    DirectX::XMFLOAT3 FresnelR0 = {0.01,0.01f,0.01f};
    float Roughness = 0.25f;
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

    // diffuse 贴图的索引,目前材质只包含一个漫反射纹理
    int DiffuseSrvHeapIndex = -1;
};

// 纹理
struct Texture
{
    // 便于查找测试
    std::string Name;
    std::wstring FileName;

    Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

// 光源
struct Light
{
    DirectX::XMFLOAT3 Strength = {0.5f,0.5f,0.5f};  // 光源的颜色 
    float FalloffStart = 1.0f;                      // 使用范围             点光源     聚光灯
    DirectX::XMFLOAT3 Direction ={0.f,-1.0f,0.f};   // 适用范围     方向光             聚光灯
    float FalloffEnd = 10.0f;                       //                     点光       聚光灯
    DirectX::XMFLOAT3 Position ={0.f,0.f,0.f};      //                     点光       聚光灯
    float SpotPower = 64.0f;                        //                                聚光灯
};

#define MaxLights 16

// DxException类

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr ,const std::wstring& functionName , const std::wstring& filename,int lineNumber );
    std::wstring ToString() const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};


// 方便开发的
#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                            \
{                                                                   \
    HRESULT hr__ = (x);                                             \
    std::wstring wfn = AnsiToWString(__FILE__);                     \
    if(FAILED(hr__)) {throw DxException(hr__,L#x,wfn,__LINE__);}    \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) {if(x){x->Release();x=0;}}
#endif