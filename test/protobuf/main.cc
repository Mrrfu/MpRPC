#include "test.pb.h"
#include <iostream>
#include <string>

// protobuf的简单用法示例

using namespace fixbug;

int main()
{
    // // 封装了login请求对象的数据
    // LoginRequest req;
    // req.set_name("zhang san");
    // req.set_pwd("123456");
    // // 序列化
    // std::string send_str;
    // if (req.SerializeToString(&send_str))
    // {
    //     std::cout << send_str.c_str() << std::endl;
    // }

    // // 从sen_str反序列化一个login请求对象
    // LoginRequest reqB;
    // if (reqB.ParseFromString(send_str))
    // {
    //     std::cout << reqB.name() << std::endl;
    //     std::cout << reqB.pwd() << std::endl;
    // }

    LoginResponse rsp;
    // 获取LoginResponse中ResulCode成员的可写指针
    ResultCode *rc = rsp.mutable_result();
    rc->set_errcode(1);
    rc->set_errmsg("登录处理失败");

    GetFriendListResponse grsp;
    ResultCode *rc1 = grsp.mutable_result();
    rc1->set_errcode(0);
    // 向好友列表中添加一个新用户，并返回指针
    User *user1 = grsp.add_friend_list();
    user1->set_age(18);
    user1->set_name("zhang san");
    user1->set_sex(User::MAN);

    User *user2 = grsp.add_friend_list();
    user2->set_age(18);
    user2->set_name("zhang san");
    user2->set_sex(User::MAN);

    // 输出好友列表中用户的数量
    std::cout << grsp.friend_list_size() << std::endl;
    return 0;
}