#pragma once

#include "Core/Types.h"

namespace Mist
{
    namespace resource_system
    {
        class IResource;
        class IResourceLoader;

		enum ResourceType
		{
			ResourceType_Model,
			ResourceType_Count
		};

        /**
         * Resource API system functions
         */
        void Init();
        void Terminate();
        void Update();

        /**
         * Resource API load functions
         */
		IResource* LoadResource(ResourceType type, const char* id);
        void DestroyResource(IResource* resource);

        /**
         * Resource API async load functions
         */
        void PushLoader(IResourceLoader* loader);
        void PushInmediateMode(bool inmediateEnabled); 
        void PopInmediateMode();

        // Utility to cast IResource object as user resource object.
        template <typename T>
        T* LoadResource(const char* id) { return static_cast<T*>(LoadResource(T::GetStaticResourceType(), id));  }

        class IResourceLoader
        {
        public:
            enum class ProcType
            {
                LoadThread,
                MainThread,
                Finished
            };
            virtual ~IResourceLoader() = default;
            virtual ProcType GetProcType() const = 0;
            virtual ProcType ProcessLoadThread() = 0;
            virtual ProcType ProcessMainThread() = 0;
        };

        class IResource
        {
        public:
            virtual ~IResource() {}

            uint64_t GetResourceHashId() const { return m_hashId; }
            void SetResourceHashId(uint64_t hashId) { m_hashId = hashId; }

            const char* GetResourceName() const { return m_resourceName; }
            void SetResourceName(const char* name) { ::strcpy_s(m_resourceName, name); }

            virtual ResourceType GetResourceType() const = 0;
            virtual bool LoadResource() = 0;
            virtual void DestroyResource() = 0;

        private:
            uint64_t m_hashId{ UINT64_MAX };
            char m_resourceName[128];
        };

        template <ResourceType T>
        class TResource : public IResource
        {
        public:
            static ResourceType GetStaticResourceType() { return T; }
            virtual ResourceType GetResourceType() const final { return GetStaticResourceType(); }
        };
    }
}