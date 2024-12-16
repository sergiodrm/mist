#include "RenderProfiling.h"
#include "RenderContext.h"
#include "Core/Logger.h"
#include <imgui.h>
#include "Application/CmdParser.h"

namespace Mist
{
	CBoolVar CVar_ShowGpuProf("r_ShowGpuProf", false);

	uint32_t GpuProf_GetOldestFrameIndex(const RenderContext& context)
	{
		return (context.FrameIndex + 1) % globals::MaxOverlappedFrames;
	}

	struct tGpuProfItem
	{
		index_t Id = index_invalid;
		double Value = 0.0;
		tFixedString<64> Label;
	};

	struct tGpuProfiler
	{
		typedef tStackTree<tGpuProfItem, 16> tGpuProfStackTree;
		tGpuProfStackTree GpuProfStacks[globals::MaxOverlappedFrames];

		void Resolve(const RenderContext& context)
		{
			uint32_t lastIndex = GpuProf_GetOldestFrameIndex(context);
			const sTimestampQueryPool& queryPool = context.FrameContextArray[lastIndex].GraphicsTimestampQueryPool;
			tGpuProfStackTree& stack = GpuProfStacks[lastIndex];
			check(stack.Current == index_invalid);
			for (index_t i = 0; i < stack.Data.GetSize(); ++i)
			{
				tGpuProfItem& item = stack.Data[i];
				uint64_t t = queryPool.GetElapsedTimestamp(context.Device, context.GPUDevice, item.Id);
				item.Value = (double)t * 0.001;
			}
		}

		static void BuildGpuProfTree(tGpuProfStackTree& stack, index_t root)
		{
			index_t index = root;
			const char* valuefmt = "%8.4f";
			while (index != index_invalid)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				tGpuProfStackTree::tItem& item = stack.Items[index];
				tGpuProfItem& data = stack.Data[item.DataIndex];
				if (item.Child != index_invalid)
				{
					bool treeOpen = ImGui::TreeNodeEx(data.Label.CStr(),
						ImGuiTreeNodeFlags_SpanAllColumns
						| ImGuiTreeNodeFlags_DefaultOpen);
					ImGui::TableNextColumn();
					ImGui::Text(valuefmt, data.Value);
					if (treeOpen)
					{
						BuildGpuProfTree(stack, item.Child);
						ImGui::TreePop();
					}
				}
				else
				{
					ImGui::Text("%s", data.Label.CStr());
					ImGui::TableNextColumn();
					ImGui::Text(valuefmt, stack.Data[item.DataIndex].Value);
				}
				index = item.Sibling;
			}
		}

		void ImGuiDraw(const RenderContext& context)
		{
			uint32_t frameIndex = GpuProf_GetOldestFrameIndex(context);
			tGpuProfiler::tGpuProfStackTree& stack = GpuProfStacks[frameIndex];

			index_t size = stack.Items.GetSize();
			float heightPerLine = 20.f; //approx?
			ImGuiViewport* viewport = ImGui::GetMainViewport();

			ImVec2 winpos = ImVec2(0.f, viewport->Size.y - heightPerLine * size);
			ImGui::SetNextWindowPos(winpos);
			ImGui::SetNextWindowSize(ImVec2(300.f, (float)size * heightPerLine));
			ImGui::SetNextWindowBgAlpha(0.f);
			ImGui::Begin("Gpu profiling", nullptr, ImGuiWindowFlags_NoDecoration
				| ImGuiWindowFlags_NoBackground
				| ImGuiWindowFlags_NoDocking);
			if (!stack.Items.IsEmpty())
			{
				ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH
					| ImGuiTableFlags_Resizable
					| ImGuiTableFlags_RowBg
					| ImGuiTableFlags_NoBordersInBody;
				if (ImGui::BeginTable("GpuProf", 2, flags))
				{
					ImGui::TableSetupColumn("Gpu process");
					ImGui::TableSetupColumn("Time (us)");
					ImGui::TableHeadersRow();
					BuildGpuProfTree(stack, 0);
					ImGui::EndTable();
				}
			}
			ImGui::End();
		}

