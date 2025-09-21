#pragma once

#ifdef RBE_VK
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include "Utils.h"
#endif
#include "Core/Debug.h"
#include "Device.h"
#include "Core/Logger.h"


namespace render
{
    namespace utils
    {
        void HandleVkError(VkResult res, const char* call, const char* file, int line)
        {
			const char* vkstr = ConvertVkResultToStr(res);
			logferror("VkCheck failed. VkResult %d: %s\n", res, vkstr);
			logferror("Call: %s (%s line %d)\n", call, file, line);
            check(false && "Failed vulkan result");
        }

        ImageLayoutState ConvertImageLayoutState(ImageLayout layout)
        {
            ImageLayoutState state = {};
            switch (layout)
            {
            case ImageLayout_Undefined:
                state.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                state.accessFlags = 0;
                state.stageFlags = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                break;
            case ImageLayout_General:
                state.layout = VK_IMAGE_LAYOUT_GENERAL;
                state.accessFlags = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
                state.stageFlags = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                break;
            case ImageLayout_ColorAttachment:
                state.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                state.accessFlags = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                state.stageFlags = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;
            case ImageLayout_DepthStencilAttachment:
            case ImageLayout_DepthAttachment:
                state.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                state.accessFlags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                state.stageFlags = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                break;
            case ImageLayout_DepthStencilReadOnly:
                state.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                state.accessFlags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                state.stageFlags = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                break;
            case ImageLayout_ShaderReadOnly:
                state.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                state.accessFlags = VK_ACCESS_2_SHADER_READ_BIT;
                state.stageFlags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                break;
            case ImageLayout_TransferSrc:
                state.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                state.accessFlags = VK_ACCESS_2_TRANSFER_READ_BIT;
                state.stageFlags = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                break;
            case ImageLayout_TransferDst:
                state.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                state.accessFlags = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                state.stageFlags = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                break;
            case ImageLayout_PresentSrc:
                state.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                state.accessFlags = 0;
                state.stageFlags = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                break;
            default:
                unreachable_code();
            }
            return state;
        }

        VkImageMemoryBarrier2 ConvertImageBarrier(const TextureBarrier& barrier)
        {
            ImageLayoutState oldState = ConvertImageLayoutState(barrier.texture->m_layouts.at(barrier.subresources));
            ImageLayoutState newState = ConvertImageLayoutState(barrier.newLayout);
            VkImageMemoryBarrier2 imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, nullptr };
            imageBarrier.image = barrier.texture->m_image;
            imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.srcAccessMask = oldState.accessFlags;
            imageBarrier.dstAccessMask = newState.accessFlags;
            imageBarrier.oldLayout = oldState.layout;
            imageBarrier.newLayout = newState.layout;
            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | oldState.stageFlags;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | newState.stageFlags;
            imageBarrier.subresourceRange = ConvertImageSubresourceRange(barrier.subresources, barrier.texture->m_description.format);
            return imageBarrier;
        }

        const char* ConvertVkResultToStr(VkResult res)
        {
            switch (res)
            {
				case VK_SUCCESS: return "VK_SUCCESS";
                case VK_NOT_READY: return "VK_NOT_READY";
                case VK_TIMEOUT: return "VK_TIMEOUT";
                case VK_EVENT_SET: return "VK_EVENT_SET";
                case VK_EVENT_RESET: return "VK_EVENT_RESET";
                case VK_INCOMPLETE: return "VK_INCOMPLETE";
                case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
                case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
                case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
                case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
                case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
                case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
                case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
                case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
                case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
                case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
                case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
                case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
                case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
                case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
                case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
                case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
                case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
                case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
                case VK_ERROR_NOT_PERMITTED: return "VK_ERROR_NOT_PERMITTED";
                case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
                case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
                case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
                case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
                case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
                case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
                case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
                case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
                case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
                case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
                case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
                case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
                case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
                case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
                case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
                case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
                case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
                case VK_PIPELINE_BINARY_MISSING_KHR: return "VK_PIPELINE_BINARY_MISSING_KHR";
                case VK_ERROR_NOT_ENOUGH_SPACE_KHR: return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
                case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM";
            }
            unreachable_code();
            return nullptr;
        }

        VkPresentModeKHR ConvertPresentMode(PresentMode mode)
        {
            switch (mode)
            {
            case PresentMode_Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
            case PresentMode_Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR;
            case PresentMode_FIFO: return VK_PRESENT_MODE_FIFO_KHR;
            case PresentMode_FIFORelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            case PresentMode_SharedDemandRefresh: return VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR;
            case PresentMode_SharedContinuousRefresh: return VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
            case PresentMode_MaxEnum: return VK_PRESENT_MODE_MAX_ENUM_KHR;
            }
            unreachable_code();
            return VK_PRESENT_MODE_MAX_ENUM_KHR;
        }

        VkColorSpaceKHR ConvertColorSpace(ColorSpace space)
        {
            switch (space)
            {
            case ColorSpace_SRGB: return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            case ColorSpace_MaxEnum: return VK_COLOR_SPACE_MAX_ENUM_KHR;
            }
            unreachable_code();
            return VK_COLOR_SPACE_MAX_ENUM_KHR;
        }

        VkQueueFlags ConvertQueueFlags(QueueType flags)
        {
            VkQueueFlags f = 0;
            if (flags & Queue_Compute) f |= VK_QUEUE_COMPUTE_BIT;
            if (flags & Queue_Graphics) f |= VK_QUEUE_GRAPHICS_BIT;
            if (flags & Queue_Transfer) f |= VK_QUEUE_TRANSFER_BIT;
            return f;
        }

        VkBufferUsageFlags ConvertBufferUsage(BufferUsage usage)
        {
            VkBufferUsageFlags flags = 0;
            if (usage & BufferUsage_TransferSrc) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            if (usage & BufferUsage_TransferDst) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            if (usage & BufferUsage_UniformTexelBuffer) flags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
            if (usage & BufferUsage_StorageTexelBuffer) flags |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
            if (usage & BufferUsage_UniformBuffer) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            if (usage & BufferUsage_StorageBuffer) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            if (usage & BufferUsage_IndexBuffer) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            if (usage & BufferUsage_VertexBuffer) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            if (usage & BufferUsage_IndirectBuffer) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            if (usage & BufferUsage_ShaderDeviceAddress) flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            return flags;
        }

        VkImageUsageFlags ConvertImageUsage(ImageUsage usage)
        {
            VkImageUsageFlags flags = 0;
            if (usage & ImageUsage_TransferSrc) flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            if (usage & ImageUsage_TransferDst) flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            if (usage & ImageUsage_Sampled) flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
            if (usage & ImageUsage_Storage) flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            if (usage & ImageUsage_ColorAttachment) flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (usage & ImageUsage_DepthStencilAttachment) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            if (usage & ImageUsage_TransientAttachment) flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
            if (usage & ImageUsage_InputAttachment) flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            if (usage & ImageUsage_VideoDecodeDst) flags |= VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
            if (usage & ImageUsage_VideoDecodeSrc) flags |= VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
            if (usage & ImageUsage_VideoDecodeDpb) flags |= VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
            if (usage & ImageUsage_FragmentDensityMap) flags |= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
            if (usage & ImageUsage_FragmentShadingRateAttachment) flags |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
            if (usage & ImageUsage_HostTransfer) flags |= VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
            if (usage & ImageUsage_VideoEncodeDst) flags |= VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR;
            if (usage & ImageUsage_VideoEncodeSrc) flags |= VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;
            if (usage & ImageUsage_VideoEncodeDpb) flags |= VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
            if (usage & ImageUsage_AttachmentFeedbackLoop) flags |= VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
            return flags;
        }

