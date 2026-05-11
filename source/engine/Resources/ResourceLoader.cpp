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

		static ResourceLoaderThread* g_loaderThread = nullptr;

		static bool g_inmediateModeStack[4];
		static uint32_t g_inmediateModeIndex = 0;

		static bool InmediateMode()
		{
			return g_inmediateModeIndex > 0 ? g_inmediateModeStack[g_inmediateModeIndex - 1] : false;
		}

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
			CPU_PROFILE_SCOPE(ResourceLoader_ProcMainThread);
			
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
	}
}

