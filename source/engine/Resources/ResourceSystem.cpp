#include "ResourceSystem.h"
#include "Core/Thread.h"
#include "Core/Mutex.h"
#include "Core/Types.h"
#include "Core/Logger.h"
#include "Core/Debug.h"
#include "Utils/TimeUtils.h"
#include "RenderSystem/UI.h"
#include "imgui.h"
#include "Render/Model.h"

#define RESOURCE_LOADER_PROFILING
#ifdef RESOURCE_LOADER_PROFILING
#define RESOURCE_LOADER_PROFILEF(_name, _msg, ...) PROFILE_SCOPE_LOGF(_name, _msg, __VA_ARGS__)
#else
#define RESOURCE_LOADER_PROFILEF(...) DUMMY_MACRO
#endif // RESOURCE_LOADER_PROFILING

#define RESOURCE_LOADER_THREAD

namespace Mist
{
	namespace resource_system
	{
		class ResourceLoaderThread
		{
			struct LoadThreadData
			{
				std::atomic<bool> finished = false;
				ResourceLoaderThread* loader = nullptr;
			};
		public:
			ResourceLoaderThread();
			~ResourceLoaderThread();

			void PushLoader(IResourceLoader* loader);
			void ProcMainThread();
		protected:
			static void ProcLoadThread(LoadThreadData* loadThreadData);

			void PushLoaderIntoLoadThread(IResourceLoader* loader);
			void PushLoaderIntoMainThread(IResourceLoader* loader);
			void MarkLoaderAsFinished(IResourceLoader* loader);
			void FlushFinishedTasks();
			
			void ProcessTask(IResourceLoader* loader);

		private:
			Thread* m_thread;
			Mutex m_mutex;
			tDynArray<IResourceLoader*> m_loadThreadTasks;
			tDynArray<IResourceLoader*> m_mainThreadTasks;
			tDynArray<IResourceLoader*> m_finishedTasks;
			LoadThreadData m_loadThreadData;
			
			tDynArray<IResourceLoader*> m_localMainThreadTasks;
			tDynArray<IResourceLoader*> m_localLoadThreadTasks;
		};

		class ResourceCache
		{
			struct ItemInfo
			{
				uint32_t ref;
			};
		public:

			typedef IResource* (*FnCreateResourceInstance)();
			typedef void(*FnDestroyResourceInstance)(IResource* resource);

			struct CacheDescription
			{
				uint32_t count{ 0 };
				FnCreateResourceInstance fnCreate{ nullptr };
				FnDestroyResourceInstance fnDestroy{ nullptr };
			};

			ResourceCache(const CacheDescription& desc);
			~ResourceCache();

			IResource* LoadResource(const char* id);
			IResource* FindResource(const char* id);
			IResource* FindResource(uint64_t h);
			void ReleaseResource(IResource* resource);

		private:
			uint64_t Hash(const char* id);

		private:
			CacheDescription m_description;
			tFixedHeapArray<IResource*> m_resources;
			tFixedHeapArray<ItemInfo> m_infoArray;
			tFixedHeapArray<uint32_t> m_freeSlots;
			tMap<uint64_t, uint32_t> m_indexMap;
		};

		class ResourceManager
		{
		public:
			ResourceManager()
			{
				m_caches[ResourceType_Model] = _new ResourceCache({ 128, 
					// Create
					[]() -> IResource* {
						return _new cModel();
					}, 
					// Destroy
					[](IResource* resource) {
						delete resource;
					}
					});
			}

			~ResourceManager()
			{
				for (uint32_t i = 0; i < ResourceType_Count; ++i)
					delete m_caches[i];
			}

			ResourceCache& GetCache(ResourceType type) { check(type < ResourceType_Count); return *m_caches[type]; }

		private:
			ResourceCache* m_caches[ResourceType_Count];
		};

		static ResourceLoaderThread* g_loaderThread = nullptr;
		static ResourceManager* g_resourceManager = nullptr;

		static bool g_inmediateModeStack[4];
		static uint32_t g_inmediateModeIndex = 0;

