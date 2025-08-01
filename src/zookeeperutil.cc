#include "zookeeperuitl.h"
#include "mprpcapplication.h"
#include <iostream>
#include <mutex>
#include <condition_variable>
#include "logger.h"
#include <memory>

/*
 * 这里不能使用全局变量！
 * 多线程共享同一组全局变量，出现竞态，导致状态混乱，部分线程会误以为连接已建立，实际还未建立
 * 虚假唤醒：条件变量cv被所有线程共享，notify_all()会唤醒所有等待线程，
 * 但是只有一个线程的连接真正建立，其他线程被唤醒后检查is_connecyed，
 * 发现设置为true，就会结束等待，但是它们的连接还未建立。
 */

// std::mutex cv_mutex;
// std::condition_variable cv;
// bool is_connected = false;

// 连接上下文
struct InitContex
{
    std::condition_variable *cv;
    std::mutex *cv_mutex;
    bool *is_connected;
    InitContex(std::condition_variable *cv_, std::mutex *cv_mutex_, bool *is_connected_)
        : cv(cv_), cv_mutex(cv_mutex_), is_connected(is_connected_) {}
};

// 生成超时时间 默认10s
void get_timeout_timespec(struct timespec &ts, int timeout_sec = 10)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    ts.tv_sec = now.tv_sec + timeout_sec;
    ts.tv_nsec = now.tv_nsec;
}

// 全局的watchewr观察器，zkserver给skclient的通知
void global_watcher(zhandle_t *zh, int type, int state,
                    const char *path, void *watcherCtx)
{
    auto *ctx = static_cast<InitContex *>(watcherCtx);
    if (type == ZOO_SESSION_EVENT)
    {
        if (state == ZOO_CONNECTED_STATE) // 连接成功
        {
            // sem_t *sem = (sem_t *)zoo_get_context(zh);
            // sem_post(sem);
            // std::lock_guard<std::mutex> lock(cv_mutex);
            // is_connected = true;

            std::lock_guard<std::mutex> lock(*(ctx->cv_mutex));
            *(ctx->is_connected) = true;
            ctx->cv->notify_all();
        }
        else
        {
            LOG_ERR("zookeeper: connect error! %d", state);
        }
    }
    // cv.notify_all(); // 通知所有等待线程
}

ZkClient::ZkClient() : m_zhandle(nullptr) {}

ZkClient::~ZkClient()
{
    if (m_zhandle != nullptr)
    {
        zookeeper_close(m_zhandle);
    }
}

void ZkClient::Start()
{
    std::string host = MprpcApplication::getInstance().getConfig().Load("zookeeperip");
    std::string port = MprpcApplication::getInstance().getConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port;
    /*
     *zookeeper_init是异步的，当前返回不能说明初始化成功
     */
    std::condition_variable cv;
    std::mutex cv_mutex;
    bool is_connected = false;

    // InitContex ctx{&cv, &cv_mutex, &is_connected};
    InitContex ctx{&cv, &cv_mutex, &is_connected};
    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, &ctx, 0);
    if (m_zhandle == nullptr)
    {
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE);
    }
    // 下面是使用信号量，但是高并发的情况下有概率出现连接失败
    // sem_t sem;
    // sem_init(&sem, 0, 0);
    // zoo_set_context(m_zhandle, &sem);
    // struct timespec ts;
    // get_timeout_timespec(ts);
    // if (sem_timedwait(&sem, &ts) != 0)
    // {
    //     std::cerr << "zookeeper connect timeout" << std::endl;
    //     sem_destroy(&sem);
    //     exit(EXIT_FAILURE);
    // }
    // sem_destroy(&sem);

    // 改为C++11的条件变量
    std::unique_lock<std::mutex> lock(cv_mutex);
    if (!cv.wait_for(lock, std::chrono::seconds(10), [&]
                     { return is_connected; }))
    {
        std::cerr << "zookeeper connect timeout (cv.wait_for)" << std::endl;
        exit(EXIT_FAILURE);
    }
    LOG_INFO("zookeeper_init success!");
    std::cout << "zookeeper_init success!" << std::endl;
}

