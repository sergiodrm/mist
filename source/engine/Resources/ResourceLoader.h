#pragma once

namespace Mist
{
    namespace resources
    {
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

        void InitLoadThread();
        void TerminateLoadThread();
        void SlotMainThread();
        void PushLoader(IResourceLoader* loader);
    }
}