        VkDescriptorType ConvertToDescriptorType(ResourceType type)
        {
            switch (type)
            {
            case ResourceType_TextureSRV: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case ResourceType_TextureUAV: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case ResourceType_ConstantBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case ResourceType_VolatileConstantBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case ResourceType_BufferUAV: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case ResourceType_DynamicBufferUAV: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            case ResourceType_MaxEnum:
            case ResourceType_None: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
            }
            unreachable_code();
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }

        VkFormat ConvertFormat(Format format)
        {
            switch (format)
            {
            case Format_R4G4_UNorm_Pack8: return VK_FORMAT_R4G4_UNORM_PACK8;
            case Format_R4G4B4A4_UNorm_Pack16:  return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
            case Format_B4G4R4A4_UNorm_Pack16:  return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
            case Format_R5G6B5_UNorm_Pack16:  return VK_FORMAT_R5G6B5_UNORM_PACK16;
            case Format_B5G6R5_UNorm_Pack16:  return VK_FORMAT_B5G6R5_UNORM_PACK16;
            case Format_R5G5B5A1_UNorm_Pack16:  return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
            case Format_B5G5R5A1_UNorm_Pack16:  return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
            case Format_A1R5G5B5_UNorm_Pack16: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
            case Format_R8_UNorm:  return VK_FORMAT_R8_UNORM;
            case Format_R8_SNorm:  return VK_FORMAT_R8_SNORM;
            case Format_R8_UScaled:  return VK_FORMAT_R8_USCALED;
            case Format_R8_SScaled:  return VK_FORMAT_R8_SSCALED;
            case Format_R8_UInt:  return VK_FORMAT_R8_UINT;
            case Format_R8_SInt:  return VK_FORMAT_R8_SINT;
            case Format_R8_SRGB: return VK_FORMAT_R8_SRGB;
            case Format_R8G8_UNorm:  return VK_FORMAT_R8G8_UNORM;
            case Format_R8G8_SNorm:  return VK_FORMAT_R8G8_SNORM;
            case Format_R8G8_UScaled:  return VK_FORMAT_R8G8_USCALED;
            case Format_R8G8_SScaled:  return VK_FORMAT_R8G8_SSCALED;
            case Format_R8G8_UInt:  return VK_FORMAT_R8G8_UINT;
            case Format_R8G8_SInt:  return VK_FORMAT_R8G8_SINT;
            case Format_R8G8_SRGB: return VK_FORMAT_R8G8_SRGB;
            case Format_R8G8B8_UNorm:  return VK_FORMAT_R8G8B8_UNORM;
            case Format_R8G8B8_SNorm:  return VK_FORMAT_R8G8B8_SNORM;
            case Format_R8G8B8_UScaled:  return VK_FORMAT_R8G8B8_USCALED;
            case Format_R8G8B8_SScaled:  return VK_FORMAT_R8G8B8_SSCALED;
            case Format_R8G8B8_UInt:  return VK_FORMAT_R8G8B8_UINT;
            case Format_R8G8B8_SInt:  return VK_FORMAT_R8G8B8_SINT;
            case Format_R8G8B8_SRGB:  return VK_FORMAT_R8G8B8_SRGB;
            case Format_B8G8R8_UNorm:  return VK_FORMAT_B8G8R8_UNORM;
            case Format_B8G8R8_SNorm:  return VK_FORMAT_B8G8R8_SNORM;
            case Format_B8G8R8_UScaled:  return VK_FORMAT_B8G8R8_USCALED;
            case Format_B8G8R8_SScaled:  return VK_FORMAT_B8G8R8_SSCALED;
            case Format_B8G8R8_UInt:  return VK_FORMAT_B8G8R8_UINT;
            case Format_B8G8R8_SInt:  return VK_FORMAT_B8G8R8_SINT;
            case Format_B8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB;
            case Format_R8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
            case Format_R8G8B8A8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
            case Format_R8G8B8A8_UScaled: return VK_FORMAT_R8G8B8A8_USCALED;
            case Format_R8G8B8A8_SScaled: return VK_FORMAT_R8G8B8A8_SSCALED;
            case Format_R8G8B8A8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
            case Format_R8G8B8A8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
            case Format_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
            case Format_B8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
            case Format_B8G8R8A8_SNorm: return VK_FORMAT_B8G8R8A8_SNORM;
            case Format_B8G8R8A8_UScaled: return VK_FORMAT_B8G8R8A8_USCALED;
            case Format_B8G8R8A8_SScaled: return VK_FORMAT_B8G8R8A8_SSCALED;
            case Format_B8G8R8A8_UInt: return VK_FORMAT_B8G8R8A8_UINT;
            case Format_B8G8R8A8_SInt: return VK_FORMAT_B8G8R8A8_SINT;
            case Format_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
            case Format_A8B8G8R8_UNorm_Pack32: return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
            case Format_A8B8G8R8_SNorm_Pack32: return VK_FORMAT_A8B8G8R8_SNORM_PACK32;
            case Format_A8B8G8R8_UScaled_Pack32: return VK_FORMAT_A8B8G8R8_USCALED_PACK32;
            case Format_A8B8G8R8_SScaled_Pack32: return VK_FORMAT_A8B8G8R8_SSCALED_PACK32;
            case Format_A8B8G8R8_UInt_Pack32: return VK_FORMAT_A8B8G8R8_UINT_PACK32;
            case Format_A8B8G8R8_SInt_Pack32: return VK_FORMAT_A8B8G8R8_SINT_PACK32;
            case Format_A8B8G8R8_SRGB_Pack32: return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
            case Format_A2R10G10B10_UNorm_Pack32: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
            case Format_A2R10G10B10_SNorm_Pack32: return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
            case Format_A2R10G10B10_UScaled_Pack32: return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
            case Format_A2R10G10B10_SScaled_Pack32: return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
            case Format_A2R10G10B10_UInt_Pack32: return VK_FORMAT_A2R10G10B10_UINT_PACK32;
            case Format_A2R10G10B10_SInt_Pack32: return VK_FORMAT_A2R10G10B10_SINT_PACK32;
            case Format_A2B10G10R10_UNorm_Pack32: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            case Format_A2B10G10R10_SNorm_Pack32: return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
            case Format_A2B10G10R10_UScaled_Pack32: return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
            case Format_A2B10G10R10_SScaled_Pack32: return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
            case Format_A2B10G10R10_UInt_Pack32: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
            case Format_A2B10G10R10_SInt_Pack32: return VK_FORMAT_A2B10G10R10_SINT_PACK32;
            case Format_R16_UNorm: return VK_FORMAT_R16_UNORM;
            case Format_R16_SNorm: return VK_FORMAT_R16_SNORM;
            case Format_R16_UScaled: return VK_FORMAT_R16_USCALED;
            case Format_R16_SScaled: return VK_FORMAT_R16_SSCALED;
            case Format_R16_UInt: return VK_FORMAT_R16_UINT;
            case Format_R16_SInt: return VK_FORMAT_R16_SINT;
            case Format_R16_SFloat: return VK_FORMAT_R16_SFLOAT;
            case Format_R16G16_UNorm: return VK_FORMAT_R16G16_UNORM;
            case Format_R16G16_SNorm: return VK_FORMAT_R16G16_SNORM;
            case Format_R16G16_UScaled: return VK_FORMAT_R16G16_USCALED;
            case Format_R16G16_SScaled: return VK_FORMAT_R16G16_SSCALED;
            case Format_R16G16_UInt: return VK_FORMAT_R16G16_UINT;
            case Format_R16G16_SInt: return VK_FORMAT_R16G16_SINT;
            case Format_R16G16_SFloat: return VK_FORMAT_R16G16_SFLOAT;
            case Format_R16G16B16_UNorm: return VK_FORMAT_R16G16B16_UNORM;
            case Format_R16G16B16_SNorm: return VK_FORMAT_R16G16B16_SNORM;
            case Format_R16G16B16_UScaled: return VK_FORMAT_R16G16B16_USCALED;
            case Format_R16G16B16_SScaled: return VK_FORMAT_R16G16B16_SSCALED;
            case Format_R16G16B16_UInt: return VK_FORMAT_R16G16B16_UINT;
            case Format_R16G16B16_SInt: return VK_FORMAT_R16G16B16_SINT;
            case Format_R16G16B16_SFloat: return VK_FORMAT_R16G16B16_SFLOAT;
            case Format_R16G16B16A16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
            case Format_R16G16B16A16_SNorm: return VK_FORMAT_R16G16B16A16_SNORM;
            case Format_R16G16B16A16_UScaled: return VK_FORMAT_R16G16B16A16_USCALED;
            case Format_R16G16B16A16_SScaled: return VK_FORMAT_R16G16B16A16_SSCALED;
            case Format_R16G16B16A16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
            case Format_R16G16B16A16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
            case Format_R16G16B16A16_SFloat: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case Format_R32_UInt: return VK_FORMAT_R32_UINT;
            case Format_R32_SInt: return VK_FORMAT_R32_SINT;
            case Format_R32_SFloat: return VK_FORMAT_R32_SFLOAT;
            case Format_R32G32_UInt: return VK_FORMAT_R32G32_UINT;
            case Format_R32G32_SInt: return VK_FORMAT_R32G32_SINT;
            case Format_R32G32_SFloat: return VK_FORMAT_R32G32_SFLOAT;
            case Format_R32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
            case Format_R32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
            case Format_R32G32B32_SFloat: return VK_FORMAT_R32G32B32_SFLOAT;
            case Format_R32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
            case Format_R32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
            case Format_R32G32B32A32_SFloat: return VK_FORMAT_R32G32B32A32_SFLOAT;
            case Format_R64_UInt:  return VK_FORMAT_R64_UINT;
            case Format_R64_SInt: return VK_FORMAT_R64_SINT;
            case Format_R64_SFloat: return VK_FORMAT_R64_SFLOAT;
            case Format_R64G64_UInt: return VK_FORMAT_R64G64_UINT;
            case Format_R64G64_SInt: return VK_FORMAT_R64G64_SINT;
            case Format_R64G64_SFloat: return VK_FORMAT_R64G64_SFLOAT;
            case Format_R64G64B64_UInt: return VK_FORMAT_R64G64B64_UINT;
            case Format_R64G64B64_SInt: return VK_FORMAT_R64G64B64_SINT;
            case Format_R64G64B64_SFloat: return VK_FORMAT_R64G64B64_SFLOAT;
            case Format_R64G64B64A64_UInt: return VK_FORMAT_R64G64B64A64_UINT;
            case Format_R64G64B64A64_SInt: return VK_FORMAT_R64G64B64A64_SINT;
            case Format_R64G64B64A64_SFloat: return VK_FORMAT_R64G64B64A64_SFLOAT;
            case Format_B10G11R11_UFLOAT_Pack32: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            case Format_E5B9G9R9_UFLOAT_Pack32: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
            case Format_D16_UNorm: return VK_FORMAT_D16_UNORM;
            case Format_X8_D24_UNorm_Pack32: return VK_FORMAT_X8_D24_UNORM_PACK32;
            case Format_D32_SFloat: return VK_FORMAT_D32_SFLOAT;
            case Format_S8_UInt: return VK_FORMAT_S8_UINT;
            case Format_D16_UNorm_S8_UInt: return VK_FORMAT_D16_UNORM_S8_UINT;
            case Format_D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
            case Format_D32_SFloat_S8_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case Format_BC1_RGB_UNorm_Block: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            case Format_BC1_RGB_SRGB_Block: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
            case Format_BC1_RGBA_UNorm_Block: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            case Format_BC1_RGBA_SRGB_Block: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
            case Format_BC2_UNorm_Block: return VK_FORMAT_BC2_UNORM_BLOCK;
            case Format_BC2_SRGB_Block: return VK_FORMAT_BC2_SRGB_BLOCK;
            case Format_BC3_UNorm_Block: return VK_FORMAT_BC3_UNORM_BLOCK;
            case Format_BC3_SRGB_Block: return VK_FORMAT_BC3_SRGB_BLOCK;
            case Format_BC4_UNorm_Block: return VK_FORMAT_BC4_UNORM_BLOCK;
            case Format_BC4_SNorm_Block: return VK_FORMAT_BC4_SNORM_BLOCK;
            case Format_BC5_UNorm_Block: return VK_FORMAT_BC5_UNORM_BLOCK;
            case Format_BC5_SNorm_Block: return VK_FORMAT_BC5_SNORM_BLOCK;
            case Format_BC6H_UFLOAT_Block: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
            case Format_BC6H_SFloat_Block: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
            case Format_BC7_UNorm_Block: return VK_FORMAT_BC7_UNORM_BLOCK;
            case Format_BC7_SRGB_Block: return VK_FORMAT_BC7_SRGB_BLOCK;
            case Format_ETC2_R8G8B8_UNorm_Block: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
            case Format_ETC2_R8G8B8_SRGB_Block: return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
            case Format_ETC2_R8G8B8A1_UNorm_Block: return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
            case Format_ETC2_R8G8B8A1_SRGB_Block: return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
            case Format_ETC2_R8G8B8A8_UNorm_Block: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
            case Format_ETC2_R8G8B8A8_SRGB_Block: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
            case Format_EAC_R11_UNorm_Block: return VK_FORMAT_EAC_R11_UNORM_BLOCK;
            case Format_EAC_R11_SNorm_Block: return VK_FORMAT_EAC_R11_SNORM_BLOCK;
            case Format_EAC_R11G11_UNorm_Block: return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
            case Format_EAC_R11G11_SNorm_Block: return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
            case Format_ASTC_4x4_UNorm_Block: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            case Format_ASTC_4x4_SRGB_Block: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
            case Format_ASTC_5x4_UNorm_Block: return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
            case Format_ASTC_5x4_SRGB_Block: return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
            case Format_ASTC_5x5_UNorm_Block: return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
            case Format_ASTC_5x5_SRGB_Block: return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
            case Format_ASTC_6x5_UNorm_Block: return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
            case Format_ASTC_6x5_SRGB_Block: return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
            case Format_ASTC_6x6_UNorm_Block: return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
            case Format_ASTC_6x6_SRGB_Block: return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
            case Format_ASTC_8x5_UNorm_Block: return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
            case Format_ASTC_8x5_SRGB_Block: return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
            case Format_ASTC_8x6_UNorm_Block: return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
            case Format_ASTC_8x6_SRGB_Block: return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
            case Format_ASTC_8x8_UNorm_Block: return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
            case Format_ASTC_8x8_SRGB_Block: return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
            case Format_ASTC_10x5_UNorm_Block: return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
            case Format_ASTC_10x5_SRGB_Block: return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
            case Format_ASTC_10x6_UNorm_Block: return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
            case Format_ASTC_10x6_SRGB_Block: return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
            case Format_ASTC_10x8_UNorm_Block: return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
            case Format_ASTC_10x8_SRGB_Block: return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
            case Format_ASTC_10x10_UNorm_Block: return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
            case Format_ASTC_10x10_SRGB_Block: return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
            case Format_ASTC_12x10_UNorm_Block: return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
            case Format_ASTC_12x10_SRGB_Block: return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
            case Format_ASTC_12x12_UNorm_Block: return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
            case Format_ASTC_12x12_SRGB_Block: return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
            case Format_G8B8G8R8_422_UNorm: return VK_FORMAT_G8B8G8R8_422_UNORM;
            case Format_B8G8R8G8_422_UNorm: return VK_FORMAT_B8G8R8G8_422_UNORM;
            case Format_G8_B8_R8_3Plane_420_UNorm: return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
            case Format_G8_B8R8_2Plane_420_UNorm: return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
            case Format_G8_B8_R8_3Plane_422_UNorm: return VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM;
            case Format_G8_B8R8_2Plane_422_UNorm: return VK_FORMAT_G8_B8R8_2PLANE_422_UNORM;
            case Format_G8_B8_R8_3Plane_444_UNorm: return VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM;
            case Format_R10X6_UNorm_Pack16: return VK_FORMAT_R10X6_UNORM_PACK16;
            case Format_R10X6G10X6_UNorm_2Pack16: return VK_FORMAT_R10X6G10X6_UNORM_2PACK16;
            case Format_R10X6G10X6B10X6A10X6_UNorm_4Pack16: return VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16;
            case Format_G10X6B10X6G10X6R10X6_422_UNorm_4Pack16: return VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
            case Format_B10X6G10X6R10X6G10X6_422_UNorm_4Pack16: return VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16;
            case Format_G10X6_B10X6_R10X6_3Plane_420_UNorm_3Pack16: return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
            case Format_G10X6_B10X6R10X6_2Plane_420_UNorm_3Pack16: return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
            case Format_G10X6_B10X6_R10X6_3Plane_422_UNorm_3Pack16: return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
            case Format_G10X6_B10X6R10X6_2Plane_422_UNorm_3Pack16: return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
            case Format_G10X6_B10X6_R10X6_3Plane_444_UNorm_3Pack16: return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16;
            case Format_R12X4_UNorm_Pack16: return VK_FORMAT_R12X4_UNORM_PACK16;
            case Format_R12X4G12X4_UNorm_2Pack16: return VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
            case Format_R12X4G12X4B12X4A12X4_UNorm_4Pack16: return VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16;
            case Format_G12X4B12X4G12X4R12X4_422_UNorm_4Pack16: return VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16;
            case Format_B12X4G12X4R12X4G12X4_422_UNorm_4Pack16: return VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16;
            case Format_G12X4_B12X4_R12X4_3Plane_420_UNorm_3Pack16: return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
            case Format_G12X4_B12X4R12X4_2Plane_420_UNorm_3Pack16: return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16;
            case Format_G12X4_B12X4_R12X4_3Plane_422_UNorm_3Pack16: return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
            case Format_G12X4_B12X4R12X4_2Plane_422_UNorm_3Pack16: return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16;
            case Format_G12X4_B12X4_R12X4_3Plane_444_UNorm_3Pack16: return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16;
            case Format_G16B16G16R16_422_UNorm: return VK_FORMAT_G16B16G16R16_422_UNORM;
            case Format_B16G16R16G16_422_UNorm: return VK_FORMAT_B16G16R16G16_422_UNORM;
            case Format_G16_B16_R16_3Plane_420_UNorm: return VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM;
            case Format_G16_B16R16_2Plane_420_UNorm: return VK_FORMAT_G16_B16R16_2PLANE_420_UNORM;
            case Format_G16_B16_R16_3Plane_422_UNorm: return VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM;
            case Format_G16_B16R16_2Plane_422_UNorm: return VK_FORMAT_G16_B16R16_2PLANE_422_UNORM;
            case Format_G16_B16_R16_3Plane_444_UNorm: return VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM;
            case Format_G8_B8R8_2Plane_444_UNorm: return VK_FORMAT_G8_B8R8_2PLANE_444_UNORM;
            case Format_G10X6_B10X6R10X6_2Plane_444_UNorm_3Pack16: return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16;
            case Format_G12X4_B12X4R12X4_2Plane_444_UNorm_3Pack16: return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16;
            case Format_G16_B16R16_2Plane_444_UNorm: return VK_FORMAT_G16_B16R16_2PLANE_444_UNORM;
            case Format_A4R4G4B4_UNorm_Pack16: return VK_FORMAT_A4R4G4B4_UNORM_PACK16;
            case Format_A4B4G4R4_UNorm_Pack16: return VK_FORMAT_A4B4G4R4_UNORM_PACK16;
            case Format_ASTC_4x4_SFloat_Block: return VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK;
            case Format_ASTC_5x4_SFloat_Block: return VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK;
            case Format_ASTC_5x5_SFloat_Block: return VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK;
            case Format_ASTC_6x5_SFloat_Block: return VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK;
            case Format_ASTC_6x6_SFloat_Block: return VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK;
            case Format_ASTC_8x5_SFloat_Block: return VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK;
            case Format_ASTC_8x6_SFloat_Block: return VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK;
            case Format_ASTC_8x8_SFloat_Block: return VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK;
            case Format_ASTC_10x5_SFloat_Block: return VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK;
            case Format_ASTC_10x6_SFloat_Block: return VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK;
            case Format_ASTC_10x8_SFloat_Block: return VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK;
            case Format_ASTC_10x10_SFloat_Block: return VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK;
            case Format_ASTC_12x10_SFloat_Block: return VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK;
            case Format_ASTC_12x12_SFloat_Block: return VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK;
            case Format_PVRTC1_2BPP_UNorm_Block_Img: return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
            case Format_PVRTC1_4BPP_UNorm_Block_Img: return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
            case Format_PVRTC2_2BPP_UNorm_Block_Img: return VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG;
            case Format_PVRTC2_4BPP_UNorm_Block_Img: return VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG;
            case Format_PVRTC1_2BPP_SRGB_Block_Img: return VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG;
            case Format_PVRTC1_4BPP_SRGB_Block_Img: return VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG;
            case Format_PVRTC2_2BPP_SRGB_Block_Img: return VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG;
            case Format_PVRTC2_4BPP_SRGB_Block_Img: return VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG;
            case Format_R16G16_SFixed5_NV: return VK_FORMAT_R16G16_SFIXED5_NV;
            case Format_A1B5G5R5_UNorm_Pack16_KHR: return VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR;
            case Format_A8_UNorm_KHR: return VK_FORMAT_A8_UNORM_KHR;
            }
            unreachable_code();
            return VK_FORMAT_UNDEFINED;
        }

