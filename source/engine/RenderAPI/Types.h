#pragma once

#ifdef RBE_VK
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#endif


namespace render
{
	struct Color
	{
		float r, g, b, a;

		static inline constexpr Color White()			{ return { 1.f, 1.f, 1.f, 1.f }; }
		static inline constexpr Color Black()			{ return { 0.f, 0.f, 0.f, 0.f }; }
		static inline constexpr Color Red()				{ return { 1.f, 0.f, 0.f, 1.f }; }
		static inline constexpr Color Green()			{ return { 0.f, 1.f, 0.f, 1.f }; }
		static inline constexpr Color Blue()			{ return { 0.f, 0.f, 1.f, 1.f }; }
		static inline constexpr Color Orange()			{ return { 1.f, 0.5f, 0.f, 1.f }; }
		static inline constexpr Color Yellow()			{ return { 1.f, 1.f, 0.f, 1.f }; }
		static inline constexpr Color SpringGreen()		{ return { 0.5f, 1.f, 0.f, 1.f }; }
		static inline constexpr Color Turquoise()		{ return { 0.f, 1.f, 0.5f, 1.f }; }
		static inline constexpr Color Cyan()			{ return { 0.f, 1.f, 1.f, 1.f }; }
		static inline constexpr Color Ocean()			{ return { 0.f, 0.5f, 1.f, 1.f }; }
		static inline constexpr Color Violet()			{ return { 0.5f, 0.f, 1.f, 1.f }; }
		static inline constexpr Color Magenta()			{ return { 1.f, 0.f, 1.f, 1.f }; }
		static inline constexpr Color Raspberry()		{ return { 1.f, 0.f, 0.5f, 1.f }; }
	};

	struct Extent2D
	{
		uint32_t width;
		uint32_t height;

		inline bool operator==(const Extent2D& other) const
		{
			return width == other.width &&
				height == other.height;
		}

		inline bool operator!=(const Extent2D& other) const
		{
			return !(*this == other);
		}
	};

	struct Extent3D
	{
		uint32_t width;
		uint32_t height;
		uint32_t depth;

		inline bool operator==(const Extent3D& other) const
		{
			return width == other.width &&
				height == other.height &&
				depth == other.depth;
		}

		inline bool operator!=(const Extent3D& other) const
		{
			return !(*this == other);
		}
	};

	struct Offset2D
	{
		int32_t x;
		int32_t y;

		inline bool operator==(const Offset2D& other) const
		{
			return x == other.x &&
				y == other.y;
		}

		inline bool operator!=(const Offset2D& other) const
		{
			return !(*this == other);
		}
	};

	struct Offset3D
	{
		int32_t x;
		int32_t y;
		int32_t z;

		inline bool operator==(const Offset3D& other) const
		{
			return x == other.x &&
				y == other.y &&
				z == other.z;
		}

		inline bool operator!=(const Offset3D& other) const
		{
			return !(*this == other);
		}
	};

	struct Rect2D
	{
		Offset2D offset;
		Extent2D extent;

		inline bool operator==(const Rect2D& other) const
		{
			return offset == other.offset &&
				extent == other.extent;
		}

		inline bool operator!=(const Rect2D& other) const
		{
			return !(*this == other);
		}
	};

	struct Rect
	{
		float minX;
		float maxX;
		float minY;
		float maxY;

		Rect()
			: minX(0.f), maxX(1.f), minY(0.f), maxY(1.f)
		{
		}

		Rect(float minX, float maxX, float minY, float maxY)
			: minX(minX), maxX(maxX), minY(minY), maxY(maxY)
		{
		}

		inline bool operator==(const Rect& other) const
		{
			return minX == other.minX &&
				maxX == other.maxX &&
				minY == other.minY &&
				maxY == other.maxY;
		}

		inline bool operator!=(const Rect& other) const
		{
			return !(*this == other);
		}

		inline float GetWidth() const
		{
			return maxX - minX;
		}

		inline float GetHeight() const
		{
			return maxY - minY;
		}
	};

	struct Viewport
	{
		float x;
		float y;
		float width;
		float height;
		float minDepth;
		float maxDepth;

		Viewport()
			: x(0.f), width(0.f), y(0.f), height(0.f), minDepth(0.f), maxDepth(0.f)
		{
		}

