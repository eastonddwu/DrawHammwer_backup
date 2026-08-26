/*
 * * file name: singleton.h
 * * description: 单例基类，强制在main之前完成实例化，避免多线程静态初始化竞争
 * */

#ifndef _APP_SINGLETON_H_
#define _APP_SINGLETON_H_

namespace app
{
template <typename T>
class Singleton
{
private:
    struct ObjectCreator
    {
        ObjectCreator() { Singleton<T>::GetInst(); }
        inline void DoNothing() const {}
    };
    static ObjectCreator create_object_;

protected:
    ~Singleton() = default;
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

public:
    static T& GetInst()
    {
        static T obj;
        create_object_.DoNothing();
        return obj;
    }
};

template <typename T>
typename Singleton<T>::ObjectCreator Singleton<T>::create_object_;

}  // namespace app

#endif
