#ifndef MEMORY_SUPP
#define MEMORY_SUPP

#include <memory>

using std::unique_ptr;

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}

#endif