		Viewport(float _x, float _y, float _width, float _height, float _minDepth, float _maxDepth)
			: x(_x), width(_width), y(_y), height(_height), minDepth(_minDepth), maxDepth(_maxDepth)
		{
		}

		inline bool operator==(const Viewport& other) const
		{
			return x == other.x &&
				y == other.y &&
				width == other.width &&
				height == other.height &&
				minDepth == other.minDepth &&
				maxDepth == other.maxDepth;
		}

		inline bool operator!=(const Viewport& other) const
		{
			return !(*this == other);
		}
	};

	enum PresentMode
	{
		PresentMode_Immediate,
		PresentMode_Mailbox,
		PresentMode_FIFO,
		PresentMode_FIFORelaxed,
		PresentMode_SharedDemandRefresh,
		PresentMode_SharedContinuousRefresh,
		PresentMode_MaxEnum,
	};

	enum ColorSpace
	{
		ColorSpace_SRGB,
		ColorSpace_MaxEnum,
	};

	enum QueueTypeFlags
	{
		Queue_None = 0,
		Queue_Compute = 1,
		Queue_Graphics = 2,
		Queue_Transfer = 4,
		Queue_All = 0xff
	};
	typedef uint8_t QueueType;

	enum BufferUsageFlags
	{
		BufferUsage_None = 0x0000,
		BufferUsage_TransferSrc = 0x0001,
		BufferUsage_TransferDst = 0x0002,
		BufferUsage_UniformTexelBuffer = 0x0004,
		BufferUsage_StorageTexelBuffer = 0x0008,
		BufferUsage_UniformBuffer = 0x0010,
		BufferUsage_StorageBuffer = 0x0020,
		BufferUsage_IndexBuffer = 0x0040,
		BufferUsage_VertexBuffer = 0x0080,
		BufferUsage_IndirectBuffer = 0x0100,
		BufferUsage_ShaderDeviceAddress = 0x0200,
	};
	typedef uint32_t BufferUsage;

	enum ImageUsageFlags
	{
		ImageUsage_TransferSrc = 0x00000001,
		ImageUsage_TransferDst = 0x00000002,
		ImageUsage_Sampled = 0x00000004,
		ImageUsage_Storage = 0x00000008,
		ImageUsage_ColorAttachment = 0x00000010,
		ImageUsage_DepthStencilAttachment = 0x00000020,
		ImageUsage_TransientAttachment = 0x00000040,
		ImageUsage_InputAttachment = 0x00000080,
		ImageUsage_VideoDecodeDst = 0x00000400,
		ImageUsage_VideoDecodeSrc = 0x00000800,
		ImageUsage_VideoDecodeDpb = 0x00001000,
		ImageUsage_FragmentDensityMap = 0x00000200,
		ImageUsage_FragmentShadingRateAttachment = 0x00000100,
		ImageUsage_HostTransfer = 0x00400000,
		ImageUsage_VideoEncodeDst = 0x00002000,
		ImageUsage_VideoEncodeSrc = 0x00004000,
		ImageUsage_VideoEncodeDpb = 0x00008000,
		ImageUsage_AttachmentFeedbackLoop = 0x00080000,
		ImageUsage_MaxEnum = 0x7FFFFFFF
	};
	typedef uint32_t ImageUsage;

	enum MemoryUsage
	{
		MemoryUsage_Unknown,
		MemoryUsage_Cpu,
		MemoryUsage_CpuCopy,
		MemoryUsage_CpuToGpu,
		MemoryUsage_Gpu,
		MemoryUsage_GpuToCpu,
	};

	enum ResourceType
	{
		ResourceType_None,
		ResourceType_TextureSRV,
		ResourceType_TextureUAV,
		ResourceType_ConstantBuffer,
		ResourceType_VolatileConstantBuffer,
		ResourceType_BufferUAV,
		ResourceType_DynamicBufferUAV,
		ResourceType_MaxEnum
	};

