#include <string>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "mprpcchannel.h"
#include "mprpccontroller.h"
#include "rpcheader.pb.h"
#include "mprpcapplication.h"
#include "logger.h"
#include "ZkConnectionPool.h"
#include <memory>

std::mutex g_data_mutex;

MprpcChannel::MprpcChannel() : m_clientFd(-1)
{
}
MprpcChannel::~MprpcChannel()
{
    if (m_clientFd >= 0)
    {
        close(m_clientFd);
    }
}
// 数据格式： [header_size (4字节)][数据头（service_name|method_name|args_size）][参数内容]
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                              google::protobuf::RpcController *controller,
                              const google::protobuf::Message *request,
                              google::protobuf::Message *response,
                              google::protobuf::Closure *done)
{
    if (m_clientFd == -1) // 客户端未初始化
    {
        const google::protobuf::ServiceDescriptor *sd = method->service();
        service_name = sd->name();
        method_name = method->name();

        // 在zk查找节点后获取远程服务ip和端口

        // 不使用zookeeper连接池
        // ZkClient *zkCli = new ZkClient();
        // zkCli->Start();
        // std::shared_ptr<ZkClient> cli(zkCli);
        auto cli = ZkConnectionPool::getInstance()->getConnection(); // 使用zookeeper连接池

        int idx = 0;
        std::string host_data = queryServiceHost(cli, service_name, method_name, idx);

        if (host_data == " ")
        {
            LOG_ERR("Method %s not found under service %s!", service_name, method_name);
            char errText[256] = {0};
            sprintf(errText, "Method %s not found under service %s!", method_name.c_str(), service_name.c_str());
            std::string errMsg(errText);
            controller->SetFailed(errMsg);
            return;
        }

        // 根据读取的数据：127.0.0.1:8088 获取ip地址和端口
        m_ip = host_data.substr(0, idx);
        m_port = std::stoi(host_data.substr(idx + 1, host_data.size() - idx));

        // 尝试连接服务器
        std::string errMsg;
        auto rt = newConnect(m_ip.c_str(), m_port, &errMsg);
        int count = 2;
        // 如果失败则发起重连
        while (!rt && count--)
        {
            auto rt = newConnect(m_ip.c_str(), m_port, &errMsg);
        }
        // 记录失败原因
        if (!rt)
        {
            LOG_ERR("connect server error, failed to reconnect after 3 attempts!");
            controller->SetFailed("connect server error, failed to reconnect after 3 attempts!");
        }
        else
        {
            LOG_INFO("connect server success! ip: %s, port: %d", m_ip, m_port);
        }
    }

    // 获取参数的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        LOG_ERR("serialize request error!-> %s:%s:%d", __FILE__, __FUNCTION__, __LINE__);
        controller->SetFailed("serialize request error!");
        return;
    }

    // 定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        LOG_ERR("serialize rpc header error! rpc_header_str: %s in %s:%s:%d", rpc_header_str.c_str(), __FILE__, __FUNCTION__, __LINE__);
        controller->SetFailed("serialize rpc header error! ");
        return;
    }

    // 组织待发送的rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char *)&header_size, 4));
    send_rpc_str += rpc_header_str;
    send_rpc_str += args_str;

    // 打印调试信息
    // std::cout << "==========================================" << std::endl;
    // std::cout << "header_size: " << header_size << std::endl;
    // std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    // std::cout << "service_name: " << service_name << std::endl;
    // std::cout << "method_name: " << method_name << std::endl;
    // std::cout << "args_size: " << args_size << std::endl;
    // std::cout << "args_str: " << args_str << std::endl;
    // std::cout << "==========================================" << std::endl;

    // std::string ip = MprpcApplication::getInstance().getConfig().Load("rpcserverip");
    // uint16_t port = std::stoi((MprpcApplication::getInstance().getConfig().Load("rpcserverport")));

    // 发送rpc请求
    if (send(m_clientFd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1)
    {
        close(m_clientFd);
        char errText[512] = {0};
        sprintf(errText, "send error! errno: %d", errno);
        LOG_ERR("send rpc error! errno: %d", errno);
        controller->SetFailed(errText);
        return;
    }

    // 阻塞接受rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = recv(m_clientFd, recv_buf, 1024, 0);
    if (recv_size == -1)
    {
        close(m_clientFd);
        char errText[512] = {0};
        sprintf(errText, "recv error! errno: %d", errno);
        LOG_ERR("recv rpc response error! errno: %d", errno);
        controller->SetFailed(errText);
        return;
    }
    // std::string response_str(recv_buf,0,recv_size); // bug,recv_buf中遇到\0后面的数据旧无法存取，导致反序列化失败(因为被\0截断)
    std::string response_str(recv_buf, recv_size); // 利用前recv_size字节构造字符串，即时有\0也不会被截断
    // 数据反序列化
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(m_clientFd);
        char errText[512] = {0};
        sprintf(errText, "parse response error! errno: %s", response_str.c_str());
        LOG_ERR("parse response error! errno: %s", response_str.c_str());
        controller->SetFailed(errText);
        return;
    }
    // close(m_clientFd);
}

bool MprpcChannel::newConnect(const char *ip, uint16_t port, std::string *errMsg)
{
    // 使用TCP编程，完成RPC方法的远程调用
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        char errText[512] = {0};
        sprintf(errText, "create socket error! errno: %d", errno);
        LOG_ERR("create socket error! errno: %d", errno);
        return false;
    }
    // 设置服务器地址信息
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    // 发起连接
    if (connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        close(clientfd);
        char errText[512] = {0};
        sprintf(errText, "connect error! errno: %d", errno);
        LOG_ERR("connect rpc server error! errno: %d", errno);
        return false;
    }
    m_clientFd = clientfd;
    return true;
}

std::string MprpcChannel::queryServiceHost(std::shared_ptr<ZkClient> &zkclient, const std::string &service_name, const std::string &methdo_name, int &idx)
{
    std::string method_path = "/" + service_name + "/" + method_name;
    // std::unique_lock<std::mutex> lock(g_data_mutex); // 加锁，保证线程安全
    std::string host_data = zkclient->GetData(method_path.c_str());
    if (host_data == "")
    {
        LOG_ERR("zookeeper: %s is not exists!", method_path.c_str());
        return " ";
    }

    idx = host_data.find(":");
    if (idx == -1)
    {
        LOG_ERR("zookeeper: in node %s , address is incalid!", method_path.c_str());
        return " ";
    }
    return host_data;
}