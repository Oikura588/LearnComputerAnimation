#pragma once
#include <d3d12.h>
#include <wrl/client.h>

// 使用示例：初始化时根据物体的Size和Number创建一大块buffer，绘制的时候根据物体内部存储的索引值来找到对应的缓冲区子区域然后修改数据即可.
template<typename T>
class UploadBuffer
{
public:
    // 初始化
    UploadBuffer(ID3D12Device* device,UINT elementCount,bool isConstantBuffer)
        :mIsConstantBuffer(isConstantBuffer)
    {
        mElementByteSize = sizeof(T);

        if(isConstantBuffer)
        {
            mElementByteSize = d3dUtil::CalcConstantBufferByteSize(mElementByteSize);
        }
        // 常量缓冲区大小位256B的整数倍.硬件只能按照m*256B的偏移量和n*256B的数据长度来查看常量数据
        // typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC
        // {
        // UINT64 OffsetInBytes;   //256的整数倍
        // UINT SizeInBytes;       //256的整数倍
        // };
        // 如果是常量缓冲区，重新计算Buffer大小
        D3D12_HEAP_PROPERTIES heapProperty;
        heapProperty.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProperty.CreationNodeMask = 1;
        heapProperty.VisibleNodeMask = 1;
        heapProperty.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperty.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;

        D3D12_RESOURCE_DESC heapResourceDesc;
        heapResourceDesc.Alignment = 0 ;
        heapResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER ;      //buffer是一维数组
        heapResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        heapResourceDesc.Format = DXGI_FORMAT_UNKNOWN;                      //这里使用Unknow,大概是因为不知道buffer会写入什么数据
        heapResourceDesc.Height = 1;
        heapResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;           //buffer是一维的，所以用Row Major?
        heapResourceDesc.Width = mElementByteSize*elementCount;             //宽度是数据大小.
        heapResourceDesc.MipLevels = 1;
        heapResourceDesc.SampleDesc.Count = 1;
        heapResourceDesc.SampleDesc.Quality = 0;
        heapResourceDesc.DepthOrArraySize = 1;

        // 上传堆的资源需要被GPU读取，所以是可读状态.
        device->CreateCommittedResource(
            &heapProperty,
            D3D12_HEAP_FLAG_NONE,
            &heapResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mUploadBuffer)
        );

        // 对于缓冲区来说，自身就是唯一的子资源，所以填0;
        // 第二个是范围，若为空，整体映射;
        // 第三个是映射的内存块。用memcpy将运行时数据拷贝到这里上传到gpu
        mUploadBuffer->Map(0,nullptr,reinterpret_cast<void**>(&mMappedData));
    }
    // 禁止拷贝和赋值
    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
    ~UploadBuffer()
    {
        if(mUploadBuffer!=nullptr)
        {
            mUploadBuffer->Unmap(0,nullptr);
        }
        mMappedData = nullptr;
    }
    // 获取Buffer资源
    ID3D12Resource* Resource() const
    {
        return mUploadBuffer.Get();
    }
    // 上传数据.
    void CopyData(int elementIndex,const T&data)
    {
        memcpy(&mMappedData[elementIndex*mElementByteSize],&data,sizeof(T));
    }
    
    

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
    BYTE* mMappedData = nullptr;    // 映射资源数据的内存块。用memcpy可以拷贝数据到这个地址，从而完成数据的上传.
    
    bool mIsConstantBuffer;     //判断是否是常量buffer，如果是的话，元素大小要256B对齐，会影响到mElementSize.
    UINT mElementByteSize;
};