	enum Format
	{
		Format_Undefined = 0,
		Format_R4G4_UNorm_Pack8 = 1,
		Format_R4G4B4A4_UNorm_Pack16 = 2,
		Format_B4G4R4A4_UNorm_Pack16 = 3,
		Format_R5G6B5_UNorm_Pack16 = 4,
		Format_B5G6R5_UNorm_Pack16 = 5,
		Format_R5G5B5A1_UNorm_Pack16 = 6,
		Format_B5G5R5A1_UNorm_Pack16 = 7,
		Format_A1R5G5B5_UNorm_Pack16 = 8,
		Format_R8_UNorm = 9,
		Format_R8_SNorm = 10,
		Format_R8_UScaled = 11,
		Format_R8_SScaled = 12,
		Format_R8_UInt = 13,
		Format_R8_SInt = 14,
		Format_R8_SRGB = 15,
		Format_R8G8_UNorm = 16,
		Format_R8G8_SNorm = 17,
		Format_R8G8_UScaled = 18,
		Format_R8G8_SScaled = 19,
		Format_R8G8_UInt = 20,
		Format_R8G8_SInt = 21,
		Format_R8G8_SRGB = 22,
		Format_R8G8B8_UNorm = 23,
		Format_R8G8B8_SNorm = 24,
		Format_R8G8B8_UScaled = 25,
		Format_R8G8B8_SScaled = 26,
		Format_R8G8B8_UInt = 27,
		Format_R8G8B8_SInt = 28,
		Format_R8G8B8_SRGB = 29,
		Format_B8G8R8_UNorm = 30,
		Format_B8G8R8_SNorm = 31,
		Format_B8G8R8_UScaled = 32,
		Format_B8G8R8_SScaled = 33,
		Format_B8G8R8_UInt = 34,
		Format_B8G8R8_SInt = 35,
		Format_B8G8R8_SRGB = 36,
		Format_R8G8B8A8_UNorm = 37,
		Format_R8G8B8A8_SNorm = 38,
		Format_R8G8B8A8_UScaled = 39,
		Format_R8G8B8A8_SScaled = 40,
		Format_R8G8B8A8_UInt = 41,
		Format_R8G8B8A8_SInt = 42,
		Format_R8G8B8A8_SRGB = 43,
		Format_B8G8R8A8_UNorm = 44,
		Format_B8G8R8A8_SNorm = 45,
		Format_B8G8R8A8_UScaled = 46,
		Format_B8G8R8A8_SScaled = 47,
		Format_B8G8R8A8_UInt = 48,
		Format_B8G8R8A8_SInt = 49,
		Format_B8G8R8A8_SRGB = 50,
		Format_A8B8G8R8_UNorm_Pack32 = 51,
		Format_A8B8G8R8_SNorm_Pack32 = 52,
		Format_A8B8G8R8_UScaled_Pack32 = 53,
		Format_A8B8G8R8_SScaled_Pack32 = 54,
		Format_A8B8G8R8_UInt_Pack32 = 55,
		Format_A8B8G8R8_SInt_Pack32 = 56,
		Format_A8B8G8R8_SRGB_Pack32 = 57,
		Format_A2R10G10B10_UNorm_Pack32 = 58,
		Format_A2R10G10B10_SNorm_Pack32 = 59,
		Format_A2R10G10B10_UScaled_Pack32 = 60,
		Format_A2R10G10B10_SScaled_Pack32 = 61,
		Format_A2R10G10B10_UInt_Pack32 = 62,
		Format_A2R10G10B10_SInt_Pack32 = 63,
		Format_A2B10G10R10_UNorm_Pack32 = 64,
		Format_A2B10G10R10_SNorm_Pack32 = 65,
		Format_A2B10G10R10_UScaled_Pack32 = 66,
		Format_A2B10G10R10_SScaled_Pack32 = 67,
		Format_A2B10G10R10_UInt_Pack32 = 68,
		Format_A2B10G10R10_SInt_Pack32 = 69,
		Format_R16_UNorm = 70,
		Format_R16_SNorm = 71,
		Format_R16_UScaled = 72,
		Format_R16_SScaled = 73,
		Format_R16_UInt = 74,
		Format_R16_SInt = 75,
		Format_R16_SFloat = 76,
		Format_R16G16_UNorm = 77,
		Format_R16G16_SNorm = 78,
		Format_R16G16_UScaled = 79,
		Format_R16G16_SScaled = 80,
		Format_R16G16_UInt = 81,
		Format_R16G16_SInt = 82,
		Format_R16G16_SFloat = 83,
		Format_R16G16B16_UNorm = 84,
		Format_R16G16B16_SNorm = 85,
		Format_R16G16B16_UScaled = 86,
		Format_R16G16B16_SScaled = 87,
		Format_R16G16B16_UInt = 88,
		Format_R16G16B16_SInt = 89,
		Format_R16G16B16_SFloat = 90,
		Format_R16G16B16A16_UNorm = 91,
		Format_R16G16B16A16_SNorm = 92,
		Format_R16G16B16A16_UScaled = 93,
		Format_R16G16B16A16_SScaled = 94,
		Format_R16G16B16A16_UInt = 95,
		Format_R16G16B16A16_SInt = 96,
		Format_R16G16B16A16_SFloat = 97,
		Format_R32_UInt = 98,
		Format_R32_SInt = 99,
		Format_R32_SFloat = 100,
		Format_R32G32_UInt = 101,
		Format_R32G32_SInt = 102,
		Format_R32G32_SFloat = 103,
		Format_R32G32B32_UInt = 104,
		Format_R32G32B32_SInt = 105,
		Format_R32G32B32_SFloat = 106,
		Format_R32G32B32A32_UInt = 107,
		Format_R32G32B32A32_SInt = 108,
		Format_R32G32B32A32_SFloat = 109,
		Format_R64_UInt = 110,
		Format_R64_SInt = 111,
		Format_R64_SFloat = 112,
		Format_R64G64_UInt = 113,
		Format_R64G64_SInt = 114,
		Format_R64G64_SFloat = 115,
		Format_R64G64B64_UInt = 116,
		Format_R64G64B64_SInt = 117,
		Format_R64G64B64_SFloat = 118,
		Format_R64G64B64A64_UInt = 119,
		Format_R64G64B64A64_SInt = 120,
		Format_R64G64B64A64_SFloat = 121,
		Format_B10G11R11_UFLOAT_Pack32 = 122,
		Format_E5B9G9R9_UFLOAT_Pack32 = 123,
		Format_D16_UNorm = 124,
		Format_X8_D24_UNorm_Pack32 = 125,
		Format_D32_SFloat = 126,
		Format_S8_UInt = 127,
		Format_D16_UNorm_S8_UInt = 128,
		Format_D24_UNorm_S8_UInt = 129,
		Format_D32_SFloat_S8_UInt = 130,
		Format_BC1_RGB_UNorm_Block = 131,
		Format_BC1_RGB_SRGB_Block = 132,
		Format_BC1_RGBA_UNorm_Block = 133,
		Format_BC1_RGBA_SRGB_Block = 134,
		Format_BC2_UNorm_Block = 135,
		Format_BC2_SRGB_Block = 136,
		Format_BC3_UNorm_Block = 137,
		Format_BC3_SRGB_Block = 138,
		Format_BC4_UNorm_Block = 139,
		Format_BC4_SNorm_Block = 140,
		Format_BC5_UNorm_Block = 141,
		Format_BC5_SNorm_Block = 142,
		Format_BC6H_UFLOAT_Block = 143,
		Format_BC6H_SFloat_Block = 144,
		Format_BC7_UNorm_Block = 145,
		Format_BC7_SRGB_Block = 146,
		Format_ETC2_R8G8B8_UNorm_Block = 147,
		Format_ETC2_R8G8B8_SRGB_Block = 148,
		Format_ETC2_R8G8B8A1_UNorm_Block = 149,
		Format_ETC2_R8G8B8A1_SRGB_Block = 150,
		Format_ETC2_R8G8B8A8_UNorm_Block = 151,
		Format_ETC2_R8G8B8A8_SRGB_Block = 152,
		Format_EAC_R11_UNorm_Block = 153,
		Format_EAC_R11_SNorm_Block = 154,
		Format_EAC_R11G11_UNorm_Block = 155,
		Format_EAC_R11G11_SNorm_Block = 156,
		Format_ASTC_4x4_UNorm_Block = 157,
		Format_ASTC_4x4_SRGB_Block = 158,
		Format_ASTC_5x4_UNorm_Block = 159,
		Format_ASTC_5x4_SRGB_Block = 160,
		Format_ASTC_5x5_UNorm_Block = 161,
		Format_ASTC_5x5_SRGB_Block = 162,
		Format_ASTC_6x5_UNorm_Block = 163,
		Format_ASTC_6x5_SRGB_Block = 164,
		Format_ASTC_6x6_UNorm_Block = 165,
		Format_ASTC_6x6_SRGB_Block = 166,
		Format_ASTC_8x5_UNorm_Block = 167,
		Format_ASTC_8x5_SRGB_Block = 168,
		Format_ASTC_8x6_UNorm_Block = 169,
		Format_ASTC_8x6_SRGB_Block = 170,
		Format_ASTC_8x8_UNorm_Block = 171,
		Format_ASTC_8x8_SRGB_Block = 172,
		Format_ASTC_10x5_UNorm_Block = 173,
		Format_ASTC_10x5_SRGB_Block = 174,
		Format_ASTC_10x6_UNorm_Block = 175,
		Format_ASTC_10x6_SRGB_Block = 176,
		Format_ASTC_10x8_UNorm_Block = 177,
		Format_ASTC_10x8_SRGB_Block = 178,
		Format_ASTC_10x10_UNorm_Block = 179,
		Format_ASTC_10x10_SRGB_Block = 180,
		Format_ASTC_12x10_UNorm_Block = 181,
		Format_ASTC_12x10_SRGB_Block = 182,
		Format_ASTC_12x12_UNorm_Block = 183,
		Format_ASTC_12x12_SRGB_Block = 184,
		Format_G8B8G8R8_422_UNorm = 1000156000,
		Format_B8G8R8G8_422_UNorm = 1000156001,
		Format_G8_B8_R8_3Plane_420_UNorm = 1000156002,
		Format_G8_B8R8_2Plane_420_UNorm = 1000156003,
		Format_G8_B8_R8_3Plane_422_UNorm = 1000156004,
		Format_G8_B8R8_2Plane_422_UNorm = 1000156005,
		Format_G8_B8_R8_3Plane_444_UNorm = 1000156006,
		Format_R10X6_UNorm_Pack16 = 1000156007,
		Format_R10X6G10X6_UNorm_2Pack16 = 1000156008,
		Format_R10X6G10X6B10X6A10X6_UNorm_4Pack16 = 1000156009,
		Format_G10X6B10X6G10X6R10X6_422_UNorm_4Pack16 = 1000156010,
		Format_B10X6G10X6R10X6G10X6_422_UNorm_4Pack16 = 1000156011,
		Format_G10X6_B10X6_R10X6_3Plane_420_UNorm_3Pack16 = 1000156012,
		Format_G10X6_B10X6R10X6_2Plane_420_UNorm_3Pack16 = 1000156013,
		Format_G10X6_B10X6_R10X6_3Plane_422_UNorm_3Pack16 = 1000156014,
		Format_G10X6_B10X6R10X6_2Plane_422_UNorm_3Pack16 = 1000156015,
		Format_G10X6_B10X6_R10X6_3Plane_444_UNorm_3Pack16 = 1000156016,
		Format_R12X4_UNorm_Pack16 = 1000156017,
		Format_R12X4G12X4_UNorm_2Pack16 = 1000156018,
		Format_R12X4G12X4B12X4A12X4_UNorm_4Pack16 = 1000156019,
		Format_G12X4B12X4G12X4R12X4_422_UNorm_4Pack16 = 1000156020,
		Format_B12X4G12X4R12X4G12X4_422_UNorm_4Pack16 = 1000156021,
		Format_G12X4_B12X4_R12X4_3Plane_420_UNorm_3Pack16 = 1000156022,
		Format_G12X4_B12X4R12X4_2Plane_420_UNorm_3Pack16 = 1000156023,
		Format_G12X4_B12X4_R12X4_3Plane_422_UNorm_3Pack16 = 1000156024,
		Format_G12X4_B12X4R12X4_2Plane_422_UNorm_3Pack16 = 1000156025,
		Format_G12X4_B12X4_R12X4_3Plane_444_UNorm_3Pack16 = 1000156026,
		Format_G16B16G16R16_422_UNorm = 1000156027,
		Format_B16G16R16G16_422_UNorm = 1000156028,
		Format_G16_B16_R16_3Plane_420_UNorm = 1000156029,
		Format_G16_B16R16_2Plane_420_UNorm = 1000156030,
		Format_G16_B16_R16_3Plane_422_UNorm = 1000156031,
		Format_G16_B16R16_2Plane_422_UNorm = 1000156032,
		Format_G16_B16_R16_3Plane_444_UNorm = 1000156033,
		Format_G8_B8R8_2Plane_444_UNorm = 1000330000,
		Format_G10X6_B10X6R10X6_2Plane_444_UNorm_3Pack16 = 1000330001,
		Format_G12X4_B12X4R12X4_2Plane_444_UNorm_3Pack16 = 1000330002,
		Format_G16_B16R16_2Plane_444_UNorm = 1000330003,
		Format_A4R4G4B4_UNorm_Pack16 = 1000340000,
		Format_A4B4G4R4_UNorm_Pack16 = 1000340001,
		Format_ASTC_4x4_SFloat_Block = 1000066000,
		Format_ASTC_5x4_SFloat_Block = 1000066001,
		Format_ASTC_5x5_SFloat_Block = 1000066002,
		Format_ASTC_6x5_SFloat_Block = 1000066003,
		Format_ASTC_6x6_SFloat_Block = 1000066004,
		Format_ASTC_8x5_SFloat_Block = 1000066005,
		Format_ASTC_8x6_SFloat_Block = 1000066006,
		Format_ASTC_8x8_SFloat_Block = 1000066007,
		Format_ASTC_10x5_SFloat_Block = 1000066008,
		Format_ASTC_10x6_SFloat_Block = 1000066009,
		Format_ASTC_10x8_SFloat_Block = 1000066010,
		Format_ASTC_10x10_SFloat_Block = 1000066011,
		Format_ASTC_12x10_SFloat_Block = 1000066012,
		Format_ASTC_12x12_SFloat_Block = 1000066013,
		Format_PVRTC1_2BPP_UNorm_Block_Img = 1000054000,
		Format_PVRTC1_4BPP_UNorm_Block_Img = 1000054001,
		Format_PVRTC2_2BPP_UNorm_Block_Img = 1000054002,
		Format_PVRTC2_4BPP_UNorm_Block_Img = 1000054003,
		Format_PVRTC1_2BPP_SRGB_Block_Img = 1000054004,
		Format_PVRTC1_4BPP_SRGB_Block_Img = 1000054005,
		Format_PVRTC2_2BPP_SRGB_Block_Img = 1000054006,
		Format_PVRTC2_4BPP_SRGB_Block_Img = 1000054007,
		Format_R16G16_SFixed5_NV = 1000464000,
		Format_A1B5G5R5_UNorm_Pack16_KHR = 1000470000,
		Format_A8_UNorm_KHR = 1000470001,
		Format_MaxEnum = 0x7FFFFFFF
	};

