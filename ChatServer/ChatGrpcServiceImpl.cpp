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
	std::cout << "receiving NotifyAddFriend request from another ChatServer" << std::endl;
	auto fromUid = request->applyuid();
	auto toUid = request->touid();
	reply->set_applyuid(fromUid);
	reply->set_error(ErrorCodes::Success);
	reply->set_touid(toUid);

	// �Ȳ����û��Ƿ�����
	auto session = UserMgr::getInstance()->getSession(toUid);
	if (session == nullptr) {	// ������
		reply->set_error(ErrorCodes::UserOffline);
		return Status::OK;
	}

	// ���ߣ���ͻ��˷�������
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromUid"] = fromUid;
	rtvalue["applyName"] = request->name();
	rtvalue["nick"] = request->nick();
	rtvalue["sex"] = request->sex();
	rtvalue["icon"] = request->icon();
	rtvalue["desc"] = request->desc();

	std::string return_str = rtvalue.toStyledString();
	session->send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);


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


