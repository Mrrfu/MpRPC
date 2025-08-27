#include "ZkConnectionPool.h"
#include "mprpcapplication.h"
#include "logger.h"

// 删除器的实现：它的工作不是delete指针，而是将其包装成shared_ptr放回队列
void ZkConnectionPool::ZkClientDeleter::operator()(ZkClient *client) const // const是防止修改m_pool的指向
{
    if (m_pool)
    {
        std::lock_guard<std::mutex> guard(m_pool->m_queueMutex);
        // 关键：重新用这个指针和它自己的删除器创建一个新的shared_ptr并放回队列
        m_pool->m_connectionQueue.push(std::shared_ptr<ZkClient>(client, *this));
        m_pool->m_cv.notify_one();
    }
    else
    {
        // 如果池不存在，就销毁它，防止内存泄漏
        delete client;
    }
}

ZkConnectionPool *ZkConnectionPool::getInstance()
{
    static ZkConnectionPool pool;
    return &pool;
}

ZkConnectionPool::ZkConnectionPool()
{
    if (!loadConfig())
    {
        LOG_ERR("ZkConnectionPool: Failed to load configuration.");
        return;
    }
    init(m_initialSize);
}

bool ZkConnectionPool::loadConfig()
{
    std::string poolSizeStr = MprpcApplication::getInstance().getConfig().Load("zookeeperpoolsize");
    if (poolSizeStr.empty())
    {
        m_initialSize = 4;
        LOG_INFO("zookeeperpoolsize not configured, using default size: %d", m_initialSize);
    }
    else
    {
        m_initialSize = std::stoi(poolSizeStr);
    }
    return true;
}

// 在init中创建一个连接时，自动绑定好自定义删除器
void ZkConnectionPool::init(int initialSize)
{
    ZkClientDeleter deleter{this};
    for (int i = 0; i < initialSize; ++i)
    {
        // 1. 创建裸指针
        ZkClient *raw_client = new ZkClient();
        raw_client->Start();

        // 2. 用裸指针和自定义删除器创建shared_ptr
        m_connectionQueue.push(std::shared_ptr<ZkClient>(raw_client, deleter));
    }
}

std::shared_ptr<ZkClient> ZkConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);

    // 等待队列非空,避免虚假唤醒
    m_cv.wait(lock, [&]()
              { return !m_connectionQueue.empty(); });

    // 从队列中取出一个已经配置好deleter的shared_ptr，使用移动语义，避免拷贝操作导致外部无法删除
    std::shared_ptr<ZkClient> zk_client = std::move(m_connectionQueue.front());
    m_connectionQueue.pop();

    return zk_client;
}