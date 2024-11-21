#include "RenderProfiling.h"
#include "RenderContext.h"
#include "Core/Logger.h"
#include <imgui.h>
#include "Application/CmdParser.h"

namespace Mist
{
	CBoolVar CVar_ShowGpuProf("r_ShowGpuProf", false);

	struct sGpuProfItem
	{
		index_t Id = index_invalid;
		double Value = 0.0;
		tFixedString<64> Label;

		index_t Parent = index_invalid;
		index_t Child = index_invalid;
		index_t Sibling = index_invalid;
	};

	struct sGpuProfStack
	{
		tStaticArray<sGpuProfItem, 128> Items;
		index_t Current = index_invalid;

		void Push(const char* label, index_t id)
		{
			Items.Push();
			index_t newItem = Items.GetSize() - 1;
			sGpuProfItem& item = Items[newItem];
			item.Label = label;
			item.Id = id;
			item.Value = 0.0;
			item.Parent = index_invalid;
			item.Child = index_invalid;
			item.Sibling = index_invalid;

			if (Current != index_invalid)
			{
				item.Parent = Current;
				check(Current != index_invalid);
				sGpuProfItem& parent = Items[Current];
				index_t lastChild = parent.Child;
				for (lastChild; lastChild != index_invalid && (Items[lastChild].Sibling != index_invalid); lastChild = Items[lastChild].Sibling);
				if (lastChild != index_invalid)
					Items[lastChild].Sibling = newItem;
				else
					parent.Child = newItem;
			}
			else
			{
				index_t sibling = 0;
				for (sibling; sibling < Items.GetSize() && (Items[sibling].Parent != index_invalid || sibling == newItem); ++sibling);
				if (sibling < Items.GetSize())
				{
					for (sibling; Items[sibling].Sibling != index_invalid; sibling = Items[sibling].Sibling);
					check(sibling < Items.GetSize());
					Items[sibling].Sibling = newItem;
				}
			}
			Current = newItem;
		}

		index_t Pop()
		{
			check(Current != index_invalid);
			index_t ret = Items[Current].Id;
			Current = Items[Current].Parent;
			return ret;
		}

		void Resolve(const RenderContext& context, const sTimestampQueryPool& queryPool)
		{
			check(Current == index_invalid);
			for (index_t i = 0; i < Items.GetSize(); ++i)
			{
				sGpuProfItem& item = Items[i];
				uint64_t t = queryPool.GetElapsedTimestamp(context.Device, context.GPUDevice, item.Id);
				item.Value = (double)t * 0.001;
			}
		}

		void Reset()
		{
			check(Current == index_invalid);
			Items.Clear();
			Items.Resize(0);
		}
	};

	sGpuProfStack GpuProfStack[globals::MaxOverlappedFrames];


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
			GpuProfStack[context.GetFrameIndex()].Push(label, id);
		}
	}

	uint32_t GpuProf_GetOldestFrameIndex(const RenderContext& context)
	{
		return (context.FrameIndex + 1) % globals::MaxOverlappedFrames;
	}

	void GpuProf_End(const RenderContext& context)
	{
		if (GpuProf_CanMakeProfile(context))
		{
			RenderFrameContext& frameContext = context.GetFrameContext();
			VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;

			index_t id = GpuProfStack[context.GetFrameIndex()].Pop();
			frameContext.GraphicsTimestampQueryPool.EndTimestamp(cmd, id);
		}
	}

	void GpuProf_Reset(const RenderContext& context)
	{
		GpuProfStack[context.GetFrameIndex()].Reset();
		RenderFrameContext& frameContext = context.GetFrameContext();
		frameContext.GraphicsTimestampQueryPool.ResetQueries(frameContext.GraphicsCommandContext.CommandBuffer);
		frameContext.GraphicsTimestampQueryPool.BeginFrame();
	}

	void GpuProf_Resolve(const RenderContext& context)
	{
		if (GpuProf_CanMakeProfile(context))
		{
			uint32_t frameIndex = GpuProf_GetOldestFrameIndex(context);
			const sTimestampQueryPool& queryPool = context.FrameContextArray[frameIndex].GraphicsTimestampQueryPool;
			GpuProfStack[frameIndex].Resolve(context, queryPool);
		}
	}

	void BuildGpuProfTree(sGpuProfStack& stack, index_t root)
	{
		index_t index = root;
		const char* valuefmt = "%8.4f";
		while (index != index_invalid)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			sGpuProfItem& item = stack.Items[index];
			if (item.Child != index_invalid)
			{
				bool treeOpen = ImGui::TreeNodeEx(item.Label.CStr(), 
					ImGuiTreeNodeFlags_SpanAllColumns 
					| ImGuiTreeNodeFlags_DefaultOpen);
				ImGui::TableNextColumn();
				ImGui::Text(valuefmt, item.Value);
				if (treeOpen)
				{
					BuildGpuProfTree(stack, item.Child);
					ImGui::TreePop();
				}
			}
			else
			{
				ImGui::Text("%s", item.Label.CStr());
				ImGui::TableNextColumn();
				ImGui::Text(valuefmt, item.Value);
			}
			index = item.Sibling;
		}

	}

	void GpuProf_ImGuiDraw(const RenderContext& context)
	{
		if (!CVar_ShowGpuProf.Get())
			return;

		uint32_t frameIndex = GpuProf_GetOldestFrameIndex(context);
		sGpuProfStack& stack = GpuProfStack[frameIndex];

		index_t size = stack.Items.GetSize();
		float heightPerLine = 20.f; //approx?
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImVec2 winpos = ImVec2(0.f, viewport->Size.y-heightPerLine*size);
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
}