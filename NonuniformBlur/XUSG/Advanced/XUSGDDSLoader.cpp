//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#include "DXFrameworkHelper.h"
#include "XUSGDDSLoader.h"
#include "dds.h"

using namespace std;
using namespace DirectX;
using namespace XUSG;
using namespace XUSG::DDS;

struct handle_closer
{
	void operator()(HANDLE h) { if (h) CloseHandle(h); }
};
typedef public unique_ptr<void, handle_closer> ScopedHandle;
inline HANDLE safe_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? 0 : h; }

static bool LoadTextureDataFromFile(const wchar_t *fileName,
	unique_ptr<uint8_t[]> &ddsData, DDS_HEADER **header,
	uint8_t **bitData, size_t *bitSize)
{
	F_RETURN(!header || !bitData || !bitSize, cerr, E_POINTER, false);

	// Open the file
	ifstream fileStream(fileName, ios::in | ios::binary);
	F_RETURN(!fileStream, cerr, GetLastError(), false);

	// Get the file size
	fileStream.seekg(0, fileStream.end);
	const auto fileSize = static_cast<uint32_t>(fileStream.tellg());
	if (!fileStream.seekg(0))
	{
		fileStream.close();

		return false;
	}

	// Need at least enough data to fill the header and magic number to be a valid DDS
	C_RETURN(fileSize < (sizeof(DDS_HEADER) + sizeof(uint32_t)), false);

	// create enough space for the file data
	ddsData.reset(new uint8_t[fileSize]);
	F_RETURN(!ddsData, cerr, E_OUTOFMEMORY, false);

	// read the data in
	F_RETURN(!fileStream.read(reinterpret_cast<char*>(ddsData.get()), fileSize),
		cerr, GetLastError(), false);

	// DDS files always start with the same magic number ("DDS ")
	const auto dwMagicNumber = *(const uint32_t*)(ddsData.get());
	C_RETURN(dwMagicNumber != DDS_MAGIC, false);

	const auto hdr = reinterpret_cast<DDS_HEADER*>(ddsData.get() + sizeof(uint32_t));

	// Verify header to validate DDS file
	C_RETURN(hdr->size != sizeof(DDS_HEADER) || hdr->ddspf.size != sizeof(DDS_PIXELFORMAT), false);

	auto offset = sizeof(uint32_t) + sizeof(DDS_HEADER);

	// Check for extensions
	if (hdr->ddspf.flags & DDS_FOURCC)
		if (MAKEFOURCC('D', 'X', '1', '0') == hdr->ddspf.fourCC)
			offset += sizeof(DDS_HEADER_DXT10);

	// Must be long enough for all headers and magic value
	C_RETURN(fileSize < offset, false);

	// setup the pointers in the process request
	*header = hdr;
	*bitData = ddsData.get() + offset;
	*bitSize = fileSize - offset;

	return true;
}

//--------------------------------------------------------------------------------------
// Get surface information for a particular format
//--------------------------------------------------------------------------------------
static void GetSurfaceInfo(uint32_t width, uint32_t height, Format fmt,
	size_t* outNumBytes, size_t* outRowBytes, size_t* outNumRows)
{
	size_t numBytes = 0;
	size_t rowBytes = 0;
	size_t numRows = 0;

	auto bc = false;
	auto packed = false;
	auto planar = false;
	size_t bpe = 0;
	switch (fmt)
	{
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		bc = true;
		bpe = 8;
		break;
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		bc = true;
		bpe = 16;
		break;
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_YUY2:
		packed = true;
		bpe = 4;
		break;
	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		packed = true;
		bpe = 8;
		break;
	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
		planar = true;
		bpe = 2;
		break;
	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		planar = true;
		bpe = 4;
		break;
	}

	if (bc)
	{
		size_t numBlocksWide = 0;
		if (width > 0) numBlocksWide = max<size_t>(1, (width + 3) / 4);
		size_t numBlocksHigh = 0;
		if (height > 0) numBlocksHigh = max<size_t>(1, (height + 3) / 4);
		rowBytes = numBlocksWide * bpe;
		numRows = numBlocksHigh;
		numBytes = rowBytes * numBlocksHigh;
	}
	else if (packed)
	{
		rowBytes = ((width + 1) >> 1) * bpe;
		numRows = height;
		numBytes = rowBytes * height;
	}
	else if (fmt == DXGI_FORMAT_NV11)
	{
		rowBytes = ((width + 3) >> 2) * 4;
		numRows = height * 2; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
		numBytes = rowBytes * numRows;
	}
	else if (planar)
	{
		rowBytes = ((width + 1) >> 1) * bpe;
		numBytes = (rowBytes * height) + ((rowBytes * height + 1) >> 1);
		numRows = height + ((height + 1) >> 1);
	}
	else
	{
		size_t bpp = Loader::BitsPerPixel(fmt);
		rowBytes = (width * bpp + 7) / 8; // round up to nearest byte
		numRows = height;
		numBytes = rowBytes * height;
	}

	if (outNumBytes) *outNumBytes = numBytes;
	if (outRowBytes) *outRowBytes = rowBytes;
	if (outNumRows) *outNumRows = numRows;
}