		double GetTimeStat(const RenderContext& context, const char* label)
		{
			uint32_t frameIndex = GpuProf_GetOldestFrameIndex(context);
			tGpuProfStackTree& stack = GpuProfStacks[frameIndex];
			index_t index = index_invalid;
			for (index_t i = 0; i < stack.Data.GetSize(); ++i)
			{
				if (stack.Data[i].Label == label)
				{
					index = i;
					break;
				}
			}
			if (index != index_invalid)
				return stack.Data[index].Value;
			return 0.0;
		}
	};

	tGpuProfiler GpuProfiler;



	void sTimestampQueryPool::Init(VkDevice device, index_t queryCount)
	{
		check(Flags == QueryPool_None);
		VkQueryPoolCreateInfo queryInfo
		{
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queryType = VK_QUERY_TYPE_TIMESTAMP,
			.queryCount = queryCount,
			.pipelineStatistics = 0
		};
		vkcheck(vkCreateQueryPool(device, &queryInfo, nullptr, &QueryPool));
		Flags = QueryPool_Terminated;

		QueryCounter = 0;
		QueryPoolReserved = queryCount;
	}

	void sTimestampQueryPool::Destroy(VkDevice device)
	{
		check(Flags == QueryPool_Terminated);
		vkDestroyQueryPool(device, QueryPool, nullptr);
	}

	void sTimestampQueryPool::ResetQueries(VkCommandBuffer cmd)
	{
		check(Flags == QueryPool_Terminated);
		vkCmdResetQueryPool(cmd, QueryPool, 0, QueryPoolReserved);
		Flags = QueryPool_Reset;
		QueryCounter = 0;
	}

	index_t sTimestampQueryPool::BeginTimestamp(VkCommandBuffer cmd)
	{
		check(Flags == QueryPool_Running);
		check(QueryCounter < QueryPoolReserved);
		index_t currentId = QueryCounter;
		QueryCounter += 2;
		vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, QueryPool, currentId);
		return currentId;
	}

	void sTimestampQueryPool::EndTimestamp(VkCommandBuffer cmd, index_t query)
	{
		check(Flags == QueryPool_Running);
		check(query != index_invalid && query < QueryCounter - 1);
		vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, QueryPool, query + 1);
	}

	void sTimestampQueryPool::GetQuery(VkDevice device, uint32_t queryId, uint32_t queryCounter, uint64_t* queryData) const
	{
		check(Flags == QueryPool_Terminated);
		check(queryData);
		check(queryCounter);
		check(queryId < QueryCounter);
		vkGetQueryPoolResults(device, QueryPool, queryId, queryCounter, sizeof(uint64_t) * queryCounter, queryData, 0, VK_QUERY_RESULT_64_BIT);
	}

	uint64_t sTimestampQueryPool::GetElapsedTimestamp(VkDevice device, VkPhysicalDevice physicalDevice, index_t query) const
	{
		uint64_t timestamps[2];
		vkGetQueryPoolResults(device, QueryPool, query, 2, sizeof(uint64_t) * 2, timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		float period = properties.limits.timestampPeriod;
		uint64_t diff = timestamps[1] - timestamps[0];
		uint64_t ns = (uint64_t)((float)diff * period);
		return diff;
	}

	bool GpuProf_CanMakeProfile(const RenderContext& context)
	{
#if 0
		return context.FrameIndex > globals::MaxOverlappedFrames && context.FrameIndex % globals::MaxOverlappedFrames == 0;
#else
		return true;
#endif // 0

	}

	void GpuProf_Begin(const RenderContext& context, const char* label)
	{
		if (GpuProf_CanMakeProfile(context))
		{
			RenderFrameContext& frameContext = context.GetFrameContext();
			VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;
			sTimestampQueryPool& queryPool = frameContext.GraphicsTimestampQueryPool;
			index_t id = queryPool.BeginTimestamp(cmd);
			GpuProfiler.GpuProfStacks[context.GetFrameIndex()].Push({ id, 0.0, label });
		}
	}

	void GpuProf_End(const RenderContext& context)
	{
		if (GpuProf_CanMakeProfile(context))
		{
			RenderFrameContext& frameContext = context.GetFrameContext();
			VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;

			index_t id = GpuProfiler.GpuProfStacks[context.GetFrameIndex()].GetCurrent().Id;
			frameContext.GraphicsTimestampQueryPool.EndTimestamp(cmd, id);
			GpuProfiler.GpuProfStacks[context.GetFrameIndex()].Pop();
		}
	}

	void GpuProf_Reset(const RenderContext& context)
	{
		GpuProfiler.GpuProfStacks[context.GetFrameIndex()].Reset();
		RenderFrameContext& frameContext = context.GetFrameContext();
		frameContext.GraphicsTimestampQueryPool.ResetQueries(frameContext.GraphicsCommandContext.CommandBuffer);
		frameContext.GraphicsTimestampQueryPool.BeginFrame();
	}

	void GpuProf_Resolve(const RenderContext& context)
	{
		if (GpuProf_CanMakeProfile(context))
		{
			GpuProfiler.Resolve(context);
		}
	}

	void GpuProf_ImGuiDraw(const RenderContext& context)
	{
		if (!CVar_ShowGpuProf.Get())
			return;
		GpuProfiler.ImGuiDraw(context);
	}

	double GpuProf_GetGpuTime(const RenderContext& context, const char* label)
	{
		return GpuProfiler.GetTimeStat(context, label);
	}

}