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
	//��Ϊ�ٴε�¼���������������������Ի���ɱ�������ɾ��key������������ע��key�����
	// �п������������¼������ɾ��key����Ҳ���key�����

	//RedisMgr::GetInstance()->Del(USERIPPREFIX + uid_str);

	{
		std::lock_guard<std::mutex> lock(_session_mtx);
		_uidToSessions.erase(uid);
	}
}