#define ISBITMASK(r,g,b,a) (ddpf.RBitMask == r && ddpf.GBitMask == g && ddpf.BBitMask == b && ddpf.ABitMask == a)

static Format GetDXGIFormat(const DDS_PIXELFORMAT& ddpf)
{
	if (ddpf.flags & DDS_RGB)
	{
		// Note that sRGB formats are written using the "DX10" extended header
		switch (ddpf.RGBBitCount)
		{
		case 32:
			if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
				return DXGI_FORMAT_B8G8R8A8_UNORM;
			if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000))
				return DXGI_FORMAT_B8G8R8X8_UNORM;
			// No DXGI format maps to ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0x00000000) aka D3DFMT_X8B8G8R8

			// Note that many common DDS reader/writers (including D3DX) swap the
			// the RED/BLUE masks for 10:10:10:2 formats. We assumme
			// below that the 'backwards' header mask is being used since it is most
			// likely written by D3DX. The more robust solution is to use the 'DX10'
			// header extension and specify the DXGI_FORMAT_R10G10B10A2_UNORM format directly

			// For 'correct' writers, this should be 0x000003ff,0x000ffc00,0x3ff00000 for RGB data
			if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
				return DXGI_FORMAT_R10G10B10A2_UNORM;
			// No DXGI format maps to ISBITMASK(0x000003ff,0x000ffc00,0x3ff00000,0xc0000000) aka D3DFMT_A2R10G10B10
			if (ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000))
				return DXGI_FORMAT_R16G16_UNORM;
			if (ISBITMASK(0xffffffff, 0x00000000, 0x00000000, 0x00000000))
			{
				// Only 32-bit color channel format in D3D9 was R32F
				return DXGI_FORMAT_R32_FLOAT; // D3DX writes this out as a FourCC of 114
			}
			break;

		case 24:
			// No 24bpp DXGI formats aka D3DFMT_R8G8B8
			break;

		case 16:
			if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
				return DXGI_FORMAT_B5G5R5A1_UNORM;
			if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0x0000))
				return DXGI_FORMAT_B5G6R5_UNORM;
			// No DXGI format maps to ISBITMASK(0x7c00,0x03e0,0x001f,0x0000) aka D3DFMT_X1R5G5B5
			if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
				return DXGI_FORMAT_B4G4R4A4_UNORM;
			// No DXGI format maps to ISBITMASK(0x0f00,0x00f0,0x000f,0x0000) aka D3DFMT_X4R4G4B4
			// No 3:3:2, 3:3:2:8, or paletted DXGI formats aka D3DFMT_A8R3G3B2, D3DFMT_R3G3B2, D3DFMT_P8, D3DFMT_A8P8, etc.
			break;
		}
	}
	else if (ddpf.flags & DDS_LUMINANCE)
	{
		if (8 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x00000000))
				return DXGI_FORMAT_R8_UNORM; // D3DX10/11 writes this out as DX10 extension
			// No DXGI format maps to ISBITMASK(0x0f,0x00,0x00,0xf0) aka D3DFMT_A4L4
		}

		if (16 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0x0000ffff, 0x00000000, 0x00000000, 0x00000000))
				return DXGI_FORMAT_R16_UNORM; // D3DX10/11 writes this out as DX10 extension
			if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x0000ff00))
				return DXGI_FORMAT_R8G8_UNORM; // D3DX10/11 writes this out as DX10 extension
		}
	}
	else if (ddpf.flags & DDS_ALPHA)
	{
		if (8 == ddpf.RGBBitCount)
			return DXGI_FORMAT_A8_UNORM;
	}
	else if (ddpf.flags & DDS_FOURCC)
	{
		if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.fourCC)
			return DXGI_FORMAT_BC1_UNORM;
		if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.fourCC)
			return DXGI_FORMAT_BC2_UNORM;
		if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.fourCC)
			return DXGI_FORMAT_BC3_UNORM;

		// While pre-mulitplied alpha isn't directly supported by the DXGI formats,
		// they are basically the same as these BC formats so they can be mapped
		if (MAKEFOURCC('D', 'X', 'T', '2') == ddpf.fourCC)
			return DXGI_FORMAT_BC2_UNORM;
		if (MAKEFOURCC('D', 'X', 'T', '4') == ddpf.fourCC)
			return DXGI_FORMAT_BC3_UNORM;

		if (MAKEFOURCC('A', 'T', 'I', '1') == ddpf.fourCC)
			return DXGI_FORMAT_BC4_UNORM;
		if (MAKEFOURCC('B', 'C', '4', 'U') == ddpf.fourCC)
			return DXGI_FORMAT_BC4_UNORM;
		if (MAKEFOURCC('B', 'C', '4', 'S') == ddpf.fourCC)
			return DXGI_FORMAT_BC4_SNORM;

		if (MAKEFOURCC('A', 'T', 'I', '2') == ddpf.fourCC)
			return DXGI_FORMAT_BC5_UNORM;
		if (MAKEFOURCC('B', 'C', '5', 'U') == ddpf.fourCC)
			return DXGI_FORMAT_BC5_UNORM;
		if (MAKEFOURCC('B', 'C', '5', 'S') == ddpf.fourCC)
			return DXGI_FORMAT_BC5_SNORM;

		// BC6H and BC7 are written using the "DX10" extended header

		if (MAKEFOURCC('R', 'G', 'B', 'G') == ddpf.fourCC)
			return DXGI_FORMAT_R8G8_B8G8_UNORM;
		if (MAKEFOURCC('G', 'R', 'G', 'B') == ddpf.fourCC)
			return DXGI_FORMAT_G8R8_G8B8_UNORM;
		if (MAKEFOURCC('Y', 'U', 'Y', '2') == ddpf.fourCC)
			return DXGI_FORMAT_YUY2;

		// Check for D3DFORMAT enums being set here
		switch (ddpf.fourCC)
		{
		case 36: // D3DFMT_A16B16G16R16
			return DXGI_FORMAT_R16G16B16A16_UNORM;
		case 110: // D3DFMT_Q16W16V16U16
			return DXGI_FORMAT_R16G16B16A16_SNORM;
		case 111: // D3DFMT_R16F
			return DXGI_FORMAT_R16_FLOAT;
		case 112: // D3DFMT_G16R16F
			return DXGI_FORMAT_R16G16_FLOAT;
		case 113: // D3DFMT_A16B16G16R16F
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case 114: // D3DFMT_R32F
			return DXGI_FORMAT_R32_FLOAT;
		case 115: // D3DFMT_G32R32F
			return DXGI_FORMAT_R32G32_FLOAT;
		case 116: // D3DFMT_A32B32G32R32F
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
	}

	return DXGI_FORMAT_UNKNOWN;
}

