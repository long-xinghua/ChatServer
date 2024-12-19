#pragma once
#include "Singleton.h"
#include <queue>
#include <thread>
#include "CSession.h"
#include <queue>
#include <map>
#include <functional>
#include "const.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <unordered_map>
#include "data.h"
#include "MysqlDao.h"

typedef  function<void(shared_ptr<CSession>, const short& msg_id, const string& msg_data)> funCallBack;
class LogicSystem :public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	void postMsgToQue(shared_ptr < LogicNode> msg);
private:
	LogicSystem();					// ���캯���ﴴ�������߳�
	void dealMsg();					// �����߳����񣬴���_msg_que�е���Ϣ�ڵ�
	void registerCallBacks();		// ע����Բ�ͬ��Ϣid�Ļص�����FunCallBack
	void loginHandler(shared_ptr<CSession> session, const short& msg_id, const string& msg_data);			// �����¼����Ļص�
	bool getBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& user_info);					// ��redis��mysql�в�ѯ�û���Ϣ
	void searchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);		// ��������û�����Ļص�
	bool isPureDigit(const std::string& str);																// �ж�һ���ַ����Ƿ�Ϊ������
	void getUserByUid(std::string uid_str, Json::Value& rtvalue);											// ͨ��uid�����û�
	void getUserByName(std::string name, Json::Value& rtvalue);												// ͨ���ǳƲ����û�
	/*void AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	
	
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
	bool GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& list);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list);*/
	std::thread _worker_thread;
	std::queue<shared_ptr<LogicNode>> _msg_que;
	std::mutex _mutex;
	std::condition_variable _consume;
	bool _b_stop;
	std::map<short, funCallBack> _fun_callbacks;			// ��������id�в�ͬ�Ļص�����
	std::map<int, std::shared_ptr<UserInfo>> _users;							// �����¼�����û���Ϣ
};

