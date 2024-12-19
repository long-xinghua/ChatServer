#include "ChatGrpcServiceImpl.h"
#include "UserMgr.h"
#include "CSession.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "RedisMgr.h"
#include "MysqlMgr.h"

ChatGrpcServiceImpl::ChatGrpcServiceImpl() {
	//todo...
}
Status ChatGrpcServiceImpl::NotifyAddFriend(::grpc::ServerContext* context, const AddFriendReq* request, AddFriendRsp* reply) {
	//todo...
	return Status::OK;
}

Status ChatGrpcServiceImpl::NotifyAuthFriend(::grpc::ServerContext* context, const AuthFriendReq* request, AuthFriendRsp* response) {
	//todo...
	return Status::OK;
}

Status ChatGrpcServiceImpl::NotifyTextChatMsg(::grpc::ServerContext* context, const TextChatMsgReq* request, TextChatMsgRsp* response) {
	//todo...
	return Status::OK;
}

bool ChatGrpcServiceImpl::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo) {
	//todo...
	return true;
}