		static bool InmediateMode()
		{
			return g_inmediateModeIndex > 0 ? g_inmediateModeStack[g_inmediateModeIndex - 1] : false;
		}

		static size_t GetSizeOfResourceType(ResourceType type)
		{
			switch (type)
			{
			case ResourceType_Model: return sizeof(cModel);
			}
			unreachable_code();
			return 0;
		}

		static ResourceManager* GetResourceManager() { return g_resourceManager; }

		/**
		 * Load thread implementation
		 */

		static void InitLoadThread()
		{
			if (g_loaderThread)
				logwarn("Trying to initialize ResourceLoaderThread but it is already initialized!\n");
			else
				g_loaderThread = _new ResourceLoaderThread();
		}

		static void TerminateLoadThread()
		{
			if (g_loaderThread)
				delete g_loaderThread;
			g_loaderThread = nullptr;
		}

		static void SlotMainThread()
		{
			check(g_loaderThread);
			g_loaderThread->ProcMainThread();
		}

		/**
		 * Public API implementation
		 */
		void Init()
		{
			check(!g_resourceManager);
			g_resourceManager = _new ResourceManager();
			InitLoadThread();
		}

		void Terminate()
		{
			TerminateLoadThread();
			check(g_resourceManager);
			delete g_resourceManager;
			g_resourceManager = nullptr;
		}

		void Update()
		{
			SlotMainThread();
		}

		void PushLoader(IResourceLoader* loader)
		{
			check(g_loaderThread);
			g_loaderThread->PushLoader(loader);
		}

		void PushInmediateMode(bool inmediateEnabled)
		{
			check(Mist::ThisThread::IsMainThread());
			check(g_inmediateModeIndex < Mist::CountOf(g_inmediateModeStack));
			g_inmediateModeStack[g_inmediateModeIndex++] = inmediateEnabled;
		}

		void PopInmediateMode()
		{
			check(Mist::ThisThread::IsMainThread());
			check(g_inmediateModeIndex > 0);
			--g_inmediateModeIndex;
		}

		IResource* LoadResource(ResourceType type, const char* id)
		{
			check(type < ResourceType_Count && id && *id);
			return GetResourceManager()->GetCache(type).LoadResource(id);
		}

		void DestroyResource(IResource* resource)
		{
			check(resource);
			GetResourceManager()->GetCache(resource->GetResourceType()).ReleaseResource(resource);
		}

		/**
		 * Internal ResourceLoaderThread Implementation
		 */

		ResourceLoaderThread::ResourceLoaderThread()
			: m_thread(nullptr)
		{ 
			m_loadThreadData.finished = false;
			m_loadThreadData.loader = this;
			m_thread = _new Thread(&ProcLoadThread, &m_loadThreadData);
		}

		ResourceLoaderThread::~ResourceLoaderThread()
		{
			m_loadThreadData.finished = true;
			m_thread->Join();
			delete m_thread;
			m_thread = nullptr;
			
			m_finishedTasks.insert(m_finishedTasks.end(), m_mainThreadTasks.begin(), m_mainThreadTasks.end());
			m_finishedTasks.insert(m_finishedTasks.end(), m_loadThreadTasks.begin(), m_loadThreadTasks.end());
			m_finishedTasks.insert(m_finishedTasks.end(), m_localMainThreadTasks.begin(), m_localMainThreadTasks.end());
			m_finishedTasks.insert(m_finishedTasks.end(), m_localLoadThreadTasks.begin(), m_localLoadThreadTasks.end());
			FlushFinishedTasks();
			check(m_finishedTasks.empty());
			m_loadThreadTasks.clear();
			m_mainThreadTasks.clear();
		}

