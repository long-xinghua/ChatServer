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


// �����ӳش洢�������
class ChatConPool {
public:
	ChatConPool(size_t size, std::string host, std::string port);
	~ChatConPool();
	std::unique_ptr<ChatService::Stub> getConnection();				// ��ȡ����
	bool returnConnection(std::unique_ptr<ChatService::Stub> stub);	// �黹����
	void close();													// �ر����ӳ�
private:
	size_t _poolSize;
	std::string _host;
	std::string _port;
	std::atomic<bool> _b_stop;	// ��־���ӳ��Ƿ�ر�
	std::queue<std::unique_ptr<ChatService::Stub>> _connections;	// ChatServiceΪ���������֮���ͨ�ŷ���
	std::mutex _mutex;
	std::condition_variable _cond;
};

// ������ChatServer����grpc���ӵĿͻ���
class ChatGrpcClient: public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;
public:
	~ChatGrpcClient();
	AddFriendRsp NotifyAddFriend(std::string server_ip, const AddFriendReq& req);			// ֪ͨ�Է����µĺ�����������
	AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req);		// ֪ͨ�Է���֤
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userInfo);
	TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue);
private:
	ChatGrpcClient();
	std::unordered_map<std::string, std::unique_ptr<ChatConPool>> _pools;	// keyΪ����ChatServer���֣���ÿ��ChatServer������һ�����ӳ�
};

