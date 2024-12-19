#pragma once
#include "Singleton.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class CSession;
// 用户管理类，通过unordered_map保存用户uid及对应的会话
class UserMgr: public Singleton<UserMgr>
{
	friend class Singleton<UserMgr>;
public:
	~UserMgr();
	std::shared_ptr<CSession> getSession(int uid);
	bool setUserSession(int uid, std::shared_ptr<CSession> session);
	void removeSession(int uid);

private:
	UserMgr();

	std::mutex _session_mtx;
	std::unordered_map<int, std::shared_ptr<CSession>> _uidToSessions;
};