        uint32_t GetBytesPerPixel(Format format)
        {
            switch (format)
            {
            case Format_R4G4_UNorm_Pack8: return 1;
            case Format_R4G4B4A4_UNorm_Pack16:
            case Format_B4G4R4A4_UNorm_Pack16:
            case Format_R5G6B5_UNorm_Pack16:
            case Format_B5G6R5_UNorm_Pack16:
            case Format_R5G5B5A1_UNorm_Pack16:
            case Format_B5G5R5A1_UNorm_Pack16:
            case Format_A1R5G5B5_UNorm_Pack16: return 2;
            case Format_R8_UNorm:
            case Format_R8_SNorm:
            case Format_R8_UScaled:
            case Format_R8_SScaled:
            case Format_R8_UInt:
            case Format_R8_SInt:
            case Format_R8_SRGB: return 1;
            case Format_R8G8_UNorm:
            case Format_R8G8_SNorm:
            case Format_R8G8_UScaled:
            case Format_R8G8_SScaled:
            case Format_R8G8_UInt:
            case Format_R8G8_SInt:
            case Format_R8G8_SRGB: return 2;
            case Format_R8G8B8_UNorm:
            case Format_R8G8B8_SNorm:
            case Format_R8G8B8_UScaled:
            case Format_R8G8B8_SScaled:
            case Format_R8G8B8_UInt:
            case Format_R8G8B8_SInt:
            case Format_R8G8B8_SRGB:
            case Format_B8G8R8_UNorm:
            case Format_B8G8R8_SNorm:
            case Format_B8G8R8_UScaled:
            case Format_B8G8R8_SScaled:
            case Format_B8G8R8_UInt:
            case Format_B8G8R8_SInt:
            case Format_B8G8R8_SRGB: return 3;
            case Format_R8G8B8A8_UNorm:
            case Format_R8G8B8A8_SNorm:
            case Format_R8G8B8A8_UScaled:
            case Format_R8G8B8A8_SScaled:
            case Format_R8G8B8A8_UInt:
            case Format_R8G8B8A8_SInt:
            case Format_R8G8B8A8_SRGB:
            case Format_B8G8R8A8_UNorm:
            case Format_B8G8R8A8_SNorm:
            case Format_B8G8R8A8_UScaled:
            case Format_B8G8R8A8_SScaled:
            case Format_B8G8R8A8_UInt:
            case Format_B8G8R8A8_SInt:
            case Format_B8G8R8A8_SRGB:
            case Format_A8B8G8R8_UNorm_Pack32:
            case Format_A8B8G8R8_SNorm_Pack32:
            case Format_A8B8G8R8_UScaled_Pack32:
            case Format_A8B8G8R8_SScaled_Pack32:
            case Format_A8B8G8R8_UInt_Pack32:
            case Format_A8B8G8R8_SInt_Pack32:
            case Format_A8B8G8R8_SRGB_Pack32:
            case Format_A2R10G10B10_UNorm_Pack32:
            case Format_A2R10G10B10_SNorm_Pack32:
            case Format_A2R10G10B10_UScaled_Pack32:
            case Format_A2R10G10B10_SScaled_Pack32:
            case Format_A2R10G10B10_UInt_Pack32:
            case Format_A2R10G10B10_SInt_Pack32:
            case Format_A2B10G10R10_UNorm_Pack32:
            case Format_A2B10G10R10_SNorm_Pack32:
            case Format_A2B10G10R10_UScaled_Pack32:
            case Format_A2B10G10R10_SScaled_Pack32:
            case Format_A2B10G10R10_UInt_Pack32:
            case Format_A2B10G10R10_SInt_Pack32: return 4;
            case Format_R16_UNorm:
            case Format_R16_SNorm:
            case Format_R16_UScaled:
            case Format_R16_SScaled:
            case Format_R16_UInt:
            case Format_R16_SInt:
            case Format_R16_SFloat: return 2;
            case Format_R16G16_UNorm:
            case Format_R16G16_SNorm:
            case Format_R16G16_UScaled:
            case Format_R16G16_SScaled:
            case Format_R16G16_UInt:
            case Format_R16G16_SInt:
            case Format_R16G16_SFloat: return 2;
            case Format_R16G16B16_UNorm:
            case Format_R16G16B16_SNorm:
            case Format_R16G16B16_UScaled:
            case Format_R16G16B16_SScaled:
            case Format_R16G16B16_UInt:
            case Format_R16G16B16_SInt:
            case Format_R16G16B16_SFloat: return 6;
            case Format_R16G16B16A16_UNorm:
            case Format_R16G16B16A16_SNorm:
            case Format_R16G16B16A16_UScaled:
            case Format_R16G16B16A16_SScaled:
            case Format_R16G16B16A16_UInt:
            case Format_R16G16B16A16_SInt:
            case Format_R16G16B16A16_SFloat: return 8;
            case Format_R32_UInt:
            case Format_R32_SInt:
            case Format_R32_SFloat: return 4;
            case Format_R32G32_UInt:
            case Format_R32G32_SInt:
            case Format_R32G32_SFloat: return 8;
            case Format_R32G32B32_UInt:
            case Format_R32G32B32_SInt:
            case Format_R32G32B32_SFloat: return 12;
            case Format_R32G32B32A32_UInt:
            case Format_R32G32B32A32_SInt:
            case Format_R32G32B32A32_SFloat: return 16;
            case Format_R64_UInt:
            case Format_R64_SInt:
            case Format_R64_SFloat: return 8;
            case Format_R64G64_UInt:
            case Format_R64G64_SInt:
            case Format_R64G64_SFloat: return 16;
            case Format_R64G64B64_UInt:
            case Format_R64G64B64_SInt:
            case Format_R64G64B64_SFloat: return 24;
            case Format_R64G64B64A64_UInt:
            case Format_R64G64B64A64_SInt:
            case Format_R64G64B64A64_SFloat: return 32;
            case Format_B10G11R11_UFLOAT_Pack32: return 4;
            case Format_E5B9G9R9_UFLOAT_Pack32: return 4;
            case Format_D16_UNorm: return 2;
            case Format_X8_D24_UNorm_Pack32: return 4;
            case Format_D32_SFloat: return 4;
            case Format_S8_UInt: return 1;
            case Format_D16_UNorm_S8_UInt: return 3;
            case Format_D24_UNorm_S8_UInt: return 4;
            case Format_D32_SFloat_S8_UInt: return 5;
            case Format_BC1_RGB_UNorm_Block:
            case Format_BC1_RGB_SRGB_Block:
            case Format_BC1_RGBA_UNorm_Block:
            case Format_BC1_RGBA_SRGB_Block:
            case Format_BC2_UNorm_Block:
            case Format_BC2_SRGB_Block:
            case Format_BC3_UNorm_Block:
            case Format_BC3_SRGB_Block:
            case Format_BC4_UNorm_Block:
            case Format_BC4_SNorm_Block:
            case Format_BC5_UNorm_Block:
            case Format_BC5_SNorm_Block:
            case Format_BC6H_UFLOAT_Block:
            case Format_BC6H_SFloat_Block:
            case Format_BC7_UNorm_Block:
            case Format_BC7_SRGB_Block:
            case Format_ETC2_R8G8B8_UNorm_Block:
            case Format_ETC2_R8G8B8_SRGB_Block:
            case Format_ETC2_R8G8B8A1_UNorm_Block:
            case Format_ETC2_R8G8B8A1_SRGB_Block:
            case Format_ETC2_R8G8B8A8_UNorm_Block:
            case Format_ETC2_R8G8B8A8_SRGB_Block:
            case Format_EAC_R11_UNorm_Block:
            case Format_EAC_R11_SNorm_Block:
            case Format_EAC_R11G11_UNorm_Block:
            case Format_EAC_R11G11_SNorm_Block:
            case Format_ASTC_4x4_UNorm_Block:
            case Format_ASTC_4x4_SRGB_Block:
            case Format_ASTC_5x4_UNorm_Block:
            case Format_ASTC_5x4_SRGB_Block:
            case Format_ASTC_5x5_UNorm_Block:
            case Format_ASTC_5x5_SRGB_Block:
            case Format_ASTC_6x5_UNorm_Block:
            case Format_ASTC_6x5_SRGB_Block:
            case Format_ASTC_6x6_UNorm_Block:
            case Format_ASTC_6x6_SRGB_Block:
            case Format_ASTC_8x5_UNorm_Block:
            case Format_ASTC_8x5_SRGB_Block:
            case Format_ASTC_8x6_UNorm_Block:
            case Format_ASTC_8x6_SRGB_Block:
            case Format_ASTC_8x8_UNorm_Block:
            case Format_ASTC_8x8_SRGB_Block:
            case Format_ASTC_10x5_UNorm_Block:
            case Format_ASTC_10x5_SRGB_Block:
            case Format_ASTC_10x6_UNorm_Block:
            case Format_ASTC_10x6_SRGB_Block:
            case Format_ASTC_10x8_UNorm_Block:
            case Format_ASTC_10x8_SRGB_Block:
            case Format_ASTC_10x10_UNorm_Block:
            case Format_ASTC_10x10_SRGB_Block:
            case Format_ASTC_12x10_UNorm_Block:
            case Format_ASTC_12x10_SRGB_Block:
            case Format_ASTC_12x12_UNorm_Block:
            case Format_ASTC_12x12_SRGB_Block:
            case Format_G8B8G8R8_422_UNorm:
            case Format_B8G8R8G8_422_UNorm:
            case Format_G8_B8_R8_3Plane_420_UNorm:
            case Format_G8_B8R8_2Plane_420_UNorm:
            case Format_G8_B8_R8_3Plane_422_UNorm:
            case Format_G8_B8R8_2Plane_422_UNorm:
            case Format_G8_B8_R8_3Plane_444_UNorm:
            case Format_R10X6_UNorm_Pack16:
            case Format_R10X6G10X6_UNorm_2Pack16:
            case Format_R10X6G10X6B10X6A10X6_UNorm_4Pack16:
            case Format_G10X6B10X6G10X6R10X6_422_UNorm_4Pack16:
            case Format_B10X6G10X6R10X6G10X6_422_UNorm_4Pack16:
            case Format_G10X6_B10X6_R10X6_3Plane_420_UNorm_3Pack16:
            case Format_G10X6_B10X6R10X6_2Plane_420_UNorm_3Pack16:
            case Format_G10X6_B10X6_R10X6_3Plane_422_UNorm_3Pack16:
            case Format_G10X6_B10X6R10X6_2Plane_422_UNorm_3Pack16:
            case Format_G10X6_B10X6_R10X6_3Plane_444_UNorm_3Pack16:
            case Format_R12X4_UNorm_Pack16:
            case Format_R12X4G12X4_UNorm_2Pack16:
            case Format_R12X4G12X4B12X4A12X4_UNorm_4Pack16:
            case Format_G12X4B12X4G12X4R12X4_422_UNorm_4Pack16:
            case Format_B12X4G12X4R12X4G12X4_422_UNorm_4Pack16:
            case Format_G12X4_B12X4_R12X4_3Plane_420_UNorm_3Pack16:
            case Format_G12X4_B12X4R12X4_2Plane_420_UNorm_3Pack16:
            case Format_G12X4_B12X4_R12X4_3Plane_422_UNorm_3Pack16:
            case Format_G12X4_B12X4R12X4_2Plane_422_UNorm_3Pack16:
            case Format_G12X4_B12X4_R12X4_3Plane_444_UNorm_3Pack16:
            case Format_G16B16G16R16_422_UNorm:
            case Format_B16G16R16G16_422_UNorm:
            case Format_G16_B16_R16_3Plane_420_UNorm:
            case Format_G16_B16R16_2Plane_420_UNorm:
            case Format_G16_B16_R16_3Plane_422_UNorm:
            case Format_G16_B16R16_2Plane_422_UNorm:
            case Format_G16_B16_R16_3Plane_444_UNorm:
            case Format_G8_B8R8_2Plane_444_UNorm:
            case Format_G10X6_B10X6R10X6_2Plane_444_UNorm_3Pack16:
            case Format_G12X4_B12X4R12X4_2Plane_444_UNorm_3Pack16:
            case Format_G16_B16R16_2Plane_444_UNorm:
            case Format_A4R4G4B4_UNorm_Pack16:
            case Format_A4B4G4R4_UNorm_Pack16:
            case Format_ASTC_4x4_SFloat_Block:
            case Format_ASTC_5x4_SFloat_Block:
            case Format_ASTC_5x5_SFloat_Block:
            case Format_ASTC_6x5_SFloat_Block:
            case Format_ASTC_6x6_SFloat_Block:
            case Format_ASTC_8x5_SFloat_Block:
            case Format_ASTC_8x6_SFloat_Block:
            case Format_ASTC_8x8_SFloat_Block:
            case Format_ASTC_10x5_SFloat_Block:
            case Format_ASTC_10x6_SFloat_Block:
            case Format_ASTC_10x8_SFloat_Block:
            case Format_ASTC_10x10_SFloat_Block:
            case Format_ASTC_12x10_SFloat_Block:
            case Format_ASTC_12x12_SFloat_Block:
            case Format_PVRTC1_2BPP_UNorm_Block_Img:
            case Format_PVRTC1_4BPP_UNorm_Block_Img:
            case Format_PVRTC2_2BPP_UNorm_Block_Img:
            case Format_PVRTC2_4BPP_UNorm_Block_Img:
            case Format_PVRTC1_2BPP_SRGB_Block_Img:
            case Format_PVRTC1_4BPP_SRGB_Block_Img:
            case Format_PVRTC2_2BPP_SRGB_Block_Img:
            case Format_PVRTC2_4BPP_SRGB_Block_Img:
            case Format_R16G16_SFixed5_NV:
            case Format_A1B5G5R5_UNorm_Pack16_KHR:
            case Format_A8_UNorm_KHR: unreachable_code();
            }
            return 0;
        }

