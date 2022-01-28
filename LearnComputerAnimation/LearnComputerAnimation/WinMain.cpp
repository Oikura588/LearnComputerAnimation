// Be sure to include glad before glfw.
#include <windows.h>
#include <iostream>
#include "Constants.h"
#include "Vertex.h"
#include "Model.h"
#include "../Common/d3dApp.h"
#include "../Common/DDSTextureLoader.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;

static const int gNumFrameResources = 3;



// 存储绘制一个物体需要的数据的结构，随着不同的程序有所差别.
struct RenderItem
{
	RenderItem() = default;
	// 用一个flag来计数，与FrameResource有关，每次判断这个值是否大于等于0来判断是否需要更新数据.
	int NumFramesDirty = gNumFrameResources;
	// 物体的世界Transform.用这个格式来存储，可以直接memcpy到buffer中.
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	// 常量缓冲区中的偏移，如第10个物体就在缓冲区中第十个位置.
	UINT ObjCBOffset = -1;
	// 几何体的引用.几何体中存储了VertexBuffer和IndexBuffer.
	MeshGeometry* Geo = nullptr;
	// 图元类型
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	// DrawInstance 的参数
	UINT IndexCount;
	UINT StartIndexLocation;
	int BaseVertexLocation;
	// 材质
	Material* Mat = nullptr;
	// 蒙皮缓冲区索引
	UINT SkinnedCBIndex = -1;
	// 运行时模型实例
	ModelInstance* SkinnedModelInst = nullptr;
};

// 以CPU每帧都需更新的资源作为基本元素，包括CmdListAlloc、ConstantBuffer等.
// Draw()中进行绘制时，执行CmdList的Reset函数，来指定当前FrameResource所使用的CmdAlloc,从而将绘制命令存储在每帧的Alloc中.
class FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT MaterialCount, UINT skinnedCount)
	{
		device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(CmdAlloc.GetAddressOf())
		);
		ObjectsCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
		PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
		MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, MaterialCount, true);
		SkinnedCB = std::make_unique<UploadBuffer<SkinnedConstants>>(device, skinnedCount, true);

	}
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;

	// GPU处理完与此Allocator相关的命令前，不能对其重置，所以每帧都需要保存自己的Alloc.
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdAlloc;

	// 在GPU处理完此ConstantBuffer相关的命令前，不能对其重置
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectsCB = nullptr;
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
	std::unique_ptr<UploadBuffer<SkinnedConstants>> SkinnedCB = nullptr;

	// 每帧需要有自己的fence，来判断GPU与CPU的帧之间的同步.
	UINT64 Fence = 0;
};



class LearnComputerAnimApp :public D3DApp
{
public:
	LearnComputerAnimApp(HINSTANCE hInstance):D3DApp(hInstance){}
	LearnComputerAnimApp(const LearnComputerAnimApp&) = delete;
	LearnComputerAnimApp& operator=(const LearnComputerAnimApp&) =delete;
	~LearnComputerAnimApp(){};


	bool Initialize() override;
private:
	virtual void OnResize() override;
	void Update(const GameTimer& gt) override;
	void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	
private:
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	UINT mSkinnedSrvHeapStart = 0;
	ComPtr<ID3D12DescriptorHeap> mSamplerHeap;
	UINT mPassCbvOffset;
	UINT mMaterialCbvOffset;
	UINT mSrvOffset;
	UINT mSkinOffset;

	// 存储所有几何体,中包含了顶点、索引的buffer及view，方便管理几何体.
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	// 顶点输入布局
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;	// 蒙皮网格，包含骨骼索引和权重信息.
	// Shaders.
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	// 材质
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	// 纹理
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	// Pipeline state object.
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	// 是否开启线框模式
	bool mIsWireframe = false;


	// 存储所有渲染项.
	std::vector<std::unique_ptr<RenderItem>> mAllRenderItems;

	// 所有渲染帧.
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameIndex = 0;

	// Camera
	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV2;
	float mRadius = 5.0f;
	XMFLOAT3 mEyePos;
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();


	// 键盘输入
	POINT mLastMousePos;

