#pragma once
#include <mutex>

// Consumer always lock waits for memory
// Producer never locks, but try-lock swaps after locking
// From Consumer point of view, memory might be delayed/old
// From Producer point of view, memory is always available, but may not be able to swap after modify
template <typename T, typename Mut>
class PNL_CL_Memory
{
public:
    T& GetProducerMemory()
    {
        return memory[producerMemory];
    }

    void ReleaseProducerMemory()
    {
        // Swap if possible
        if (m_swapMutex.try_lock())
        {
            producerMemory = 1 - producerMemory;
            m_swapMutex.unlock();
        }
    }

    T& GetConsumerMemory()
    {
        //PROFILE_SCOPE_STR("LazyMem_Lock", PROFILE_COL_LOCK);
        m_swapMutex.lock();
        return memory[1 - producerMemory];
    }

    void ReleaseConsumerMemory()
    {
        m_swapMutex.unlock();
    };

private:
    Mut m_swapMutex;
    uint32_t producerMemory = 0;
    T memory[2];
};

template <typename T, typename Mut>
class ConsumerMemLock
{
public:
    ConsumerMemLock(PNL_CL_Memory<T, Mut>& lazyMem)
        : memory(lazyMem.GetConsumerMemory())
        , mem(lazyMem)
    {
    }
    ~ConsumerMemLock()
    {
        mem.ReleaseConsumerMemory();
    }

    T& Data() const
    {
        return memory;
    }

    T& memory;
    PNL_CL_Memory<T, Mut>& mem;
};

template <typename T, typename Mut>
class ProducerMemLock
{
public:
    ProducerMemLock(PNL_CL_Memory<T, Mut>& lazyMem)
        : memory(lazyMem.GetProducerMemory())
        , mem(lazyMem)
    {
    }
    ~ProducerMemLock()
    {
        mem.ReleaseProducerMemory();
    }

    T& Data() const
    {
        return memory;
    }

    T& memory;
    PNL_CL_Memory<T, Mut>& mem;
};

