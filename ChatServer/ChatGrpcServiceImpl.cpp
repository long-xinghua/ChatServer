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

	// 先查找用户是否还在线
	auto session = UserMgr::getInstance()->getSession(toUid);
	if (session == nullptr) {	// 不在线
		reply->set_error(ErrorCodes::UserOffline);
		return Status::OK;
	}

	// 在线，向客户端发送提醒
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["from_uid"] = fromUid;
	rtvalue["applyName"] = request->name();
	rtvalue["nick"] = request->nick();
	rtvalue["sex"] = request->sex();
	rtvalue["icon"] = request->icon();
	rtvalue["desc"] = request->desc();

	std::string return_str = rtvalue.toStyledString();
	session->send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);


	return Status::OK;
}

Status ChatGrpcServiceImpl::NotifyAuthFriend(::grpc::ServerContext* context, const AuthFriendReq* request, AuthFriendRsp* reply) {
	//查找用户是否在本服务器
	auto touid = request->touid();
	auto fromuid = request->fromuid();
	auto session = UserMgr::getInstance()->getSession(touid);

	Defer defer([request, reply]() {
		reply->set_error(ErrorCodes::Success);
		reply->set_fromuid(request->fromuid());
		reply->set_touid(request->touid());
		});

	//用户不在内存中则直接返回
	if (session == nullptr) {
		return Status::OK;
	}

	//在内存中则直接发送通知对方
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["from_uid"] = request->fromuid();
	rtvalue["to_uid"] = request->touid();

	std::string base_key = USER_BASE_INFO + std::to_string(fromuid);
	auto user_info = std::make_shared<UserInfo>();
	// 获取认证方的信息
	bool b_info = GetBaseInfo(base_key, fromuid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}

	std::string return_str = rtvalue.toStyledString();

	session->send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
	return Status::OK;
}

Status ChatGrpcServiceImpl::NotifyTextChatMsg(::grpc::ServerContext* context, const TextChatMsgReq* request, TextChatMsgRsp* response) {
	//todo...
	return Status::OK;
}

bool ChatGrpcServiceImpl::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo) {
	//优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::getInstance()->get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->passwd = root["passwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		std::cout << "user login uid is  " << userinfo->uid << " name  is "
			<< userinfo->name << " passwd is " << userinfo->passwd << " email is " << userinfo->email << endl;
	}
	else {
		//redis中没有则查询mysql
		//查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		
		user_info = MysqlMgr::getInstance()->getUser(uid);
		if (user_info == nullptr) {
			return false;
		}

		userinfo = user_info;

		//将数据库内容写入redis缓存
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["passwd"] = userinfo->passwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::getInstance()->set(base_key, redis_root.toStyledString());
	}

	return true;
}