	// 模型信息(程序中只会渲染一个模型)
	std::string mSkinnedModelFileName = "Models\\soldier.m3d";
	Model mModel;
	std::vector<M3DLoader::Subset> mSkinnedSubsets;
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;
	std::vector<std::string> mSkinnedTextureNames;
	// 骨骼模型实例信息
	std::unique_ptr<ModelInstance> mSkinnedModelInst = nullptr;


};

bool LearnComputerAnimApp::Initialize()
{
	if(!D3DApp::Initialize())
	{
		return false;
	}
	// 重置命令列表来执行初始化命令
	ThrowIfFailed( mCommandList->Reset(mDirectCmdListAlloc.Get(),nullptr));

	// 加载模型
	{
		std::vector<SkinnedVertex> vertices;
		std::vector<std::uint16_t> indices;

		// 骨骼的默认信息
		std::vector<XMFLOAT4X4> boneOffsets;
		std::vector<int> boneIndexToParentIndex;
		std::unordered_map<std::string, AnimationClip> animationClips;

		M3DLoader m3dLoader;
		m3dLoader.LoadM3d(mSkinnedModelFileName,vertices,indices,mSkinnedSubsets,mSkinnedMats,mModel);

		// 创建实例
		mSkinnedModelInst = std::make_unique<ModelInstance>();
		mSkinnedModelInst->ModelInfo = &mModel;
		mSkinnedModelInst->FinalTransforms.resize(mModel.BoneCount());
		// 暂时不处理动画信息.

		const UINT vbByteSize = (UINT) vertices.size()* sizeof(SkinnedVertex);
		const UINT ibByteSize = (UINT) indices.size()* sizeof(std::uint16_t);
		auto geo = std::make_unique<MeshGeometry>();
		geo->Name = mSkinnedModelFileName;
		geo->IndexBufferByteSize = ibByteSize;
		geo->VertexBufferByteSize = vbByteSize;
		geo->VertexByteStride = sizeof(SkinnedVertex);
		geo->IndexFormat = DXGI_FORMAT_R16_UINT;

		// 加载submesh
		for(UINT i=0;i<(UINT)mSkinnedSubsets.size();++i)
		{
			SubmeshGeometry submesh;
			std::string name = "sm_" + std::to_string(i);

			submesh.IndexCount = (UINT)mSkinnedSubsets[i].FaceCount * 3;
			submesh.StartIndexLocation = mSkinnedSubsets[i].FaceStart * 3;
			submesh.BaseVertexLocation = 0;

			geo->DrawArgs[name] = submesh;
		}

		// 创建buffer.
		geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),mCommandList.Get(),vertices.data(),vbByteSize,geo->VertexBufferUploader);
		geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

		
		mGeometries[geo->Name] = std::move(geo);
	}

	// 加载纹理
	{
		std::vector<std::string> texNames;
		std::vector<std::wstring> texFileName;
		// 遍历模型文件来加载
		for(UINT i=0;i<mSkinnedMats.size();++i)
		{
			std::string diffuseName = mSkinnedMats[i].DiffuseMapName;
			std::wstring diffuseFilename = L"Textures/" + AnsiToWString(diffuseName);
			// 去掉后缀
			diffuseName = diffuseName.substr(0, diffuseName.find_last_of("."));

			mSkinnedTextureNames.push_back(diffuseName);
			texNames.push_back(diffuseName);
			texFileName.push_back(diffuseFilename);

			// 查找是否有重名，如果没有，创建resource.
			if (mTextures.find(diffuseName) == std::end(mTextures))
			{
				auto texMap = std::make_unique<Texture>();
				texMap->Name = diffuseName;
				texMap->FileName = diffuseFilename;
				ThrowIfFailed( CreateDDSTextureFromFile12(md3dDevice.Get(),mCommandList.Get(),diffuseFilename.c_str(),texMap->Resource,texMap->UploadHeap));
				mTextures[diffuseName] = std::move(texMap);
			}	
		}
	}
	// 创建根参数
	// 根参数的组成:
	// 0. object cbv
	// 1. material cbv
	// 2. pass cbv.
	// 3. srv table
	// 4. sampler	table
	// 5. model cbv.
	{


		D3D12_ROOT_PARAMETER slotRootParameter[6];
		// obj cbv
		slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameter[0].Descriptor.RegisterSpace = 0;
		slotRootParameter[0].Descriptor.ShaderRegister = 0;			//b0
		// mat cbv.
		slotRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		slotRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameter[1].Descriptor.RegisterSpace = 0;	
		slotRootParameter[1].Descriptor.ShaderRegister = 1;			//b1
		// pass cbv.
		slotRootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		slotRootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameter[2].Descriptor.RegisterSpace = 0;
		slotRootParameter[2].Descriptor.ShaderRegister = 2;			//b2

		// srv table
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.BaseShaderRegister = 0;			//t0
		srvRange.NumDescriptors = 1;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.RegisterSpace = 0;

		D3D12_ROOT_DESCRIPTOR_TABLE srvTable = {};
		srvTable.NumDescriptorRanges = 1;
		srvTable.pDescriptorRanges = &srvRange;
		slotRootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		slotRootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameter[3].DescriptorTable = srvTable;

		// sampler table
		D3D12_DESCRIPTOR_RANGE samplerDescRange;
		samplerDescRange.NumDescriptors = 1;
		samplerDescRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		samplerDescRange.RegisterSpace = 0;
		samplerDescRange.BaseShaderRegister = 0;	//s0
		samplerDescRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_DESCRIPTOR_TABLE samplerDescTable;
		samplerDescTable.pDescriptorRanges = &samplerDescRange;
		samplerDescTable.NumDescriptorRanges = 1;
		slotRootParameter[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		slotRootParameter[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameter[4].DescriptorTable = samplerDescTable;

		// model cbv
		slotRootParameter[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		slotRootParameter[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameter[5].Descriptor.RegisterSpace = 0;
		slotRootParameter[5].Descriptor.ShaderRegister = 3;			//b3

		// d3d12规定，必须将根签名的描述布局进行序列化，才可以传入CreateRootSignature方法.
		ComPtr<ID3DBlob> serializedBlob = nullptr, errBlob = nullptr;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.NumParameters = 6;
		rootSignatureDesc.pParameters = slotRootParameter;
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;

		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedBlob.GetAddressOf(), errBlob.GetAddressOf());
		ThrowIfFailed(hr);
		md3dDevice->CreateRootSignature(
			0,
			serializedBlob->GetBufferPointer(),
			serializedBlob->GetBufferSize(),
			IID_PPV_ARGS(mRootSignature.GetAddressOf())
		);

	}

	// 初始化采样器堆及采样器
	{
		// 采样器
		D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
		samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		samplerHeapDesc.NumDescriptors = 1;

		md3dDevice->CreateDescriptorHeap(
			&samplerHeapDesc, IID_PPV_ARGS(mSamplerHeap.GetAddressOf())
		);
		// 采样器
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		// 仅限于3D纹理
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		md3dDevice->CreateSampler(&samplerDesc, mSamplerHeap->GetCPUDescriptorHandleForHeapStart());

	}
	// 创建cbv/srv heap.
	{
		// 只有纹理用到了cbvheap
		UINT textureCount = (UINT)mSkinnedTextureNames.size();
		// 总数量为textureCount 个描述符
		mSrvOffset =  0;
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.NumDescriptors = textureCount;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.NodeMask = 0;

		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
			&cbvHeapDesc,
			IID_PPV_ARGS(mCbvHeap.GetAddressOf())
		));

		// 绑定纹理
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceDesc = {};
		shaderResourceDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			
		for(UINT i=0;i<(UINT)mSkinnedTextureNames.size();++i)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mCbvHeap->GetCPUDescriptorHandleForHeapStart();
			srvHandle.ptr += (mSkinnedSrvHeapStart + i)*mCbvUavDescriptorSize;

			auto texResource = mTextures[mSkinnedTextureNames[i]]->Resource;
			shaderResourceDesc.Format = texResource->GetDesc().Format;
			shaderResourceDesc.Texture2D.MipLevels = texResource->GetDesc().MipLevels;
			md3dDevice->CreateShaderResourceView(
				texResource.Get(), &shaderResourceDesc, srvHandle
			);
		}
		
	}

	// 创建shader
	{
		const D3D_SHADER_MACRO skinnedDefines[] =
		{
				"SKINNED", "1",
				NULL, NULL
		};	

		mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
		mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", skinnedDefines, "VS", "vs_5_1");
		mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");
		mInputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		mSkinnedInputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
	}


	// 创建材质(材质需要引用texture在srv中的偏移，所以在根常量、描述符之后创建)
	{
		UINT matCBIndex = 0;
		UINT srvHeapIndex = mSkinnedSrvHeapStart;
		for(UINT i = 0;i<mSkinnedMats.size();++i)
		{
			auto mat = std::make_unique<Material>();
			mat->Name = mSkinnedMats[i].Name;
			mat->MatCBIndex = matCBIndex++;
			mat->DiffuseSrvHeapIndex = srvHeapIndex++;
			mat->DiffuseAlbedo = mSkinnedMats[i].DiffuseAlbedo;
			mat->FresnelR0 = mSkinnedMats[i].FresnelR0;
			mat->Roughness = mSkinnedMats[i].Roughness;
			mat->NumFramesDirty = gNumFrameResources;
			mMaterials[mat->Name] = std::move(mat);
		}
	}

	// 创建渲染项
	{
		// 模型的渲染项根据材质来区分
		UINT objCBIndex = 0;
		for(UINT i=0;i<mSkinnedMats.size();++i)
		{
			std::string submeshName = "sm_" + std::to_string(i);
			auto ritem = std::make_unique<RenderItem>();
			// Reflect to change coordinate system from the RHS the data was exported out as.
			XMMATRIX modelScale = XMMatrixScaling(0.05f, 0.05f, -0.05f);
			XMMATRIX modelRot = XMMatrixRotationY(0.F);
			XMMATRIX modelOffset = XMMatrixTranslation(0.0f, -2.f, 0.f);
			XMStoreFloat4x4(&ritem->World, modelScale* modelRot* modelOffset);

			ritem->ObjCBOffset = objCBIndex++;
			ritem->Mat = mMaterials[mSkinnedMats[i].Name].get();
			ritem->Geo = mGeometries[mSkinnedModelFileName].get();
			ritem->PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			ritem->IndexCount = ritem->Geo->DrawArgs[submeshName].IndexCount;
			ritem->StartIndexLocation = ritem->Geo->DrawArgs[submeshName].StartIndexLocation;
			ritem->BaseVertexLocation = ritem->Geo->DrawArgs[submeshName].BaseVertexLocation;
			
			// 同一模型的所有的模型渲染项引用共同的实例
			ritem->SkinnedCBIndex = 0;
			ritem->SkinnedModelInst = mSkinnedModelInst.get();
			mAllRenderItems.push_back(std::move(ritem));
		}
	}

	// 创建frame resource.初始化时需要材质数量等信息，因此放在后面
	{
		for (int i = 0; i < gNumFrameResources; ++i)
		{
			mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
				1, (UINT)mAllRenderItems.size(),
				(UINT)mMaterials.size(),1));
		}
	}

	// 创建PSO.根据shader数目来创建，同时加一个wireframe的pso
	{
		// Pso for opauqe objects.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
		ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
		opaquePsoDesc.pRootSignature = mRootSignature.Get();
		opaquePsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
			mShaders["standardVS"]->GetBufferSize()
		};
		opaquePsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
			mShaders["opaquePS"]->GetBufferSize()
		};
		opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		opaquePsoDesc.SampleMask = UINT_MAX;
		opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		opaquePsoDesc.NumRenderTargets = 1;
		opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
		opaquePsoDesc.SampleDesc.Count =  1;
		opaquePsoDesc.SampleDesc.Quality = 0;
		opaquePsoDesc.DSVFormat = mDepthStencilFormat;
		ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

		// PSO for skinned pass.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
		skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
		skinnedOpaquePsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
			mShaders["skinnedVS"]->GetBufferSize()
		};
		skinnedOpaquePsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
			mShaders["opaquePS"]->GetBufferSize()
		};
		ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));

		// wireframe
		D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframePSO = opaquePsoDesc;
		wireframePSO.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&wireframePSO, IID_PPV_ARGS(&mPSOs["wireframe"])));
	
	}



	// 执行初始化命令
	mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// 等待初始化完成.
	FlushCommandQueue();

	return true;

}