void ZkClient::Create(const char *path, const char *data, int datalen, int state)
{
    struct CallbackContext
    {
        int rc;    // 异步线程返回值
        sem_t sem; // 异步线程同步 信号量
    } exists_ctx, create_ctx;

    // 先判断path表示的znode节点是否存在，如果不存在再创建
    sem_init(&exists_ctx.sem, 0, 0);
    exists_ctx.rc = ZSYSTEMERROR;

    zoo_aexists(m_zhandle, path, 0, [](int rc, const struct Stat *, const void *data)
                {
                    auto *ctx = (CallbackContext *)data;
                    ctx->rc = rc;
                    sem_post(&ctx->sem); }, &exists_ctx);

    struct timespec ts;
    get_timeout_timespec(ts);
    if (sem_timedwait(&exists_ctx.sem, &ts) != 0)
    {
        std::cerr << "zoo_aexists timeout for path: " << path << std::endl;
        sem_destroy(&exists_ctx.sem);
        return;
    }
    sem_destroy(&exists_ctx.sem);

    if (exists_ctx.rc == ZNONODE)
    {
        // 创建节点
        sem_init(&create_ctx.sem, 0, 0);
        create_ctx.rc = ZSYSTEMERROR;

        zoo_acreate(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, [](int rc, const char *value, const void *data)
                    {
                        auto *ctx = (CallbackContext *)data;
                        ctx->rc = rc;
                        sem_post(&ctx->sem); }, &create_ctx);

        get_timeout_timespec(ts);
        if (sem_timedwait(&create_ctx.sem, &ts) != 0)
        {
            std::cerr << "zoo_acreate timeout for path: " << path << std::endl;
            sem_destroy(&create_ctx.sem);
            return;
        }

        sem_destroy(&create_ctx.sem);

        if (create_ctx.rc == ZOK)
        {
            std::cout << "znode created: " << path << std::endl;
        }
        else
        {
            std::cerr << "znode create failed: " << path << ", error: " << create_ctx.rc << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    else if (exists_ctx.rc == ZOK)
    {
        std::cout << "znode exists: " << path << std::endl;
        std::string val = GetData(path);
        std::cout << "data: " << val << std::endl;
    }
    else
    {
        std::cerr << "zoo_aexists failed: " << path << ", error: " << exists_ctx.rc << std::endl;
    }
}

std::string ZkClient::GetData(const char *path)
{
    struct GetDataContext
    {
        char *buffer;
        int rc;
        sem_t sem;
    };

    char buf[64] = {0};
    GetDataContext ctx{buf, ZSYSTEMERROR};
    sem_init(&ctx.sem, 0, 0);

    zoo_aget(m_zhandle, path, 0, [](int rc, const char *value, int value_len, const struct Stat *, const void *data)
             {
                 auto *ctx = (GetDataContext *)data;
                 ctx->rc = rc;
                 if (rc == ZOK && value != nullptr)
                 {
                     int len = (value_len < 63) ? value_len : 63;
                     memcpy(ctx->buffer, value, len);
                     ctx->buffer[len] = '\0';
                 }
                 sem_post(&ctx->sem); }, &ctx);

    struct timespec ts;
    get_timeout_timespec(ts);
    if (sem_timedwait(&ctx.sem, &ts) != 0)
    {
        std::cerr << "get znode data timeout: " << path << std::endl;
        LOG_ERR("get znode data timeout: %s", path);
        sem_destroy(&ctx.sem);
        return "";
    }

    sem_destroy(&ctx.sem);

    if (ctx.rc != ZOK)
    {
        std::cerr << "get znode data error: " << path << ", rc=" << ctx.rc << std::endl;
        LOG_ERR("get znode data error: %s, rc=%d", path, ctx.rc);
        return "";
    }

    return std::string(buf);
}