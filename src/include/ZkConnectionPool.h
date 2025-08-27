#pragma once

#include "zookeeperuitl.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>

// zookeeper连接池
/*
 * zookeeper连接池，使用单例模式，全局只有一个对象
 * 连接池使用队列作为存储已连接对象的容器
 * 使用共享智能指针管理连接，当引用计数为0（即外部不再使用）时使用自定义的删除器重新添加至队列中，表示回到连接池
 */
class ZkConnectionPool
{
public:
    static ZkConnectionPool *getInstance();
    std::shared_ptr<ZkClient> getConnection();

private:
    ZkConnectionPool();
    ZkConnectionPool(const ZkConnectionPool &) = delete;
    ZkConnectionPool &operator=(const ZkConnectionPool &) = delete;

    bool loadConfig();
    void init(int initialSize);

    // 定义一个可复用的删除器（限定名称空间在连接池内）
    struct ZkClientDeleter
    {
        ZkConnectionPool *m_pool;
        void operator()(ZkClient *client) const;
    };

    int m_initialSize;
    std::queue<std::shared_ptr<ZkClient>> m_connectionQueue; // 定义时不需要声明删除器类型，在构造对象时将删除器实例作为参数传入
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
};