//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGCommand.h"

#define BIND_PACKED_UAV	ResourceFlags(0x4 | 0x8000)

namespace XUSG
{
	//--------------------------------------------------------------------------------------
	// Constant buffer
	//--------------------------------------------------------------------------------------
	class ConstantBuffer
	{
	public:
		ConstantBuffer();
		virtual ~ConstantBuffer();

		bool Create(const Device &device, uint32_t byteWidth, uint32_t numCBVs = 1,
			const uint32_t *offsets = nullptr, MemoryType memoryType = MemoryType(2),
			const wchar_t *name = nullptr);
		bool Upload(const CommandList &commandList, Resource &uploader, const void *pData,
			size_t size, uint32_t i = 0);

		void *Map(uint32_t i = 0);
		void Unmap();

		const Resource	&GetResource() const;
		Descriptor		GetCBV(uint32_t i = 0) const;

	protected:
		Descriptor allocateCbvPool(const wchar_t *name);

		Device			m_device;

		Resource		m_resource;
		std::vector<DescriptorPool>	m_cbvPools;
		std::vector<Descriptor>	m_cbvs;
		std::vector<uint32_t> m_cbvOffsets;

		void			*m_pDataBegin;
	};

	//--------------------------------------------------------------------------------------
	// Resource base
	//--------------------------------------------------------------------------------------
	class ResourceBase
	{
	public:
		ResourceBase();
		virtual ~ResourceBase();

		uint32_t SetBarrier(ResourceBarrier *pBarriers, ResourceState dstState,
			uint32_t numBarriers = 0, uint32_t subresource = 0xffffffff,
			BarrierFlags flags = BarrierFlags(0));

		const Resource	&GetResource() const;
		Descriptor		GetSRV(uint32_t i = 0) const;

		ResourceBarrier	Transition(ResourceState dstState, uint32_t subresource = 0xffffffff,
			BarrierFlags flags = BarrierFlags(0));
		ResourceState	GetResourceState(uint32_t i = 0) const;

		//static void CreateReadBuffer(const Device &device,
			//CPDXBuffer &pDstBuffer, const CPDXBuffer &pSrcBuffer);
	protected:
		void setDevice(const Device &device);
		Descriptor allocateSrvUavPool();

		Device			m_device;

		Resource		m_resource;
		std::vector<DescriptorPool>	m_srvUavPools;
		std::vector<Descriptor> m_srvs;
		std::vector<ResourceState> m_states;

		std::wstring	m_name;
	};

	//--------------------------------------------------------------------------------------
	// 2D Texture
	//--------------------------------------------------------------------------------------
	class Texture2D :
		public ResourceBase
	{
	public:
		Texture2D();
		virtual ~Texture2D();

		bool Create(const Device &device, uint32_t width, uint32_t height, Format format,
			uint32_t arraySize = 1, ResourceFlags resourceFlags = ResourceFlags(0),
			uint8_t numMips = 1, uint8_t sampleCount = 1, MemoryType memoryType = MemoryType(1),
			ResourceState state = ResourceState(0), bool isCubeMap = false,
			const wchar_t *name = nullptr);
		bool Upload(const CommandList &commandList, Resource &uploader,
			SubresourceData *pSubresourceData, uint32_t numSubresources = 1,
			ResourceState dstState = ResourceState(0), uint32_t i = 0);
		bool Upload(const CommandList &commandList, Resource &uploader, const void *pData,
			uint8_t stride = sizeof(float), ResourceState dstState = ResourceState(0));
		bool CreateSRVs(uint32_t arraySize, Format format = Format(0), uint8_t numMips = 1,
			uint8_t sampleCount = 1, bool isCubeMap = false);
		bool CreateSRVLevels(uint32_t arraySize, uint8_t numMips, Format format = Format(0),
			uint8_t sampleCount = 1, bool isCubeMap = false);
		bool CreateUAVs(uint32_t arraySize, Format format = Format(0), uint8_t numMips = 1);

		Descriptor GetUAV(uint8_t i = 0) const;
		Descriptor GetSRVLevel(uint8_t i) const;

	protected:
		std::vector<Descriptor>	m_uavs;
		std::vector<Descriptor>	m_srvLevels;
		Resource m_counter;
	};