static Format MakeSRGB(Format format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM_SRGB;
	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM_SRGB;
	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM_SRGB;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
	case DXGI_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
	default:
		return format;
	}
}

static bool FillInitData(uint32_t width, uint32_t height, uint32_t depth,
	uint32_t mipCount, uint32_t arraySize, Format format,
	size_t maxsize, size_t bitSize, const uint8_t *bitData,
	uint32_t &twidth, uint32_t &theight, uint32_t &tdepth, uint8_t &skipMip,
	SubresourceData *initData)
{
	F_RETURN(!bitData || !initData, cerr, E_POINTER, false);

	skipMip = 0;
	twidth = 0;
	theight = 0;
	tdepth = 0;

	size_t NumBytes = 0;
	size_t RowBytes = 0;
	auto pSrcBits = bitData;
	const auto pEndBits = bitData + bitSize;

	size_t index = 0;
	for (auto j = 0u; j < arraySize; j++)
	{
		auto w = width;
		auto h = height;
		auto d = depth;
		for (auto i = 0u; i < mipCount; i++)
		{
			GetSurfaceInfo(w, h, format, &NumBytes, &RowBytes, nullptr);

			if (mipCount <= 1 || !maxsize || (w <= maxsize && h <= maxsize && d <= maxsize))
			{
				if (!twidth)
				{
					twidth = w;
					theight = h;
					tdepth = d;
				}

				assert(index < mipCount * arraySize);
				_Analysis_assume_(index < mipCount * arraySize);
				initData[index].pData = reinterpret_cast<const void*>(pSrcBits);
				initData[index].RowPitch = static_cast<uint32_t>(RowBytes);
				initData[index].SlicePitch = static_cast<uint32_t>(NumBytes);
				++index;
			}
			else if (!j) ++skipMip;	// Count number of skipped mipmaps (first item only)

			pSrcBits += NumBytes * d;
			F_RETURN(pSrcBits > pEndBits, cerr, ERROR_HANDLE_EOF, false);
			
			w = (max)(w >> 1, 1u);
			h = (max)(h >> 1, 1u);
			d = (max)(d >> 1, 1u);
		}
	}

	return index > 0;
}