void LearnComputerAnimApp::OnResize()
{
	D3DApp::OnResize();

	// 更新纵横比、重算投影矩阵.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25 * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void LearnComputerAnimApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	// Frame resource buffer.
	mCurrentFrameIndex = (mCurrentFrameIndex + 1) % gNumFrameResources;
	mCurrentFrameResource = mFrameResources[mCurrentFrameIndex].get();

	// 如果队列满，等待
	if (mCurrentFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrentFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	// 更新相机位置
	{
		// 更新相机位置
		float x = mRadius * sinf(mPhi) * cosf(mTheta);
		float z = mRadius * sinf(mPhi) * sinf(mTheta);
		float y = mRadius * cosf(mPhi);

		// View matrix.
		XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
		XMStoreFloat3(&mEyePos, pos);
		XMVECTOR target = XMVectorZero();
		XMVECTOR up = XMVectorSet(0.f, 1, 0.f, 0.f);
		XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
		XMStoreFloat4x4(&mView, view);
	}

	// 更新物体CB
	{
		auto currObjCB = mCurrentFrameResource->ObjectsCB.get();
		for(auto& e:mAllRenderItems)
		{
			if (e->NumFramesDirty > 0)
			{
				e->NumFramesDirty--;
				XMMATRIX world = XMLoadFloat4x4(&e->World);

				ObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
				currObjCB->CopyData(e->ObjCBOffset, objConstants);
			}
		}
	}
	// 更新动画的CB
	{
		auto currSkinCB = mCurrentFrameResource->SkinnedCB.get();
		SkinnedConstants skinnedConstants;
		std::copy(std::begin(mSkinnedModelInst->FinalTransforms),std::end(mSkinnedModelInst->FinalTransforms),&skinnedConstants.BoneTransform[0]);
		currSkinCB->CopyData(0,skinnedConstants);
	}
	// 更新材质的CB
	{
		auto currMaterialCB = mCurrentFrameResource->MaterialCB.get();
		for (auto& e : mMaterials)
		{
			Material* mat = e.second.get();
			if (mat->NumFramesDirty > 0)
			{
				MaterialConstants matConstants;
				matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
				matConstants.FresnelR0 = mat->FresnelR0;
				matConstants.MaterialTransform = mat->MatTransform;
				matConstants.Roughness = mat->Roughness;
				currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
				mat->NumFramesDirty--;
			}
		}
	}
	// 更新Pass的CB
	{
		XMMATRIX view = XMLoadFloat4x4(&mView);
		XMMATRIX proj = XMLoadFloat4x4(&mProj);
		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);
		auto currPassCB = mCurrentFrameResource->PassCB.get();
		PassConstants passConstants;

		XMStoreFloat4x4(&passConstants.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&passConstants.InvView, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(view), view)));
		XMStoreFloat4x4(&passConstants.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&passConstants.InvProj, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(proj), proj)));
		XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&passConstants.InvViewProj, XMMatrixTranspose(invViewProj));
		passConstants.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
		passConstants.InvRenderTargetSize = XMFLOAT2(1 / (float)mClientWidth, 1 / (float)mClientHeight);
		passConstants.EyePosW = mEyePos;
		passConstants.NearZ = 1.0f;
		passConstants.FarZ = 1000.0f;
		passConstants.DeltaTime = gt.DeltaTime();
		passConstants.TotalTime = gt.TotalTime();

		// 环境光
		passConstants.AmbientLight = XMFLOAT4(0.25f, 0.25f, 0.35f, 1.0f);
		// 三个光源
		passConstants.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
		passConstants.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
		passConstants.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
		passConstants.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
		passConstants.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
		passConstants.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

		currPassCB->CopyData(0, passConstants);
	}


}

void LearnComputerAnimApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrentFrameResource->CmdAlloc;
	cmdListAlloc->Reset();
	// 3 pso:skinnedOpaque,opaque,wireframe
	if (mIsWireframe)
	{
		mCommandList->Reset(cmdListAlloc.Get(), mPSOs["wireframe"].Get());

	}
	else
	{	
		// 目前只绘制一个模型，不考虑默认
		mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get());
		//mCommandList->Reset(cmdListAlloc.Get(), mPSOs["skinnedOpaque"].Get());

	}

	// 设置视口
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);
	// 开始绘制
	mCommandList->ResourceBarrier(
		1, &CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);
	// 清除颜色和深度buffer
	mCommandList->ClearRenderTargetView(CurrentBackBufferDescriptor(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilDescriptor(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferDescriptor(), true, &DepthStencilDescriptor());
	// 指定根参数
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	// 设置采样器
	ID3D12DescriptorHeap* samplerHeaps[] = { mSamplerHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(samplerHeaps), samplerHeaps);
	mCommandList->SetGraphicsRootDescriptorTable(4, mSamplerHeap->GetGPUDescriptorHandleForHeapStart());

	// 更新PassConstants.
	mCommandList->SetGraphicsRootConstantBufferView(2,mCurrentFrameResource->PassCB->Resource()->GetGPUVirtualAddress());

	// 设置描述符堆为cbv srv heap
	ID3D12DescriptorHeap* cbvHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(cbvHeaps), cbvHeaps);
	// 绘制物体
	{
		UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
		UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
		UINT skinCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
		auto objCB = mCurrentFrameResource->ObjectsCB->Resource();
		// 目前都在一个pass内
		for (size_t i = 0; i < mAllRenderItems.size(); ++i)
		{
			auto ri = mAllRenderItems[i].get();
			mCommandList->IASetVertexBuffers(0,1,&ri->Geo->VertexBufferView());
			mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
			mCommandList->IASetPrimitiveTopology(ri->PrimitiveTopology);

			// 设置obj cbv
			D3D12_GPU_VIRTUAL_ADDRESS objAddress = mCurrentFrameResource->ObjectsCB->Resource()->GetGPUVirtualAddress();
			objAddress += ri->ObjCBOffset*objCBByteSize;
			mCommandList->SetGraphicsRootConstantBufferView(0, objAddress);
			// 设置材质 cbv
			D3D12_GPU_VIRTUAL_ADDRESS matAddress = mCurrentFrameResource->MaterialCB->Resource()->GetGPUVirtualAddress();
			matAddress += ri->Mat->MatCBIndex *matCBByteSize;
			mCommandList->SetGraphicsRootConstantBufferView(1, matAddress);
			// 设置纹理 
			D3D12_GPU_DESCRIPTOR_HANDLE texHandle = mCbvHeap->GetGPUDescriptorHandleForHeapStart();
			texHandle.ptr += (mSrvOffset  + ri->Mat->DiffuseSrvHeapIndex) * mCbvUavDescriptorSize;
			mCommandList->SetGraphicsRootDescriptorTable(3, texHandle);
			// 设置模型
			D3D12_GPU_VIRTUAL_ADDRESS modelAddress = mCurrentFrameResource->SkinnedCB->Resource()->GetGPUVirtualAddress();
			mCommandList->SetGraphicsRootConstantBufferView(5, modelAddress);

			mCommandList->DrawIndexedInstanced(ri->IndexCount,1,ri->StartIndexLocation,ri->BaseVertexLocation,0);
		}
	}



	// 结束绘制
	  // 绘制完成后改变资源状态.
	mCommandList->ResourceBarrier(
		1, &CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);
	mCommandList->Close();

	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// 增加Fence
	mCurrentFence++;
	mCurrentFrameResource->Fence = mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

















void LearnComputerAnimApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void LearnComputerAnimApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();

}

void LearnComputerAnimApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void LearnComputerAnimApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
	{
		mIsWireframe = true;
	}
	else
	{
		mIsWireframe = false;
	}
}

// debug模式下开启命令行窗口
#if _DEBUG
#pragma comment(linker,"/subsystem:console" )
int main(int argc,const char** argv)
{
    return WinMain(GetModuleHandle(NULL),NULL,GetCommandLineA(),SW_SHOWDEFAULT);
}
#else
#pragma  comment(linker,"/subsystem:windows")
#endif

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try
	{
		LearnComputerAnimApp theApp(hInstance);

		if (!theApp.Initialize())
		{
			return 0;
		}
		return theApp.Run();
	}
	catch (DxException e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}