	enum ImageLayout
	{
		ImageLayout_Undefined,
		ImageLayout_General,
		ImageLayout_ColorAttachment,
		ImageLayout_DepthStencilAttachment,
		ImageLayout_DepthStencilReadOnly,
		ImageLayout_ShaderReadOnly,
		ImageLayout_TransferSrc,
		ImageLayout_TransferDst,
		ImageLayout_Preinitialized,
		ImageLayout_DepthReadOnly_StencilAttachment,
		ImageLayout_DepthAttachment_StencilReadOnly,
		ImageLayout_DepthAttachment,
		ImageLayout_DepthReadOnly,
		ImageLayout_StencilAttachment,
		ImageLayout_StencilReadOnly,
		ImageLayout_Attachment,
		ImageLayout_ReadOnly,
		ImageLayout_PresentSrc,
		ImageLayout_VideoDecodeDst,
		ImageLayout_VideoDecodeSrc,
		ImageLayout_VideoDecodeDpb,
		ImageLayout_SharedPresent,
		ImageLayout_FragmentDensityMap,
		ImageLayout_FragmentShadingRateAttachment,
		ImageLayout_RenderingLocalRead,
		ImageLayout_VideoEncodeDst,
		ImageLayout_VideoEncodeSrc,
		ImageLayout_VideoEncodeDpb,
		ImageLayout_AttachmentFeedbackLoop,
		ImageLayout_MaxEnum,
	};

