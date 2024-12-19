#pragma once
#include "Singleton.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class CSession;
// �û������࣬ͨ��unordered_map�����û�uid����Ӧ�ĻỰ
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

