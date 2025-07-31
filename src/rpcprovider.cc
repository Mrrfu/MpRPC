#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperuitl.h"

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
        // 记录每个方法的注册
        LOG_INFO("Register service: [%s] method: [%s]", service_name.c_str(), method_name.c_str());
        service_info.m_methodMap.insert({method_name, pmethodDesc});
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
    // 记录服务注册完成
    LOG_INFO("Service [%s] registered successfully.", service_name.c_str());
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

    // 把当前rpc节点上发布的服务全部注册到zookeeper，使rpc client可以在zookeeper上发现服务
    ZkClient zkCli;
    zkCli.Start();
    // 使service_name为永久性节点 method_name为临时性节点
    for (auto &sp : m_serviceMap)
    {
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        LOG_INFO("zookeeper: created persistent node: %s", service_path.c_str());
        for (auto &mp : sp.second.m_methodMap)
        {
            // /service_name/method_name
            std::string method_path = service_path + "/" + mp.first;
            // 节点存储的数据为当前rpc服务主机的ip和端口
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            // ZOO_EPHEMERAL表示是一个临时性节点
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
            LOG_INFO("zookeeper: created ephemeral node: %s, data: %s", method_path.c_str(), method_path_data);
        }
    }

    std::cout << "RpcProvider start service at ip : " << ip << " port:" << port << std::endl;
    LOG_INFO("RpcProvider start service at ip: %s port: %d", ip.c_str(), port);
    // 启动网络服务
    server.start();
    m_eventLoop.loop();
}

// 连接回调
void RpcProvider::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        // 和rpc client的连接断开了
        conn->shutdown();
    }
}
/*
 * 在框架内部，RpcProvider和RpcConsumer需要协商好之间通信所用的protobuf数据类型
 * service_name method_name args
 * 定义proto的message类型，进行数据的序列化和反序列化
 * header_size（4个字节） +header_str+ args_str (解决TCP粘包问题)
 * 协议格式：
 * 前4字节：数据头长度
 * 数据头内容：服务名、方法名、参数长度
 * 最后：参数内容
 * [header_size (4字节)][数据头（service_name|method_name|args_size）][参数内容]
 */
// 已建立连接用户的读写事件回调，如果远程有一个rpc服务的调用请求，onMessage就会响应
void RpcProvider::onMessage(const muduo::net::TcpConnectionPtr &conn,
                            muduo::net::Buffer *buffer,
                            muduo::Timestamp)
{
    // 网络上接收的远程rpc调用请求的字符流
    std::string recv_buf = buffer->retrieveAllAsString();

    // 从字符流中读取前4个字节的内容
    uint32_t header_size = 0;
    recv_buf.copy((char *)&header_size, 4, 0);

    // 根据head_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
        LOG_INFO("Received RPC call: service=[%s], method=[%s], args_size=%u", service_name.c_str(), method_name.c_str(), args_size);
    }
    else
    {
        LOG_ERR("rpc_header_str parse error! raw: %s", rpc_header_str.c_str());
        return;
    }
    // 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 打印调试信息
    std::cout << "==========================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_size: " << args_size << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "==========================================" << std::endl;

    // 查找service对象
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end())
    {
        LOG_ERR("Service [%s] not exist!", service_name.c_str());
        return;
    }

    // 获查找method对象
    auto m_it = it->second.m_methodMap.find(method_name);
    if (m_it == it->second.m_methodMap.end())
    {
        LOG_ERR("Service [%s] method [%s] not exist!", service_name.c_str(), method_name.c_str());
        return;
    }

    google::protobuf::Service *service = it->second.m_service;       // 获取service对象
    const google::protobuf::MethodDescriptor *method = m_it->second; // 获取method对象

    // 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        LOG_ERR("Request parse error! service=[%s], method=[%s], content=[%s]", service_name.c_str(), method_name.c_str(), args_str.c_str());
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给method方法绑定一个Closure回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr &, google::protobuf::Message *>(
        this, &RpcProvider::SendRpcResponse, conn, response);

    // 在框架上根据远程rpc请求，调用当前rpc节点上发布的方法（子类通过继承基类的CallMethod）
    service->CallMethod(method, nullptr, request, response, done);
}

// 本地方法执行结束后的回调函数
// 1.序列化response
// 2.利用muduo发送给调用方
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str))
    {
        // 序列化成功，通过网络把rpc方法执行的结果发送回调用方
        conn->send(response_str);
        LOG_INFO("Send response to client success. size=%zu", response_str.size());
    }
    else
    {
        LOG_ERR("Serialize response_str error, content: %s", response_str.c_str());
    }
    conn->shutdown(); // 模拟http的短连接，由rpcprovider主动断开连接
}