	//--------------------------------------------------------------------------------------
	// Render target
	//--------------------------------------------------------------------------------------
	class RenderTarget :
		public Texture2D
	{
	public:
		RenderTarget();
		virtual ~RenderTarget();

		bool Create(const Device &device, uint32_t width, uint32_t height, Format format,
			uint32_t arraySize = 1, ResourceFlags resourceFlags = ResourceFlags(0),
			uint8_t numMips = 1, uint8_t sampleCount = 1, ResourceState state = ResourceState(0),
			const float *pClearColor = nullptr, bool isCubeMap = false, const wchar_t *name = nullptr);
		bool CreateArray(const Device &device, uint32_t width, uint32_t height, uint32_t arraySize,
			Format format, ResourceFlags resourceFlags = ResourceFlags(0), uint8_t numMips = 1,
			uint8_t sampleCount = 1, ResourceState state = ResourceState(0),
			const float *pClearColor = nullptr, bool isCubeMap = false, const wchar_t *name = nullptr);
		bool CreateFromSwapChain(const Device &device, const SwapChain &swapChain, uint32_t bufferIdx);

		void Populate(const CommandList &commandList, const DescriptorTable &srcSrvTable,
			const DescriptorTable &samplerTable, uint32_t srcSlot = 0, uint32_t samplerSlot = 1,
			uint8_t mipLevel = 0, int32_t slice = 0, const PipelineLayout &pipelineLayout = nullptr,
			const Pipeline &pipeline = nullptr);

		Descriptor	GetRTV(uint32_t slice = 0, uint8_t mipLevel = 0) const;
		uint32_t	GetArraySize() const;
		uint8_t		GetNumMips(uint32_t slice = 0) const;

	protected:
		bool create(const Device &device, uint32_t width, uint32_t height,
			uint32_t arraySize, Format format, uint8_t numMips, uint8_t sampleCount,
			ResourceFlags resourceFlags, ResourceState state, const float *pClearColor,
			bool isCubeMap, const wchar_t *name);
		Descriptor allocateRtvPool();

		std::vector<DescriptorPool>	m_rtvPools;
		std::vector<std::vector<Descriptor>> m_rtvs;
	};

	//--------------------------------------------------------------------------------------
	// Depth stencil
	//--------------------------------------------------------------------------------------
	class DepthStencil :
		public Texture2D
	{
	public:
		DepthStencil();
		virtual ~DepthStencil();

		bool Create(const Device &device, uint32_t width, uint32_t height,
			Format format = Format(0), ResourceFlags resourceFlags = ResourceFlags(0),
			uint32_t arraySize = 1, uint8_t numMips = 1, uint8_t sampleCount = 1,
			ResourceState state = ResourceState(0), float clearDepth = 1.0f,
			uint8_t clearStencil = 0, bool isCubeMap = false, const wchar_t *name = nullptr);
		bool CreateArray(const Device &device, uint32_t width, uint32_t height, uint32_t arraySize,
			Format format = Format(0), ResourceFlags resourceFlags = ResourceFlags(0),
			uint8_t numMips = 1, uint8_t sampleCount = 1, ResourceState state = ResourceState(0),
			float clearDepth = 1.0f, uint8_t clearStencil = 0, bool isCubeMap = false,
			const wchar_t *name = nullptr);

		Descriptor GetDSV(uint32_t slice = 0, uint8_t mipLevel = 0) const;
		Descriptor GetReadOnlyDSV(uint32_t slice = 0, uint8_t mipLevel = 0) const;
		const Descriptor &GetStencilSRV() const;

		Format		GetDSVFormat() const;
		uint32_t	GetArraySize() const;
		uint8_t		GetNumMips() const;

	protected:
		bool create(const Device &device, uint32_t width, uint32_t height, uint32_t arraySize,
			uint8_t numMips, uint8_t sampleCount, Format &format, ResourceFlags resourceFlags,
			ResourceState state, float clearDepth, uint8_t clearStencil, bool &hasSRV,
			Format &formatStencil, bool isCubeMap, const wchar_t *name);
		Descriptor allocateDsvPool();

		std::vector<DescriptorPool> m_dsvPools;
		std::vector<std::vector<Descriptor>> m_dsvs;
		std::vector<std::vector<Descriptor>> m_readOnlyDsvs;
		Descriptor	m_stencilSrv;
		Format		m_dsvFormat;
	};

