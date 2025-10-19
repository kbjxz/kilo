#ifndef ERR_H
#define ERR_H

#include <stdio.h>
#include <stdlib.h>
#include <concepts>

inline void panic(const char* s)
{
    perror(s);
    exit(1);
}

inline void panic(void)
{
    exit(1);
}

template <typename F>
requires std::is_invocable_v<F>
void panic(const char* s, F cleanup)
{
    cleanup();
    perror(s);
    exit(1);
}

struct error {
    int err_no;
    const char* msg;
};

template <typename T>
struct maybe {
    T value;
    bool ok;
    
    operator bool()
    {
        return ok; 
    }
    T operator() ()
    {
        return value;
    }
};

template <typename T>
maybe<T> some(T v)
{
    return maybe<T>{.value = v, .ok = true};
}

template <typename T>
maybe<T> none()
{
    return maybe<T>{.value = {}, .ok = false};
}

template <typename T, typename Err = error>
struct result {
    maybe<Err> merr;
    T value;
    
    operator bool ()
    {
        return !merr;
    }
};

template <typename T, typename Err = error>
result<T, Err> result_err(Err e)
{
    return result<T, Err>{.merr = some(e), .value ={}};
}

template <typename T, typename Err = error>
result<T, Err> result_v(T v)
{
    return result<T, Err>{.merr = none<Err>(), .value =v};
}

template <typename _, typename Err>
bool result_is_err(const result<_, Err>* r)
{
    return r->merr.ok;
}

template <typename _, typename Err>
Err result_unwrap_err(const result<_, Err>* r)
{
    return r->merr.value;
}

#endif