#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
#include "logger.h"

/*
    UserService是一个本地服务，提供了两个进程内的本地方法，Login和GetFriendLists
*/
class UserService : public fixbug::UserServiceRpc // 使用在rpc服务发布端（rpc服务提供者）（继承至UserServiceRpc,UserServiceRpc继承至Service）
{
public:
    bool Login(const std::string &name, const std::string &pwd)
    {
        std::cout << " doing local service Login" << std::endl;
        std::cout << "name: " << name << " pwd: " << pwd << std::endl;
        return false;
    }

    bool Register(uint32_t id, const std::string &name, const std::string &pwd)
    {
        std::cout << " doing local service Register" << std::endl;
        std::cout << "id: " << id << " name: " << name << " pwd: " << pwd << std::endl;
        return true;
    }
    /*
     * 重写基类UserServiceRpc的虚函数
     * 1.caller -->Login(LoginRequest) -->muduo -->callee
     * 2.callee -->Login(LoginRequest) -->重写的Login方法
     */
    void Login(::google::protobuf::RpcController *controller,
               const ::fixbug::LoginRequest *request,
               ::fixbug::LoginResponse *response,
               ::google::protobuf::Closure *done)
    {
        // 框架给业务上报请求参数LoginRequest，业务获取相应数据做本地业务
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 本地业务
        bool login_result = Login(name, pwd);

        // 写入响应
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_success(login_result);

        // 执行回调操作，执行响应对象数据的序列化和网络发送（由框架完成）
        done->Run();
    }

    void Register(::google::protobuf::RpcController *controller,
                  const ::fixbug::RegisterRequest *request,
                  ::fixbug::RegisterResponse *response,
                  ::google::protobuf::Closure *done)
    {
        uint32_t id = request->id();
        std::string name = request->name();
        std::string pwd = request->pwd();
        // 调用本地的注册方法
        bool register_result = Register(id, name, pwd);
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_success(register_result);
        //执行回调，将结果序列化和通过网络发送
        done->Run();
    }
};

int main(int argc, char **argv)
{
    MprpcApplication::Init(argc, argv);
    // 把UserService对象发布到rpc节点
    RpcProvider provider;
    provider.NotifyService(new UserService());

    // 启动一个rpc服务发布节点,Run以后，进程进入阻塞状态，等待远程调用
    provider.Run();

    return 0;
}