static bool CreateTexture(const Device &device, const CommandList &commandList,
	const DDS_HEADER* header, const uint8_t *bitData, size_t bitSize, size_t maxsize,
	bool forceSRGB, shared_ptr<ResourceBase> &texture, Resource &uploader,
	const wchar_t *name)
{
	const auto width = header->width;
	auto height = header->height;
	auto depth = header->depth;

	uint32_t resDim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
	auto arraySize = 1u;
	auto format = DXGI_FORMAT_UNKNOWN;
	bool isCubeMap = false;

	const auto mipCount = static_cast<uint8_t>((max)(header->mipMapCount, 1u));

	if ((header->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
	{
		const auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>
			(reinterpret_cast<const char*>(header) + sizeof(DDS_HEADER));

		arraySize = d3d10ext->arraySize;
		F_RETURN(arraySize == 0, cerr, ERROR_INVALID_DATA, false);

		switch (d3d10ext->dxgiFormat)
		{
		case DXGI_FORMAT_AI44:
		case DXGI_FORMAT_IA44:
		case DXGI_FORMAT_P8:
		case DXGI_FORMAT_A8P8:
			V_RETURN(ERROR_NOT_SUPPORTED, cerr, false);
		default:
			F_RETURN(Loader::BitsPerPixel(d3d10ext->dxgiFormat) == 0, cerr, ERROR_NOT_SUPPORTED, false);
		}

		format = d3d10ext->dxgiFormat;

		switch (d3d10ext->resourceDimension)
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			// D3DX writes 1D textures with a fixed Height of 1
			F_RETURN((header->flags & DDS_HEIGHT) && height != 1, cerr, ERROR_INVALID_DATA, false);
			height = depth = 1;
			break;

		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			if (d3d10ext->miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE)
			{
				arraySize *= 6;
				isCubeMap = true;
			}
			depth = 1;
			break;

		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			F_RETURN(!(header->flags & DDS_HEADER_FLAGS_VOLUME), cerr, ERROR_INVALID_DATA, false);
			F_RETURN(arraySize > 1, cerr, ERROR_INVALID_DATA, false);
			break;

		default:
			V_RETURN(ERROR_NOT_SUPPORTED, cerr, false);
		}

		resDim = d3d10ext->resourceDimension;
	}
	else
	{
		format = GetDXGIFormat(header->ddspf);
		F_RETURN(format == DXGI_FORMAT_UNKNOWN, cerr, ERROR_NOT_SUPPORTED, false);

		if (header->flags & DDS_HEADER_FLAGS_VOLUME)
			resDim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		else
		{
			if (header->caps2 & DDS_CUBEMAP)
			{
				// We require all six faces to be defined
				F_RETURN((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES,
					cerr, ERROR_NOT_SUPPORTED, false);

				arraySize = 6;
				isCubeMap = true;
			}

			depth = 1;
			resDim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			// Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture
		}

		assert(Loader::BitsPerPixel(format) != 0);
	}

	// Bound sizes (for security purposes we don't trust DDS file metadata larger than the D3D 11.x hardware requirements)
	F_RETURN(mipCount > D3D12_REQ_MIP_LEVELS, cerr, ERROR_NOT_SUPPORTED, false);

	switch (resDim)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		F_RETURN(arraySize > D3D12_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION ||
			width > D3D12_REQ_TEXTURE1D_U_DIMENSION, cerr, ERROR_NOT_SUPPORTED, false);
		texture = make_shared<Texture2D>();
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (isCubeMap)
		{
			// This is the right bound because we set arraySize to (NumCubes * 6) above
			F_RETURN(arraySize > D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION ||
				width > D3D12_REQ_TEXTURECUBE_DIMENSION ||
				height > D3D12_REQ_TEXTURECUBE_DIMENSION,
				cerr, ERROR_NOT_SUPPORTED, false);
		}
		else F_RETURN(arraySize > D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION ||
			width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
			height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION,
			cerr, ERROR_NOT_SUPPORTED, false);
		texture = make_shared<Texture2D>();
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		F_RETURN(arraySize > 1 ||
			width > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION ||
			height > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION ||
			depth > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION,
			cerr, ERROR_NOT_SUPPORTED, false);
		texture = make_shared<Texture3D>();
		break;

	default:
		V_RETURN(ERROR_NOT_SUPPORTED, cerr, false);
	}

	{
		// Create the texture
		const auto subresourceCount = static_cast<uint32_t>(mipCount) * arraySize;
		unique_ptr<SubresourceData[]> initData(new SubresourceData[subresourceCount]);
		F_RETURN(!initData, cerr, E_OUTOFMEMORY, false);

		auto skipMip = 0ui8;
		auto twidth = 0u;
		auto theight = 0u;
		auto tdepth = 0u;

		if (FillInitData(width, height, depth, mipCount, arraySize, format, maxsize, bitSize, bitData,
			twidth, theight, tdepth, skipMip, initData.get()))
		{
			bool success;
			const auto texture2D = dynamic_pointer_cast<Texture2D, ResourceBase>(texture);
			const auto texture3D = dynamic_pointer_cast<Texture3D, ResourceBase>(texture);
			if (texture2D)
			{
				const auto fmt = forceSRGB ? MakeSRGB(format) : format;
				success = texture2D->Create(device, twidth, theight, fmt, arraySize, ResourceFlags(0),
					mipCount - skipMip, 1, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, isCubeMap, name);
				if (success) success = texture2D->Upload(commandList, uploader, initData.get(), subresourceCount,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			else if (texture3D)
			{
				const auto fmt = forceSRGB ? MakeSRGB(format) : format;
				success = texture3D->Create(device, twidth, theight, tdepth, fmt, ResourceFlags(0),
					mipCount - skipMip, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, name);
			}
			else V_RETURN(ERROR_NOT_SUPPORTED, cerr, false);

			if (!success && !maxsize && (mipCount > 1))
			{
				// Retry with a maxsize determined by feature level
				maxsize = (resDim == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
					? 2048 /*D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
					: 8192 /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;

				if (FillInitData(width, height, depth, mipCount, arraySize, format, maxsize, bitSize, bitData,
					twidth, theight, tdepth, skipMip, initData.get()))
				{
					if (texture2D)
					{
						const auto fmt = forceSRGB ? MakeSRGB(format) : format;
						texture = make_shared<Texture2D>();
						success = texture2D->Create(device, width, height, fmt, arraySize, ResourceFlags(0), mipCount,
							1, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, isCubeMap, name);
						if (success) success = texture2D->Upload(commandList, uploader, initData.get(), subresourceCount,
							D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					}
					else if (texture3D)
					{
						const auto fmt = forceSRGB ? MakeSRGB(format) : format;
						texture = make_shared<Texture3D>();
						success = texture3D->Create(device, width, height, depth, fmt, ResourceFlags(0),
							mipCount, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, name);
					}
					else V_RETURN(ERROR_NOT_SUPPORTED, cerr, false);
				}
			}

			M_RETURN(!success, cerr, "Failed to the create texture.", false);
		}
	}

	return true;
}

static AlphaMode GetAlphaMode(const DDS_HEADER *header)
{
	if (header->ddspf.flags & DDS_FOURCC)
	{
		if (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC)
		{
			auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>
				(reinterpret_cast<const char*>(header) + sizeof(DDS_HEADER));
			auto mode = static_cast<AlphaMode>(d3d10ext->miscFlags2 & DDS_MISC_FLAGS2_ALPHA_MODE_MASK);
			switch (mode)
			{
			case ALPHA_MODE_STRAIGHT:
			case ALPHA_MODE_PREMULTIPLIED:
			case ALPHA_MODE_OPAQUE:
			case ALPHA_MODE_CUSTOM:
				return mode;
			}
		}
		else if ((MAKEFOURCC('D', 'X', 'T', '2') == header->ddspf.fourCC) ||
			(MAKEFOURCC('D', 'X', 'T', '4') == header->ddspf.fourCC))
			return ALPHA_MODE_PREMULTIPLIED;
	}

	return ALPHA_MODE_UNKNOWN;
}

//--------------------------------------------------------------------------------------

Loader::Loader()
{
}

Loader::~Loader()
{
}

bool Loader::CreateTextureFromMemory(const Device &device, const CommandList &commandList,
	const uint8_t *ddsData, size_t ddsDataSize, size_t maxsize, bool forceSRGB,
	std::shared_ptr<ResourceBase>& texture, Resource &uploader, AlphaMode *alphaMode)
{
	if (alphaMode) *alphaMode = ALPHA_MODE_UNKNOWN;
	F_RETURN(!device || !ddsData, cerr, E_INVALIDARG, false);

	// Validate DDS file in memory
	C_RETURN(ddsDataSize < sizeof(uint32_t) + sizeof(DDS_HEADER), false);
	
	const auto magicNumber = reinterpret_cast<const uint32_t&>(*ddsData);
	C_RETURN(magicNumber != DDS_MAGIC, false);

	// Verify header to validate DDS file
	auto header = reinterpret_cast<const DDS_HEADER*>(ddsData + sizeof(uint32_t));
	C_RETURN(header->size != sizeof(DDS_HEADER) ||
		header->ddspf.size != sizeof(DDS_PIXELFORMAT), false);

	auto offset = sizeof(DDS_HEADER) + sizeof(uint32_t);

	// Check for extensions
	if (header->ddspf.flags & DDS_FOURCC)
		if (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC)
			offset += sizeof(DDS_HEADER_DXT10);

	// Must be long enough for all headers and magic value
	C_RETURN(ddsDataSize < offset, false);

	N_RETURN(CreateTexture(device, commandList, header, ddsData + offset, ddsDataSize - offset,
		maxsize, forceSRGB, texture, uploader, L"DDSTextureLoader"), false);

	if (alphaMode) *alphaMode = GetAlphaMode(header);

	return true;
}

bool Loader::CreateTextureFromFile(const Device &device, const CommandList &commandList,
	const wchar_t *fileName, size_t maxsize, bool forceSRGB, shared_ptr<ResourceBase> &texture,
	Resource &uploader, AlphaMode *alphaMode)
{
	if (alphaMode) *alphaMode = ALPHA_MODE_UNKNOWN;
	F_RETURN(!device || !fileName, cerr, E_INVALIDARG, false);

	DDS_HEADER *header = nullptr;
	uint8_t *bitData = nullptr;
	size_t bitSize = 0;

	unique_ptr<uint8_t[]> ddsData;
	N_RETURN(LoadTextureDataFromFile(fileName, ddsData, &header, &bitData, &bitSize), false);

	N_RETURN(CreateTexture(device, commandList, header, bitData, bitSize,
		maxsize, forceSRGB, texture, uploader, fileName), false);

	if (alphaMode) *alphaMode = GetAlphaMode(header);

	return true;
}

size_t Loader::BitsPerPixel(DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 128;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 96;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_Y416:
	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		return 64;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_AYUV:
	case DXGI_FORMAT_Y410:
	case DXGI_FORMAT_YUY2:
		return 32;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		return 24;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_A8P8:
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		return 16;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
	case DXGI_FORMAT_NV11:
		return 12;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_AI44:
	case DXGI_FORMAT_IA44:
	case DXGI_FORMAT_P8:
		return 8;

	case DXGI_FORMAT_R1_UNORM:
		return 1;

	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 4;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return 8;

	default:
		return 0;
	}
}