		uint32_t GetFormatSize(Format format)
		{
			return GetBytesPerPixel(format);
		}

        VkImageLayout ConvertImageLayout(ImageLayout layout)
        {
            switch (layout)
            {
            case ImageLayout_Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
            case ImageLayout_General: return VK_IMAGE_LAYOUT_GENERAL;
            case ImageLayout_ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ImageLayout_DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case ImageLayout_DepthStencilReadOnly: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            case ImageLayout_ShaderReadOnly: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ImageLayout_TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case ImageLayout_TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case ImageLayout_Preinitialized: return VK_IMAGE_LAYOUT_PREINITIALIZED;
            case ImageLayout_DepthReadOnly_StencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
            case ImageLayout_DepthAttachment_StencilReadOnly: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
            case ImageLayout_DepthAttachment: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case ImageLayout_DepthReadOnly: return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
            case ImageLayout_StencilAttachment: return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            case ImageLayout_StencilReadOnly: return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
            case ImageLayout_Attachment: return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            case ImageLayout_ReadOnly: return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            case ImageLayout_PresentSrc: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            case ImageLayout_VideoDecodeDst: return VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR;
            case ImageLayout_VideoDecodeSrc: return VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR;
            case ImageLayout_VideoDecodeDpb: return VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
            case ImageLayout_SharedPresent: return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
            case ImageLayout_FragmentDensityMap: return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
            case ImageLayout_FragmentShadingRateAttachment: return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
            case ImageLayout_RenderingLocalRead: return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
            case ImageLayout_VideoEncodeDst: return VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR;
            case ImageLayout_VideoEncodeSrc: return VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR;
            case ImageLayout_VideoEncodeDpb: return VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR;
            case ImageLayout_AttachmentFeedbackLoop: return VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT;
            case ImageLayout_MaxEnum: return VK_IMAGE_LAYOUT_MAX_ENUM;
            }
            unreachable_code();
            return VK_IMAGE_LAYOUT_MAX_ENUM;
        }

