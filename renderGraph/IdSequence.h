#pragma once

#include <cstdint>

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

    static int32_t next()
    {
        return get().nextId();
    }

private:
    IdSequence() = default;

    int32_t nextId()
    {
        return mLastId++;
    }

    int32_t mLastId = 0;
};
