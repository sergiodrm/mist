#pragma once

#include "Render/RenderAPI.h"
#include "Render/RenderTypes.h"
#include "Core/Types.h"

namespace Mist
{
	struct RenderContext;

	struct sTimestampQueryPool
	{
		enum
		{
			QueryPool_None = 0x00,
			QueryPool_Reset = 0x01,
			QueryPool_Running = 0x02,
			QueryPool_Terminated = 0x04
		};

		uint8_t Flags = QueryPool_None;
		VkQueryPool QueryPool;
		index_t QueryPoolReserved;
		index_t QueryCounter;

		void Init(VkDevice device, index_t queryCount);
		void Destroy(VkDevice device);

		inline void BeginFrame() { Flags = QueryPool_Running; }
		inline void EndFrame() { Flags = QueryPool_Terminated; }

		void ResetQueries(VkCommandBuffer cmd);

		index_t BeginTimestamp(VkCommandBuffer cmd);
		void EndTimestamp(VkCommandBuffer cmd, index_t query);
		void GetQuery(VkDevice device, uint32_t queryId, uint32_t queryCounter, uint64_t* queryData) const;

		// return elapsed time in nanoseconds
		uint64_t GetElapsedTimestamp(VkDevice device, VkPhysicalDevice physicalDevice, index_t query) const;
	};

	void GpuProf_Begin(const RenderContext& context, const char* label);
	void GpuProf_End(const RenderContext& context);
	void GpuProf_Reset(const RenderContext& context);
	void GpuProf_Resolve(const RenderContext& context);
	void GpuProf_ImGuiDraw(const RenderContext& context);
	double GpuProf_GetGpuTime(const RenderContext& context, const char* label);
}