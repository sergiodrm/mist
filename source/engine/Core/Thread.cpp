#include "Thread.h"
#include "Debug.h"

namespace Mist
{
    ThreadId ThisThread::s_mainThreadId = {};

    static ThreadId GetThisThreadId()
    {
        return _Thrd_id();
    }

    bool ThisThread::IsMainThread()
    {
        check(s_mainThreadId != 0);
        return s_mainThreadId == GetId();
    }

    void ThisThread::SetMainThread()
    {
        check(s_mainThreadId == 0);
        s_mainThreadId = GetId();
    }

    ThreadId ThisThread::GetId()
    {
        return GetThisThreadId();
    }

}