        const char* ConvertImageLayoutToStr(ImageLayout layout)
        {
			switch (layout)
			{
			case ImageLayout_Undefined: return "ImageLayout_Undefined";
            case ImageLayout_General: return "ImageLayout_General";
            case ImageLayout_ColorAttachment: return "ImageLayout_ColorAttachment";
            case ImageLayout_DepthStencilAttachment: return "ImageLayout_DepthStencilAttachment";
            case ImageLayout_DepthStencilReadOnly: return "ImageLayout_DepthStencilReadOnly";
            case ImageLayout_ShaderReadOnly: return "ImageLayout_ShaderReadOnly";
            case ImageLayout_TransferSrc: return "ImageLayout_TransferSrc";
            case ImageLayout_TransferDst: return "ImageLayout_TransferDst";
            case ImageLayout_Preinitialized: return "ImageLayout_Preinitialized";
            case ImageLayout_DepthReadOnly_StencilAttachment: return "ImageLayout_DepthReadOnly_StencilAttachment";
            case ImageLayout_DepthAttachment_StencilReadOnly: return "ImageLayout_DepthAttachment_StencilReadOnly";
            case ImageLayout_DepthAttachment: return "ImageLayout_DepthAttachment";
            case ImageLayout_DepthReadOnly: return "ImageLayout_DepthReadOnly";
            case ImageLayout_StencilAttachment: return "ImageLayout_StencilAttachment";
            case ImageLayout_StencilReadOnly: return "ImageLayout_StencilReadOnly";
            case ImageLayout_Attachment: return "ImageLayout_Attachment";
            case ImageLayout_ReadOnly: return "ImageLayout_ReadOnly";
            case ImageLayout_PresentSrc: return "ImageLayout_PresentSrc";
            case ImageLayout_VideoDecodeDst: return "ImageLayout_VideoDecodeDst";
            case ImageLayout_VideoDecodeSrc: return "ImageLayout_VideoDecodeSrc";
            case ImageLayout_VideoDecodeDpb: return "ImageLayout_VideoDecodeDpb";
            case ImageLayout_SharedPresent: return "ImageLayout_SharedPresent";
            case ImageLayout_FragmentDensityMap: return "ImageLayout_FragmentDensityMap";
            case ImageLayout_FragmentShadingRateAttachment: return "ImageLayout_FragmentShadingRateAttachment";
            case ImageLayout_RenderingLocalRead: return "ImageLayout_RenderingLocalRead";
            case ImageLayout_VideoEncodeDst: return "ImageLayout_VideoEncodeDst";
            case ImageLayout_VideoEncodeSrc: return "ImageLayout_VideoEncodeSrc";
            case ImageLayout_VideoEncodeDpb: return "ImageLayout_VideoEncodeDpb";
            case ImageLayout_AttachmentFeedbackLoop: return "ImageLayout_AttachmentFeedbackLoop";
            case ImageLayout_MaxEnum: return "ImageLayout_MaxEnum";
			}
			unreachable_code();
			return "ImageLayout_Undefined";
        }

#if !defined(RBE_MEM_MANAGEMENT)
        VmaMemoryUsage ConvertMemoryUsage(MemoryUsage usage)
        {
            switch (usage)
            {
            case MemoryUsage_Unknown: return VMA_MEMORY_USAGE_UNKNOWN;
            case MemoryUsage_Cpu: return VMA_MEMORY_USAGE_CPU_ONLY;
            case MemoryUsage_CpuCopy: return VMA_MEMORY_USAGE_CPU_COPY;
            case MemoryUsage_CpuToGpu: return VMA_MEMORY_USAGE_CPU_TO_GPU;
            case MemoryUsage_Gpu: return VMA_MEMORY_USAGE_GPU_ONLY;
            case MemoryUsage_GpuToCpu: return VMA_MEMORY_USAGE_GPU_TO_CPU;
            }
            unreachable_code();
            return VMA_MEMORY_USAGE_UNKNOWN;
        }
#else
        VkMemoryPropertyFlags GetMemPropertyFlags(MemoryUsage usage)
        {
            VkMemoryPropertyFlags flags = 0;
            switch (usage)
            {
            case MemoryUsage_Unknown:
                break;
            case MemoryUsage_Gpu:
                flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case MemoryUsage_Cpu:
                flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            case MemoryUsage_CpuToGpu:
                flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case MemoryUsage_GpuToCpu:
                flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                break;
            case MemoryUsage_CpuToGpu:
                //notPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                //break;
            default:
                unreachable_code();
                break;
            }
            return flags;
        }
#endif
        VkImageType ConvertImageType(ImageDimension dim)
        {
            switch (dim)
            {
            case ImageDimension_1D:
            case ImageDimension_1DArray: return VK_IMAGE_TYPE_1D;
            case ImageDimension_Cube:
            case ImageDimension_CubeArray: 
            case ImageDimension_2D:
            case ImageDimension_2DArray: return VK_IMAGE_TYPE_2D;
            case ImageDimension_3D: return VK_IMAGE_TYPE_3D;
            case ImageDimension_MaxEnum: return VK_IMAGE_TYPE_MAX_ENUM;
            }
            unreachable_code();
            return VK_IMAGE_TYPE_MAX_ENUM;
        }