		void ResourceLoaderThread::PushLoader(IResourceLoader* loader)
		{
			if (!InmediateMode())
			{
				GuardMutex guardMutex(m_mutex);
				switch (loader->GetProcType())
				{
				case IResourceLoader::ProcType::MainThread: PushLoaderIntoMainThread(loader); break;
				case IResourceLoader::ProcType::LoadThread: PushLoaderIntoLoadThread(loader); break;
				case IResourceLoader::ProcType::Finished:
					unreachable_code();
				}
			}
			else
			{
				check(ThisThread::IsMainThread());
				m_mutex.Lock();
				FlushFinishedTasks();
				m_mutex.Unlock();

				IResourceLoader::ProcType type = loader->GetProcType();
				while (type != IResourceLoader::ProcType::Finished)
				{
					switch (type)
					{
					case IResourceLoader::ProcType::MainThread: 
						type = loader->ProcessMainThread();
						break;
					case IResourceLoader::ProcType::LoadThread: 
						type = loader->ProcessLoadThread();
						break;
					}
				}
				m_mutex.Lock();
				m_finishedTasks.push_back(loader);
				FlushFinishedTasks();
				m_mutex.Unlock();
			}
		}

		void ResourceLoaderThread::ProcMainThread()
		{
			check(ThisThread::IsMainThread());
			PROF_SCOPE(ResourceLoader_ProcMainThread);
			
			m_mutex.Lock();
			std::swap(m_localMainThreadTasks, m_mainThreadTasks);
			m_mutex.Unlock();
			
			// Process local buffer
			while (!m_localMainThreadTasks.empty())
			{
				IResourceLoader* loader = m_localMainThreadTasks.back();
				m_localMainThreadTasks.pop_back();

				IResourceLoader::ProcType procType = loader->ProcessMainThread();
				m_mutex.Lock();
				switch (procType)
				{
				case IResourceLoader::ProcType::LoadThread:
					PushLoaderIntoLoadThread(loader);
					break;
				case IResourceLoader::ProcType::MainThread:
					PushLoaderIntoMainThread(loader);
					break;
				case IResourceLoader::ProcType::Finished:
					MarkLoaderAsFinished(loader);
					break;
				}
				m_mutex.Unlock();
			}

			// TODO: delete finished tasks in load thread to not to block main thread?
			FlushFinishedTasks();
		}

		void ResourceLoaderThread::ProcLoadThread(LoadThreadData* loadThreadData)
		{
			while (!loadThreadData->finished)
			{
				ResourceLoaderThread& loaderInstance = *loadThreadData->loader;
				loaderInstance.m_mutex.Lock();
				const bool emptyLoadRequests = loaderInstance.m_loadThreadTasks.empty();
				loaderInstance.m_mutex.Unlock();

				if (emptyLoadRequests)
				{
					ThisThread::YieldTask();
					ThisThread::SleepFor(1000);
				}
				else
				{
					// Copy requests to local buffer
					loaderInstance.m_mutex.Lock();
					RESOURCE_LOADER_PROFILEF(ProcLoadThread, "ResourceLoader_LoadThread (%d)", loaderInstance.m_loadThreadTasks.size());
					std::swap(loaderInstance.m_localLoadThreadTasks, loaderInstance.m_loadThreadTasks);
					loaderInstance.m_mutex.Unlock();

					// Process local buffer
					while (!loaderInstance.m_localLoadThreadTasks.empty() && !loadThreadData->finished)
					{
						PROF_ZONE_SCOPED("ResourceLoader_ProcLoadThread");
						IResourceLoader* loader = loaderInstance.m_localLoadThreadTasks.back();
						loaderInstance.m_localLoadThreadTasks.pop_back();

						IResourceLoader::ProcType procType = loader->ProcessLoadThread();
						loaderInstance.m_mutex.Lock();
						switch (procType)
						{
						case IResourceLoader::ProcType::LoadThread:
							loaderInstance.PushLoaderIntoLoadThread(loader);
							break;
						case IResourceLoader::ProcType::MainThread:
							loaderInstance.PushLoaderIntoMainThread(loader);
							break;
						case IResourceLoader::ProcType::Finished:
							loaderInstance.MarkLoaderAsFinished(loader);
							break;
						}
						loaderInstance.m_mutex.Unlock();
					}
				}
			}
		}

		void ResourceLoaderThread::PushLoaderIntoLoadThread(IResourceLoader* loader)
		{
			m_loadThreadTasks.push_back(loader);
		}

