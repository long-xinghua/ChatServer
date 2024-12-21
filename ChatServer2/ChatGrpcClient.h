#pragma once
#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include <grpcpp/grpcpp.h> 
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <queue>
#include "data.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "CSession.h"


//#include <unordered_map>

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;

using message::GetChatServerRsp;
using message::LoginRsp;
using message::LoginReq;
using message::ChatService;


// 用连接池存储多个连接
class ChatConPool {
public:
	ChatConPool(size_t size, std::string host, std::string port);
	~ChatConPool();
	std::unique_ptr<ChatService::Stub> getConnection();				// 获取连接
	bool returnConnection(std::unique_ptr<ChatService::Stub> stub);	// 归还连接
	void close();													// 关闭连接池
private:
	size_t _poolSize;
	std::string _host;
	std::string _port;
	std::atomic<bool> _b_stop;	// 标志连接池是否关闭
	std::queue<std::unique_ptr<ChatService::Stub>> _connections;	// ChatService为聊天服务器之间的通信服务
	std::mutex _mutex;
	std::condition_variable _cond;
};

// 与其他ChatServer进行grpc连接的客户端
class ChatGrpcClient: public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;
public:
	~ChatGrpcClient();
	AddFriendRsp NotifyAddFriend(std::string server_ip, const AddFriendReq& req);			// 通知对方有新的好友申请请求
	AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req);		// 通知对方认证
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userInfo);
	TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue);
private:
	ChatGrpcClient();
	std::unordered_map<std::string, std::unique_ptr<ChatConPool>> _pools;	// key为其他ChatServer名字，对每个ChatServer都创建一个连接池
};

