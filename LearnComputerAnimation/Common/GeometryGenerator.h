#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include <vector>
#include <string>

#include "MathHelper.h"

class GeometryGenerator
{
public:
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    // 顶点信息，坐标、法线、切线、纹理坐标.
    struct Vertex{
        Vertex(){}
        Vertex(
            const DirectX::XMFLOAT3& p,
            const DirectX::XMFLOAT3& n,
            const DirectX::XMFLOAT3& t,
            const DirectX::XMFLOAT2& uv
            ):Position(p),Normal(n),TangentU(t),TexC(uv)
            {}
        Vertex(
            float px,float py,float pz,
            float nx,float ny,float nz,
            float tx,float ty,float tz,
            float u ,float v
        ):Position(px,py,pz),Normal(nx,ny,nz),TangentU(tx,ty,tz),TexC(u,v){}
        
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 TangentU;
        DirectX::XMFLOAT2 TexC;
    };

    // 存储顶点和索引信息.
    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<uint32> Indices32;
        std::vector<uint16>& GetIndices16()
        {
            if(mIndices16.empty())
            {
                mIndices16.resize(Indices32.size());
                for(size_t i=0;i<Indices32.size();++i)
                {
                    mIndices16[i]=static_cast<uint16>(Indices32[i]);
                }
            }
            return mIndices16;
        }
    private:
        std::vector<uint16> mIndices16;
    };
    // 球
	static MeshData CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);
    // 几何球
    static MeshData CreateGeosphere(float radius, uint32 numSubdivisions);

    // 圆柱体.
    static MeshData CreateCylinder(float bottomRadius,float topRadius,float height,uint32 sliceCount,uint32 stackCount);

    // box
    static MeshData CreateBox(float width,float height,float depth,uint32 numSubdivision);

	/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
    static MeshData CreateGrid(float width,float depth,uint32 m,uint32 n);

    // 进行一次细分.
    static void Subdivide(MeshData& meshData);

    static Vertex MidPoint(const Vertex& v0,const Vertex& v1);

    // 加载模型
    static MeshData LoadModel(std::string path);

};
