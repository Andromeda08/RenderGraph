#pragma once

#include <atomic>
#include <memory>

class IdSequence
{
public:
    IdSequence(IdSequence const&)     = delete;
    void operator=(IdSequence const&) = delete;

    static IdSequence& get()
    {
        static IdSequence sInstance;
        return sInstance;
    }

    int32_t nextId()
    {
        return mLastId.fetch_add(1);
    }

    static int32_t next()
    {
        return get().nextId();
    }

private:
    IdSequence() = default;

    std::atomic_int32_t mLastId = 0;
};
