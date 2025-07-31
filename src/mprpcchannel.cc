#include <string>
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
#include "zookeeperuitl.h"

// 数据格式： [header_size (4字节)][数据头（service_name|method_name|args_size）][参数内容]
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                              google::protobuf::RpcController *controller,
                              const google::protobuf::Message *request,
                              google::protobuf::Message *response,
                              google::protobuf::Closure *done)
{
    const google::protobuf::ServiceDescriptor *sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name();

    // 获取参数的序列化字符串长度 args_size
    int args_size = 0;
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
    std::cout << "==========================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_size: " << args_size << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "==========================================" << std::endl;

    // 使用TCP编程，完成RPC方法的远程调用
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        char errText[512] = {0};
        sprintf(errText, "create socket error! errno: %d", errno);
        LOG_ERR("create socket error! errno: %d", errno);
        controller->SetFailed(errText);
        return;
    }

    // std::string ip = MprpcApplication::getInstance().getConfig().Load("rpcserverip");
    // uint16_t port = std::stoi((MprpcApplication::getInstance().getConfig().Load("rpcserverport")));
    // 在zk查找节点后获取远程服务ip和端口
    ZkClient zkCli;
    zkCli.Start();
    std::string method_path = "/" + service_name + "/" + method_name;
    std::string host_data = zkCli.GetData(method_path.c_str());
    if (host_data == "")
    {
        controller->SetFailed(method_path + " is not exist!");
        LOG_ERR("zookeeper: %s is not exists!", method_path.c_str());
        return;
    }

    int idx = host_data.find(":");
    if (idx == -1)
    {
        controller->SetFailed(method_path + " address is invalid!");
        LOG_ERR("zookeeper: in node %s , address is incalid!", method_path.c_str());
        return;
    }

    // 根据读取的数据：127.0.0.1:8088获取ip地址和端口
    std::string ip = host_data.substr(0, idx);
    uint16_t port = std::stoi(host_data.substr(idx + 1, host_data.size() - idx));
    // LOG_INFO("zookeeper: %s.%s found in path: %s:%d", service_name.c_str(), method_name.c_str(), ip.c_str(), port);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 发起连接
    if (connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        close(clientfd);
        char errText[512] = {0};
        sprintf(errText, "connect error! errno: %d", errno);
        LOG_ERR("connect rpc server error! errno: %d", errno);
        controller->SetFailed(errText);
        return;
    }
    // 发送rpc请求
    if (send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1)
    {
        close(clientfd);
        char errText[512] = {0};
        sprintf(errText, "send error! errno: %d", errno);
        LOG_ERR("send rpc error! errno: %d", errno);
        controller->SetFailed(errText);
        return;
    }

    // 阻塞接受rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = recv(clientfd, recv_buf, 1024, 0);
    if (recv_size == -1)
    {
        close(clientfd);
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
        close(clientfd);
        char errText[512] = {0};
        sprintf(errText, "parse response error! errno: %s", response_str.c_str());
        LOG_ERR("parse response error! errno: %s", response_str.c_str());
        controller->SetFailed(errText);
        return;
    }
    close(clientfd);
}
