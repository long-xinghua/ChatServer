#pragma once

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "message.pb.h"
#include "data.h"
#include <mutex>

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

// ������ChatServer����grpc���ӵķ����
class ChatGrpcServiceImpl final: public ChatService::Service	// final���������ټ̳������
{
public:
	ChatGrpcServiceImpl();
	// ���ѱ�������Ŀ���û� ���º�������
	Status NotifyAddFriend(grpc::ServerContext* context, const AddFriendReq* request,
		AddFriendRsp* reply) override;

	Status NotifyAuthFriend(::grpc::ServerContext* context,
		const AuthFriendReq* request, AuthFriendRsp* reply) override;

	Status NotifyTextChatMsg(::grpc::ServerContext* context,
		const TextChatMsgReq* request, TextChatMsgRsp* reply) override;

	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
};