        VkImageViewType ConvertImageViewType(ImageDimension dim)
        {
            switch (dim)
            {
            case ImageDimension_1D: return VK_IMAGE_VIEW_TYPE_1D;
            case ImageDimension_1DArray: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case ImageDimension_2D: return VK_IMAGE_VIEW_TYPE_2D;
            case ImageDimension_2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case ImageDimension_3D: return VK_IMAGE_VIEW_TYPE_3D;
            case ImageDimension_Cube: return VK_IMAGE_VIEW_TYPE_CUBE;
            case ImageDimension_CubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            case ImageDimension_MaxEnum: return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
            }
            unreachable_code();
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }

        VkImageAspectFlags ConvertImageAspectFlags(Format format)
        {
            VkImageAspectFlags flags = VK_IMAGE_ASPECT_NONE;
            if (IsDepthFormat(format))
                flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
            if (IsStencilFormat(format))
                flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
            if (!flags)
                flags = VK_IMAGE_ASPECT_COLOR_BIT;
            return flags;
        }

        bool IsDepthFormat(Format format)
        {
            switch (format)
            {
            case Format_D16_UNorm:
            case Format_X8_D24_UNorm_Pack32:
            case Format_D16_UNorm_S8_UInt:
            case Format_D24_UNorm_S8_UInt:
            case Format_D32_SFloat_S8_UInt:
            case Format_D32_SFloat: return true;
            default:
                return false;
            }
            unreachable_code();
            return false;
        }