		void ResourceLoaderThread::PushLoaderIntoMainThread(IResourceLoader* loader)
		{
			m_mainThreadTasks.push_back(loader);
		}

		void ResourceLoaderThread::MarkLoaderAsFinished(IResourceLoader* loader)
		{
			m_finishedTasks.push_back(loader);
		}

		void ResourceLoaderThread::FlushFinishedTasks()
		{
			for (uint32_t i = 0; i < m_finishedTasks.size(); ++i)
				delete m_finishedTasks[i];
			m_finishedTasks.clear();
		}

		/**
		 * Internal Resource cache implementation
		 */

		ResourceCache::ResourceCache(const CacheDescription& desc)
			: m_description(desc)
		{
			check(m_description.count);
			check(m_description.fnCreate);
			check(m_description.fnDestroy);
			m_resources.AllocateAndResize(m_description.count);
			m_infoArray.AllocateAndResize(m_description.count);
			m_freeSlots.AllocateAndResize(m_description.count);
			for (uint32_t i = 0; i < m_resources.GetSize(); ++i)
			{
				m_freeSlots[i] = m_resources.GetSize() - 1 - i;
				m_infoArray[i] = { UINT32_MAX };
			}
		}

		ResourceCache::~ResourceCache()
		{
			check(m_indexMap.empty());
		}

		IResource* ResourceCache::LoadResource(const char* id)
		{
			uint64_t h = Hash(id);
			IResource* r = FindResource(h);
			if (r)
			{
				// Add reference counter to resource
				auto it = m_indexMap.find(r->GetResourceHashId());
				check(it != m_indexMap.end());
				++m_infoArray[it->second].ref;
				logfinfo("[LoadResource] Resource in memory (%s) (refs: %d)\n", id, m_infoArray[it->second].ref);
				return r;
			}

			// Check if there is enough room
			check(m_freeSlots.GetSize() > 0);

			// Gather free resource index
			uint32_t index = m_freeSlots.Back();
			m_freeSlots.Back() = UINT32_MAX;
			m_freeSlots.Pop();

  			// Gather resource memory and build
			check(!m_resources[index]);
			m_resources[index] = (*m_description.fnCreate)();
			check(m_resources[index]);
			r = m_resources[index];
			r->SetResourceHashId(h);
			r->SetResourceName(id);

			// Submit new resource into registers and reference counters
			check(!m_indexMap.contains(h));
			check(m_infoArray[index].ref == UINT32_MAX);
			m_indexMap[h] = index;
			m_infoArray[index].ref = 1;

			r->LoadResource();

			logfinfo("[LoadResource] Resource created (%s) (refs: %d)\n", id, m_infoArray[index].ref);
			return r;
		}

		IResource* ResourceCache::FindResource(const char* id)
		{
			uint64_t h = Hash(id);
			return FindResource(h);
		}

		IResource* ResourceCache::FindResource(uint64_t h)
		{
			auto it = m_indexMap.find(h);
			return it != m_indexMap.end() ? m_resources[it->second] : nullptr;
		}

		void ResourceCache::ReleaseResource(IResource* resource)
		{
			IResource* r = resource;
			auto it = m_indexMap.find(r->GetResourceHashId());
			check(it != m_indexMap.end());
			check(m_infoArray[it->second].ref > 0);
			check(m_resources[it->second] == resource);
			uint32_t c = --m_infoArray[it->second].ref;
			logfinfo("[ReleaseResource] Resource in memory (%s) (refs: %d)\n", r->GetResourceName(), m_infoArray[it->second].ref);
			if (!c)
			{
				logfinfo("[ReleaseResource] Resource with zero refs (%s)\n", r->GetResourceName());
				r->DestroyResource();
				(*m_description.fnDestroy)(r);
				m_infoArray[it->second].ref = UINT32_MAX;
				m_freeSlots.Push();
				m_freeSlots.Back() = it->second;
				m_resources[it->second] = nullptr;
				m_indexMap.erase(it);
			}
		}

		uint64_t ResourceCache::Hash(const char* id)
		{
			return hash(id);
		}
	}
}