	//--------------------------------------------------------------------------------------
	// 3D Texture
	//--------------------------------------------------------------------------------------
	class Texture3D :
		public ResourceBase
	{
	public:
		Texture3D();
		virtual ~Texture3D();

		bool Create(const Device &device, uint32_t width, uint32_t height, uint32_t depth,
			Format format, ResourceFlags resourceFlags = ResourceFlags(0), uint8_t numMips = 1,
			MemoryType memoryType = MemoryType(1), ResourceState state = ResourceState(0),
			const wchar_t *name = nullptr);
		bool CreateSRVs(Format format = Format(0), uint8_t numMips = 1);
		bool CreateSRVLevels(uint8_t numMips, Format format = Format(0));
		bool CreateUAVs(Format format = Format(0), uint8_t numMips = 1);
		
		Descriptor GetUAV(uint8_t i = 0) const;
		Descriptor GetSRVLevel(uint8_t i) const;

	protected:
		std::vector<Descriptor>	m_uavs;
		std::vector<Descriptor>	m_srvLevels;
		Resource m_counter;
	};

	//--------------------------------------------------------------------------------------
	// Raw buffer
	//--------------------------------------------------------------------------------------
	class RawBuffer :
		public ResourceBase
	{
	public:
		RawBuffer();
		virtual ~RawBuffer();

		bool Create(const Device &device, uint32_t byteWidth, ResourceFlags resourceFlags = ResourceFlags(0),
			MemoryType memoryType = MemoryType(1), ResourceState state = ResourceState(0),
			uint32_t numSRVs = 1, const uint32_t *firstSRVElements = nullptr,
			uint32_t numUAVs = 1, const uint32_t *firstUAVElements = nullptr,
			const wchar_t *name = nullptr);
		bool Upload(const CommandList &commandList, Resource &uploader, const void *pData,
			size_t size, ResourceState dstState = ResourceState(0), uint32_t i = 0);
		bool CreateSRVs(uint32_t byteWidth, const uint32_t *firstElements = nullptr,
			uint32_t numDescriptors = 1);
		bool CreateUAVs(uint32_t byteWidth, const uint32_t *firstElements = nullptr,
			uint32_t numDescriptors = 1);

		Descriptor GetUAV(uint32_t i = 0) const;
		
		void *Map(uint32_t i = 0);
		void Unmap();

	protected:
		bool create(const Device &device, uint32_t byteWidth, ResourceFlags resourceFlags,
			MemoryType memoryType, ResourceState state, uint32_t numSRVs, uint32_t numUAVs,
			const wchar_t *name);

		std::vector<Descriptor>	m_uavs;
		std::vector<uint32_t>	m_srvOffsets;
		Resource m_counter;

		void *m_pDataBegin;
	};

	//--------------------------------------------------------------------------------------
	// Structured buffer
	//--------------------------------------------------------------------------------------
	class StructuredBuffer :
		public RawBuffer
	{
	public:
		StructuredBuffer();
		virtual ~StructuredBuffer();

		bool Create(const Device &device, uint32_t numElements, uint32_t stride,
			ResourceFlags resourceFlags = ResourceFlags(0), MemoryType memoryType = MemoryType(1),
			ResourceState state = ResourceState(0),
			uint32_t numSRVs = 1, const uint32_t *firstSRVElements = nullptr,
			uint32_t numUAVs = 1, const uint32_t *firstUAVElements = nullptr,
			const wchar_t *name = nullptr);
		
		bool CreateSRVs(uint32_t numElements, uint32_t stride,
			const uint32_t *firstElements = nullptr, uint32_t numDescriptors = 1);
		bool CreateUAVs(uint32_t numElements, uint32_t stride,
			const uint32_t *firstElements = nullptr, uint32_t numDescriptors = 1);
	};

