#include "UserMgr.h"
#include "CSession.h"	

std::shared_ptr<CSession> UserMgr::getSession(int uid) {
	std::unique_lock<std::mutex> lock(_session_mtx);
	auto iter_find = _uidToSessions.find(uid);
	if (iter_find == _uidToSessions.end()) {
		return nullptr;
	}
	return iter_find->second;
}

UserMgr::UserMgr(){

}

UserMgr::~UserMgr() {
	_uidToSessions.clear();
}

bool UserMgr::setUserSession(int uid, std::shared_ptr<CSession> session) {
	std::unique_lock<std::mutex> lock(_session_mtx);
	_uidToSessions[uid] = session;
	return true;
}

void UserMgr::removeSession(int uid){
	auto uid_str = std::to_string(uid);
	//因为再次登录可能是其他服务器，所以会造成本服务器删除key，其他服务器注册key的情况
	// 有可能其他服务登录，本服删除key造成找不到key的情况

	//RedisMgr::GetInstance()->Del(USERIPPREFIX + uid_str);

	{
		std::lock_guard<std::mutex> lock(_session_mtx);
		_uidToSessions.erase(uid);
	}
}