	enum ImageDimension
	{
		ImageDimension_Undefined,
		ImageDimension_1D,
		ImageDimension_2D,
		ImageDimension_3D,
		ImageDimension_Cube,
		ImageDimension_1DArray,
		ImageDimension_2DArray,
		ImageDimension_CubeArray,
		ImageDimension_MaxEnum
	};

	enum Filter
	{
		Filter_Nearest,
		Filter_Linear,
		Filter_Cubic,
		Filter_MaxEnum
	};

	enum SamplerAddressMode
	{
		SamplerAddressMode_Repeat,
		SamplerAddressMode_ClampToEdge,
		SamplerAddressMode_ClampToBorder,
		SamplerAddressMode_MirrorRepeat,
		SamplerAddressMode_MirrorClampToEdge,
		SamplerAddressMode_MaxEnum,
	};

	enum ShaderTypeFlags
	{
		ShaderType_None = 0x0000,
		ShaderType_Vertex = 0x0001,
		ShaderType_TesselationControl = 0x0002,
		ShaderType_TesselationEvaluation = 0x0004,
		ShaderType_Geometry = 0x0008,
		ShaderType_Fragment = 0x0010,
		ShaderType_Compute = 0x0020,
		ShaderType_RayGen = 0x0040,
		ShaderType_RayAnyHit = 0x0080,
		ShaderType_RayClosestHit = 0x0100,
		ShaderType_RayMiss = 0x0200,
		ShaderType_RayIntersection = 0x0400,
		ShaderType_RayCallable = 0x0800,
		ShaderType_MaxEnum = 0xffff
	};
	typedef uint32_t ShaderType;