        bool IsStencilFormat(Format format)
        {
            switch (format)
            {
            case Format_S8_UInt:
            case Format_D16_UNorm_S8_UInt:
            case Format_D24_UNorm_S8_UInt:
            case Format_D32_SFloat_S8_UInt: return true;
            default:
                return false;
            }
            unreachable_code();
            return false;
        }

        bool IsDepthStencilFormat(Format format)
        {
            switch (format)
            {
            case Format_D16_UNorm_S8_UInt:
            case Format_D24_UNorm_S8_UInt:
            case Format_D32_SFloat_S8_UInt: return true;
            default:
                return false;
            }
            unreachable_code();
            return false;
        }

        VkImageSubresourceRange ConvertImageSubresourceRange(const TextureSubresourceRange& range, Format format)
        {
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = ConvertImageAspectFlags(format);
            subresourceRange.baseMipLevel = range.baseMipLevel;
            subresourceRange.levelCount = range.countMipLevels == TextureSubresourceRange::AllMipLevels ? VK_REMAINING_MIP_LEVELS : range.countMipLevels;
            subresourceRange.baseArrayLayer = range.baseLayer;
            subresourceRange.layerCount = range.countLayers == TextureSubresourceRange::AllLayers ? VK_REMAINING_ARRAY_LAYERS : range.countLayers;
            return subresourceRange;
        }

        VkImageSubresourceLayers ConvertImageSubresourceLayer(const TextureSubresourceLayer& layer, Format format)
        {
            VkImageSubresourceLayers ret;
            ret.aspectMask = ConvertImageAspectFlags(format);
            ret.mipLevel = layer.mipLevel;
            ret.baseArrayLayer = layer.layer;
            ret.layerCount = layer.layerCount;
            return ret;
        }

        VkFilter ConvertFilter(Filter filter)
        {
            switch (filter)
            {
            case Filter_Nearest: return VK_FILTER_NEAREST;
            case Filter_Linear: return VK_FILTER_LINEAR;
            case Filter_Cubic: return VK_FILTER_CUBIC_EXT;
            }
            unreachable_code();
            return VK_FILTER_MAX_ENUM;
        }

        VkSamplerMipmapMode ConvertMipmapMode(Filter filter)
        {
            switch (filter)
            {
            case Filter_Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case Filter_Cubic:
                logfwarn("Invalid filter type to convert to mipmap mode: %d. Replaced by LINEAR sampler.\n", filter);
            case Filter_Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            }
            unreachable_code();
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }

        VkSamplerAddressMode ConvertSamplerAddressMode(SamplerAddressMode mode)
        {
            switch (mode)
            {
            case SamplerAddressMode_Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case SamplerAddressMode_ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case SamplerAddressMode_ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case SamplerAddressMode_MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            case SamplerAddressMode_MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            }
            unreachable_code();
            return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
        }

