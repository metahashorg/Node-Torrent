#ifndef REFERENCE_WRAPPER_H_
#define REFERENCE_WRAPPER_H_

#include "safe_ptr.h"

#include <memory>
#include <shared_mutex>

#include "OopUtils.h"

namespace torrent_node_lib {

class DestroyedException {
    
};

template<class T>
class ReferenseWrapperMaster;

template<class T>
class ReferenceWrapperPtr {
    template<class T2> friend class ReferenseWrapperMaster;
public:
    
    ReferenceWrapperPtr(T &&t)
        : t(std::move(t))
    {}
    
public:
       
    const T* operator->() const {
        if (isDestroyed) {
            throw DestroyedException();
        }
        return &t;
    }
    
    const T* operator*() const {
        if (isDestroyed) {
            throw DestroyedException();
        }
        return &t;
    }
    
    const T* get() const {
        if (isDestroyed) {
            throw DestroyedException();
        }
        return &t;
    }
    
protected:
    
    T& operator->() {
        return t;
    }
    
    T& operator*() {
        return t;
    }
    
    void destroy() {
        isDestroyed = true;
    }
    
private:
    
    bool isDestroyed = false;
    T t;
};

template<class T>
using safe_ptr = sf::safe_ptr<ReferenceWrapperPtr<T>, std::shared_mutex, std::unique_lock<std::shared_mutex>, std::shared_lock<std::shared_mutex>>;

template<class T>
class ReferenseWrapperSlave {
public:
    
    ReferenseWrapperSlave(const safe_ptr<T> &ptr)
        : ptr(ptr)
    {}
    
    const safe_ptr<T>& operator->() const {
        return ptr;
    }
    
    const safe_ptr<T>& operator*() const {
        return ptr;
    }
    
private:
    
    const safe_ptr<T> ptr;
};

template<class T>
class ReferenseWrapperMaster: public common::no_copyable, public common::no_moveable {
public:
    
    ReferenseWrapperMaster(T &&t) 
        : ptr(safe_ptr<T>::make_safe(std::move(t)))
    {}
    
    ~ReferenseWrapperMaster() {
        ptr->destroy();
    }
    
public:
    
    void destroy() {
        ptr->destroy();
    }
    
    ReferenseWrapperSlave<T> makeSlave() {
        return ReferenseWrapperSlave(ptr);
    }
    
private:
    
    safe_ptr<T> ptr;
};

} // namespace torrent_node_lib

#endif // REFERENCE_WRAPPER_H_
