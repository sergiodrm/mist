#include "ResourceLoader.h"
#include "Core/Thread.h"
#include "Core/Mutex.h"
#include "Core/Types.h"
#include "Core/Logger.h"
#include "Core/Debug.h"
#include "Utils/TimeUtils.h"
#include "RenderSystem/UI.h"
#include "imgui.h"

#define RESOURCE_LOADER_PROFILING
#ifdef RESOURCE_LOADER_PROFILING
#define RESOURCE_LOADER_PROFILEF(_name, _msg, ...) PROFILE_SCOPE_LOGF(_name, _msg, __VA_ARGS__)
#else
#define RESOURCE_LOADER_PROFILEF(...) DUMMY_MACRO
#endif // RESOURCE_LOADER_PROFILING

#define RESOURCE_LOADER_THREAD

namespace Mist
{
	namespace resources
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

		private:
			Thread* m_thread;
			Mutex m_mutex;
			tDynArray<IResourceLoader*> m_loadThreadTasks;
			tDynArray<IResourceLoader*> m_mainThreadTasks;
			tDynArray<IResourceLoader*> m_finishedTasks;
			LoadThreadData m_loadThreadData;
		};

		static ResourceLoaderThread* g_loaderThread = nullptr;

		void InitLoadThread()
		{
			if (g_loaderThread)
				logwarn("Trying to initialize ResourceLoaderThread but it is already initialized!\n");
			else
				g_loaderThread = _new ResourceLoaderThread();
		}

		void TerminateLoadThread()
		{
			if (g_loaderThread)
				delete g_loaderThread;
			g_loaderThread = nullptr;
		}

		void SlotMainThread()
		{
			check(g_loaderThread);
			g_loaderThread->ProcMainThread();
		}

		void PushLoader(IResourceLoader* loader)
		{
			check(g_loaderThread);
			g_loaderThread->PushLoader(loader);
		}

		/**
		 * ResourceLoaderThread Implementation
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
			check(m_loadThreadTasks.empty());
			check(m_mainThreadTasks.empty());
			check(m_finishedTasks.empty());
			delete m_thread;
			m_thread = nullptr;
		}

		void ResourceLoaderThread::PushLoader(IResourceLoader* loader)
		{
			GuardMutex guardMutex(m_mutex);
			switch (loader->GetProcType())
			{
			case IResourceLoader::ProcType::MainThread: PushLoaderIntoMainThread(loader); break;
			case IResourceLoader::ProcType::LoadThread: PushLoaderIntoLoadThread(loader); break;
			default:
				unreachable_code();
			}
		}

		void ResourceLoaderThread::ProcMainThread()
		{
			check(ThisThread::IsMainThread());
			CPU_PROFILE_SCOPE(ResourceLoader_ProcMainThread);
			GuardMutex guardMutex(m_mutex);
			for (uint32_t i = m_mainThreadTasks.size() - 1; i < m_mainThreadTasks.size(); --i)
			{
				IResourceLoader::ProcType procType = m_mainThreadTasks[i]->ProcessMainThread();
				switch (procType)
				{
				case IResourceLoader::ProcType::LoadThread:
					PushLoaderIntoLoadThread(m_mainThreadTasks[i]);
					// not break, pop from main thread requests
				case IResourceLoader::ProcType::Finished:
					MarkLoaderAsFinished(m_mainThreadTasks[i]);
					if (i != m_mainThreadTasks.size() - 1)
						m_mainThreadTasks[i] = m_mainThreadTasks.back();
					m_mainThreadTasks.pop_back();
					break;
				}
			}

			// TODO: delete finished tasks in load thread to not to block main thread?
			FlushFinishedTasks();
		}

		void ResourceLoaderThread::ProcLoadThread(LoadThreadData* loadThreadData)
		{
			tDynArray<IResourceLoader*> loadRequests;

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
					loadRequests.resize(loaderInstance.m_loadThreadTasks.size());
					RESOURCE_LOADER_PROFILEF(ProcLoadThread, "ResourceLoader_LoadThread (%d)", loadRequests.size());
					memcpy_s(loadRequests.data(), loadRequests.size() * sizeof(IResourceLoader*), loaderInstance.m_loadThreadTasks.data(), loadRequests.size() * sizeof(IResourceLoader*));
					loaderInstance.m_loadThreadTasks.resize(0);
					loaderInstance.m_mutex.Unlock();

					// Process local buffer
					while (!loadRequests.empty())
					{
						IResourceLoader* loader = loadRequests.back();
						loadRequests.pop_back();

						IResourceLoader::ProcType procType = loader->ProcessLoadThread();
						switch (procType)
						{
						case IResourceLoader::ProcType::LoadThread:
							loaderInstance.m_mutex.Lock();
							loaderInstance.PushLoaderIntoLoadThread(loader);
							loaderInstance.m_mutex.Unlock();
							break;
						case IResourceLoader::ProcType::MainThread:
							loaderInstance.m_mutex.Lock();
							loaderInstance.PushLoaderIntoMainThread(loader);
							loaderInstance.m_mutex.Unlock();
							break;
						case IResourceLoader::ProcType::Finished:
							loaderInstance.m_mutex.Lock();
							loaderInstance.MarkLoaderAsFinished(loader);
							loaderInstance.m_mutex.Unlock();
							break;
						}
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
	}
}

