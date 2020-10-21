#ifndef __BASE_H__
#define __BASE_H__

#include <cstdint>

typedef int64_t _key_t;
typedef int64_t _value_t;

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/time.h>
#endif


// OS-specific timing
static double seconds()
{
#ifdef _WIN32
    LARGE_INTEGER frequency, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return now.QuadPart / double(frequency.QuadPart);
#else
    timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + now.tv_usec/1000000.0;
#endif
}

static double get_seed() {
#ifdef _WIN32
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
#else
    timeval now;
    gettimeofday(&now, NULL);
    return now.tv_usec;
#endif
}

class tree_api {
/*
    all the keys are 8-bytes integer
    the value is seperately stored to its key
*/
public:
    virtual ~tree_api(){};

    virtual bool find(_key_t key, _value_t & value) = 0;

    virtual void insert(_key_t key, _value_t value) = 0;

    virtual bool update(_key_t key, _value_t value) = 0;

    virtual bool remove(_key_t key) = 0;

    virtual void printAll() = 0;
};

#endif //__BASE_H__