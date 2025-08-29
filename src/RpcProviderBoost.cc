#include "RpcProviderBoost.h"
#include "RpcSession.h" // 引入会话类
#include "mprpcapplication.h"
#include "logger.h"
#include "zookeeperuitl.h"
#include <thread>
#include <vector>

RpcProviderBoost::RpcProviderBoost()
    // 初始化列表必须初始化acceptor
    : m_acceptor(m_io_context)
{
}

// 注册服务
void RpcProviderBoost::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    std::string service_name = pserviceDesc->name();
    int methodCnt = pserviceDesc->method_count();

    for (int i = 0; i < methodCnt; ++i)
    {
        const google::protobuf::MethodDescriptor *pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        LOG_INFO("Register service: [%s] method: [%s]", service_name.c_str(), method_name.c_str());
        service_info.m_methodMap.insert({method_name, pmethodDesc});
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
    LOG_INFO("Service [%s] registered successfully.", service_name.c_str());
}

void RpcProviderBoost::Run()
{
    // 1. 加载配置
    std::string ip = MprpcApplication::getInstance().getConfig().Load("rpcserverip");
    uint16_t port = std::stoi(MprpcApplication::getInstance().getConfig().Load("rpcserverport"));
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(ip), port);

    // 2. 配置并启动acceptor
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    m_acceptor.bind(endpoint); // 绑定ip和端口
    m_acceptor.listen();       // 监听

    LOG_INFO("RpcProvider starting acceptor at %s:%d", ip.c_str(), port);
    std::cout << "RpcProvider starting acceptor at " << ip << ":" << port << std::endl;

    // 3. Zookeeper服务注册
    ZkClient zkCli;
    zkCli.Start();
    for (auto &sp : m_serviceMap)
    {
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        LOG_INFO("zookeeper: created persistent node: %s", service_path.c_str());
        for (auto &mp : sp.second.m_methodMap)
        {
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
            LOG_INFO("zookeeper: created ephemeral node: %s, data: %s", method_path.c_str(), method_path_data);
        }
    }

    // 4. 开始接受连接
    do_accept();

    // 5. 创建线程池，运行io_context事件循环
    int thread_num = 4; // 同样可以从配置中读取
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_num; ++i)
    {
        threads.emplace_back([this]()
                             { m_io_context.run(); });
    }
    LOG_INFO("RpcProvider service running in %d threads.", thread_num);

    // 主线程也加入事件循环
    // io_context.run()会阻塞，直到所有工作完成
    m_io_context.run();

    // 等待所有工作线程结束
    for (auto &th : threads)
    {
        th.join();
    }
}

// 处理接受连接函数
void RpcProviderBoost::do_accept()
{
    m_acceptor.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
        {
            if (!ec)
            {
                // 接受连接成功，创建一个新的Session来处理它
                std::make_shared<RpcSession>(std::move(socket), this)->start();
            }
            else
            {
                LOG_ERR("RpcProvider: accept error. %s", ec.message().c_str());
            }

            // 无论本次是否成功，都继续监听下一个连接，递归调用
            do_accept();
        });
}

// 处理请求
void RpcProviderBoost::dispatch_request(const mprpc::RpcHeader &header,
                                        const std::string &args_str,
                                        std::shared_ptr<RpcSession> session)
{
    std::string service_name = header.service_name();
    std::string method_name = header.method_name();

    LOG_INFO("Received RPC call: service=[%s], method=[%s], args_size=%u", service_name.c_str(), method_name.c_str(), header.args_size());

    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end())
    {
        LOG_ERR("Service [%s] not exist!", service_name.c_str());
        return;
    }
    auto m_it = it->second.m_methodMap.find(method_name);
    if (m_it == it->second.m_methodMap.end())
    {
        LOG_ERR("Service [%s] method [%s] not exist!", service_name.c_str(), method_name.c_str());
        return;
    }

    google::protobuf::Service *service = it->second.m_service;
    const google::protobuf::MethodDescriptor *method = m_it->second;

    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        LOG_ERR("Request parse error!");
        delete request;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // Closure的回调函数现在是session的成员函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcSession, google::protobuf::Message *>(
        session.get(),
        &RpcSession::send_rpc_response,
        response);

    // 调用本地方法
    service->CallMethod(method, nullptr, request, response, done);

    // request Message在这里可以安全删除了，因为它已经被CallMethod处理
    delete request;
}