	enum BlendFactor
	{
		BlendFactor_Zero,
		BlendFactor_One,
		BlendFactor_SrcColor,
		BlendFactor_OneMinusSrcColor,
		BlendFactor_DstColor,
		BlendFactor_OneMinusDstColor,
		BlendFactor_SrcAlpha,
		BlendFactor_OneMinusSrcAlpha,
		BlendFactor_DstAlpha,
		BlendFactor_OneMinusDstAlpha,
		BlendFactor_ConstantColor,
		BlendFactor_OneMinusConstantColor,
		BlendFactor_ConstantAlpha,
		BlendFactor_OneMinusConstantAlpha,
		BlendFactor_SrcAlphaSaturate,
		BlendFactor_MaxEnum
	};

	enum BlendOp
	{
		BlendOp_Add,
		BlendOp_Subtract,
		BlendOp_ReverseSubtract,
		BlendOp_Min,
		BlendOp_Max,
		BlendOp_MaxEnum
	};

	enum ColorMaskFlags
	{
		ColorMask_Red = 0x1,
		ColorMask_Green = 0x2,
		ColorMask_Blue = 0x4,
		ColorMask_Alpha = 0x8,
		ColorMask_All = ColorMask_Red | ColorMask_Green | ColorMask_Blue | ColorMask_Alpha
	};
	typedef uint32_t ColorMask;

