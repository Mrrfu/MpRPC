#include "rpcprovider.h"
#include "mprpcapplication.h"

/*
 * service_name --> service描述


*/

// 注册一个protobuf服务对象
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;

    // 获取服务对象的描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = pserviceDesc->name();
    // std::cout << "service_name: " << service_name << std::endl;

    // 获取服务对象service的方法的数量
    int methodCnt = pserviceDesc->method_count();
    for (int i = 0; i < methodCnt; ++i)
    {
        // 获取服务对象指定下标的服务方法的描述（抽象描述）
        const google::protobuf::MethodDescriptor *pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        // std::cout << "method_name: " << method_name << std::endl;
        service_info.m_methodMap.insert({method_name, pmethodDesc});
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

void RpcProvider::Run()
{
    // std::unique_ptr<muduo::net::TcpServer> m_tcpserverPtr;
    std::string ip = MprpcApplication::getInstance().getConfig().Load("rpcserverip");
    uint16_t port = std::stoi((MprpcApplication::getInstance().getConfig().Load("rpcserverport")));
    muduo::net::InetAddress address(ip, port);

    // 创建tcpserver
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    // 绑定连接回调和消息读写回调
    server.setConnectionCallback(std::bind(&RpcProvider::onConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server.setThreadNum(4);
    std::cout << "RpcProvider start service at ip : " << ip << std::endl;
    // 启动网络服务
    server.start();
    m_eventLoop.loop();
}

// 连接回调
void RpcProvider::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
}

void RpcProvider::onMessage(const muduo::net::TcpConnectionPtr &,
                            muduo::net::Buffer *,
                            muduo::Timestamp)
{
}