	//--------------------------------------------------------------------------------------
	// Typed buffer
	//--------------------------------------------------------------------------------------
	class TypedBuffer :
		public RawBuffer
	{
	public:
		TypedBuffer();
		virtual ~TypedBuffer();

		bool Create(const Device &device, uint32_t numElements, uint32_t stride, Format format,
			ResourceFlags resourceFlags = ResourceFlags(0), MemoryType memoryType = MemoryType(1),
			ResourceState state = ResourceState(0),
			uint32_t numSRVs = 1, const uint32_t *firstSRVElements = nullptr,
			uint32_t numUAVs = 1, const uint32_t *firstUAVElements = nullptr,
			const wchar_t *name = nullptr);
		
		bool CreateSRVs(uint32_t numElements, Format format, uint32_t stride,
			const uint32_t *firstElements = nullptr, uint32_t numDescriptors = 1);
		bool CreateUAVs(uint32_t numElements, Format format, uint32_t stride,
			const uint32_t *firstElements = nullptr, uint32_t numDescriptors = 1);
	};

	//--------------------------------------------------------------------------------------
	// Vertex buffer
	//--------------------------------------------------------------------------------------
	class VertexBuffer :
		public StructuredBuffer
	{
	public:
		VertexBuffer();
		virtual ~VertexBuffer();

		bool Create(const Device &device, uint32_t numVertices, uint32_t stride,
			ResourceFlags resourceFlags = ResourceFlags(0), MemoryType memoryType = MemoryType(1),
			ResourceState state = ResourceState(0),
			uint32_t numVBVs = 1, const uint32_t *firstVertices = nullptr,
			uint32_t numSRVs = 1, const uint32_t *firstSRVElements = nullptr,
			uint32_t numUAVs = 1, const uint32_t *firstUAVElements = nullptr,
			const wchar_t *name = nullptr);
		bool CreateAsRaw(const Device &device, uint32_t numVertices, uint32_t stride,
			ResourceFlags resourceFlags = ResourceFlags(0), MemoryType memoryType = MemoryType(1),
			ResourceState state = ResourceState(0),
			uint32_t numVBVs = 1, const uint32_t *firstVertices = nullptr,
			uint32_t numSRVs = 1, const uint32_t *firstSRVElements = nullptr,
			uint32_t numUAVs = 1, const uint32_t *firstUAVElements = nullptr,
			const wchar_t *name = nullptr);

		VertexBufferView GetVBV(uint32_t i = 0) const;

	protected:
		std::vector<VertexBufferView> m_vbvs;
	};

	//--------------------------------------------------------------------------------------
	// Index buffer
	//--------------------------------------------------------------------------------------
	class IndexBuffer :
		public TypedBuffer
	{
	public:
		IndexBuffer();
		virtual ~IndexBuffer();

		bool Create(const Device &device, uint32_t byteWidth, Format format = Format(42),
			ResourceFlags resourceFlags = ResourceFlags(0x8), MemoryType memoryType = MemoryType(1),
			ResourceState state = ResourceState(0),
			uint32_t numIBVs = 1, const uint32_t *offsets = nullptr,
			uint32_t numSRVs = 1, const uint32_t *firstSRVElements = nullptr,
			uint32_t numUAVs = 1, const uint32_t *firstUAVElements = nullptr,
			const wchar_t *name = nullptr);

		IndexBufferView GetIBV(uint32_t i = 0) const;

	protected:
		std::vector<IndexBufferView> m_ibvs;
	};
}