	enum CompareOp
	{
		CompareOp_Never,
		CompareOp_Less,
		CompareOp_Equal,
		CompareOp_LessOrEqual,
		CompareOp_Greater,
		CompareOp_NotEqual,
		CompareOp_GreaterOrEqual,
		CompareOp_Always,
		CompareOp_MaxEnum
	};

	enum StencilOp
	{
		StencilOp_Keep,
		StencilOp_Zero,
		StencilOp_Replace,
		StencilOp_IncrementAndClamp,
		StencilOp_DecrementAndClamp,
		StencilOp_Invert,
		StencilOp_IncrementAndWrap,
		StencilOp_DecrementAndWrap,
		StencilOp_MaxEnum
	};

	enum PrimitiveType
	{
		PrimitiveType_TriangleList,
		PrimitiveType_TriangleStrip,
		PrimitiveType_LineList,
		PrimitiveType_LineStrip,
		PrimitiveType_PointList,
		PrimitiveType_TriangleFan,
		PrimitiveType_PatchList,
		PrimitiveType_MaxEnum
	};

	enum RasterFillMode
	{
		RasterFillMode_Fill,
		RasterFillMode_Line,
		RasterFillMode_Point,
		RasterFillMode_MaxEnum
	};

	enum RasterCullMode
	{
		RasterCullMode_None,
		RasterCullMode_Front,
		RasterCullMode_Back,
		RasterCullMode_FrontAndBack,
		RasterCullMode_MaxEnum
	};
}