        VkShaderStageFlags ConvertShaderStage(ShaderType type)
        {
            VkShaderStageFlags flags = 0;
            if (type & ShaderType_Vertex) flags |= VK_SHADER_STAGE_VERTEX_BIT;
            if (type & ShaderType_TesselationControl) flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            if (type & ShaderType_TesselationEvaluation) flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            if (type & ShaderType_Geometry) flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
            if (type & ShaderType_Fragment) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            if (type & ShaderType_Compute) flags |= VK_SHADER_STAGE_COMPUTE_BIT;
            if (type & ShaderType_RayGen) flags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            if (type & ShaderType_RayAnyHit) flags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
            if (type & ShaderType_RayClosestHit) flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            if (type & ShaderType_RayMiss) flags |= VK_SHADER_STAGE_MISS_BIT_KHR;
            if (type & ShaderType_RayIntersection) flags |= VK_SHADER_STAGE_MISS_BIT_KHR;
            if (type & ShaderType_RayCallable) flags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
            return flags;
        }

        VkBlendFactor ConvertBlendFactor(BlendFactor factor)
        {
            switch (factor)
            {
            case BlendFactor_Zero: return VK_BLEND_FACTOR_ZERO;
            case BlendFactor_One: return VK_BLEND_FACTOR_ONE;
            case BlendFactor_SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
            case BlendFactor_OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case BlendFactor_DstColor: return VK_BLEND_FACTOR_DST_COLOR;
            case BlendFactor_OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case BlendFactor_SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
            case BlendFactor_OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case BlendFactor_DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
            case BlendFactor_OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case BlendFactor_ConstantColor: return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case BlendFactor_OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            case BlendFactor_ConstantAlpha: return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case BlendFactor_OneMinusConstantAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            case BlendFactor_SrcAlphaSaturate: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            case BlendFactor_MaxEnum: return VK_BLEND_FACTOR_MAX_ENUM;
            }
            unreachable_code();
            return VK_BLEND_FACTOR_MAX_ENUM;
        }

        VkBlendOp ConvertBlendOp(BlendOp op)
        {
            switch (op)
            {
            case BlendOp_Add: return VK_BLEND_OP_ADD;
            case BlendOp_Subtract: return VK_BLEND_OP_SUBTRACT;
            case BlendOp_ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
            case BlendOp_Min: return VK_BLEND_OP_MIN;
            case BlendOp_Max: return VK_BLEND_OP_MAX;
            case BlendOp_MaxEnum: return VK_BLEND_OP_MAX_ENUM;
            }
            unreachable_code();
            return VK_BLEND_OP_MAX_ENUM;
        }

        VkCompareOp ConvertCompareOp(CompareOp op)
        {
            switch (op)
            {
            case CompareOp_Never: return VK_COMPARE_OP_NEVER;
            case CompareOp_Less: return VK_COMPARE_OP_LESS;
            case CompareOp_Equal: return VK_COMPARE_OP_EQUAL;
            case CompareOp_LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case CompareOp_Greater: return VK_COMPARE_OP_GREATER;
            case CompareOp_NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
            case CompareOp_GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case CompareOp_Always: return VK_COMPARE_OP_ALWAYS;
            case CompareOp_MaxEnum: return VK_COMPARE_OP_MAX_ENUM;
            }
            unreachable_code();
            return VK_COMPARE_OP_MAX_ENUM;
        }

        VkColorComponentFlags ConvertColorComponentFlags(ColorMask mask)
        {
            VkColorComponentFlags flags = 0;
            if (mask & ColorMask_Red) flags |= VK_COLOR_COMPONENT_R_BIT;
            if (mask & ColorMask_Green) flags |= VK_COLOR_COMPONENT_G_BIT;
            if (mask & ColorMask_Blue) flags |= VK_COLOR_COMPONENT_B_BIT;
            if (mask & ColorMask_Alpha) flags |= VK_COLOR_COMPONENT_A_BIT;
            return flags;
        }

        VkStencilOp ConvertStencilOp(StencilOp op)
        {
            switch (op)
            {
            case StencilOp_Keep: return VK_STENCIL_OP_KEEP;
            case StencilOp_Zero: return VK_STENCIL_OP_ZERO;
            case StencilOp_Replace: return VK_STENCIL_OP_REPLACE;
            case StencilOp_IncrementAndClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
            case StencilOp_DecrementAndClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
            case StencilOp_Invert: return VK_STENCIL_OP_INVERT;
            case StencilOp_IncrementAndWrap: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
            case StencilOp_DecrementAndWrap: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
            case StencilOp_MaxEnum: return VK_STENCIL_OP_MAX_ENUM;
            }
            unreachable_code();
            return VK_STENCIL_OP_MAX_ENUM;
        }

        VkPrimitiveTopology ConvertPrimitiveType(PrimitiveType type)
        {
            switch (type)
            {
            case PrimitiveType_PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case PrimitiveType_LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case PrimitiveType_LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case PrimitiveType_TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case PrimitiveType_TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case PrimitiveType_TriangleFan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
            case PrimitiveType_PatchList: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            case PrimitiveType_MaxEnum: return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
            }
            unreachable_code();
            return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
        }

        VkPolygonMode ConvertPolygonMode(RasterFillMode mode)
        {
            switch (mode)
            {
            case RasterFillMode_Fill: return VK_POLYGON_MODE_FILL;
            case RasterFillMode_Line: return VK_POLYGON_MODE_LINE;
            case RasterFillMode_Point: return VK_POLYGON_MODE_POINT;
            case RasterFillMode_MaxEnum: return VK_POLYGON_MODE_MAX_ENUM;
            }
            unreachable_code();
            return VK_POLYGON_MODE_MAX_ENUM;
        }

        VkCullModeFlags ConvertCullMode(RasterCullMode mode)
        {
            switch (mode)
            {
            case RasterCullMode_None: return VK_CULL_MODE_NONE;
            case RasterCullMode_Front: return VK_CULL_MODE_FRONT_BIT;
            case RasterCullMode_Back: return VK_CULL_MODE_BACK_BIT;
            case RasterCullMode_FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
            case RasterCullMode_MaxEnum: return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
            }
            unreachable_code();
            return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
        }

        VkQueryType ConvertQueryType(QueryType type)
        {
            switch (type)
            {
            case QueryType_Oclussion: return VK_QUERY_TYPE_OCCLUSION;
            case QueryType_PipelineStatistics: return VK_QUERY_TYPE_PIPELINE_STATISTICS;
            case QueryType_Timestamp: return VK_QUERY_TYPE_TIMESTAMP;
            }
            unreachable_code();
            return VK_QUERY_TYPE_MAX_ENUM;
        }

        void ComputeMipExtent(uint32_t mipLevel, uint32_t width, uint32_t height, uint32_t depth, uint32_t* mipWidth, uint32_t* mipHeight, uint32_t* mipDepth)
        {
            if (mipWidth)
                *mipWidth = __max(width >> mipLevel, 1);
            if (mipHeight)
                *mipHeight = __max(height >> mipLevel, 1);
            if (mipDepth)
                *mipDepth = __max(depth >> mipLevel, 1);
        }

        uint32_t ComputeMipLevels(uint32_t width, uint32_t height)
        {
            return static_cast<uint32_t>(floorf(std::log2f(static_cast<float>(__max(width, height))))) + 1;
        }

        BufferUsage GetBufferUsage(const BufferDescription& description)
        {
            BufferUsage usage = BufferUsage_TransferSrc | BufferUsage_TransferDst | description.bufferUsage;
            return usage;
        }

        ImageUsage GetImageUsage(const TextureDescription& description)
        {
            ImageUsage usage = ImageUsage_TransferDst | ImageUsage_TransferSrc;

            if (description.isRenderTarget)
            {
                if (IsDepthFormat(description.format) || IsStencilFormat(description.format))
                    usage |= ImageUsage_DepthStencilAttachment;
                else
                    usage |= ImageUsage_ColorAttachment;
            }
            if (description.isShaderResource)
                usage |= ImageUsage_Sampled;
            if (description.isStorageTexture)
                usage |= ImageUsage_Storage;
            return usage;
        }
    }
}
