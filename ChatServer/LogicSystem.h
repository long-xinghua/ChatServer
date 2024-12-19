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
	LogicSystem();					// 构造函数里创建工作线程
	void dealMsg();					// 工作线程任务，处理_msg_que中的消息节点
	void registerCallBacks();		// 注册针对不同消息id的回调函数FunCallBack
	void loginHandler(shared_ptr<CSession> session, const short& msg_id, const string& msg_data);			// 处理登录请求的回调
	bool getBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& user_info);					// 从redis或mysql中查询用户信息
	void searchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);		// 处理查找用户请求的回调
	bool isPureDigit(const std::string& str);																// 判断一个字符串是否为纯数字
	void getUserByUid(std::string uid_str, Json::Value& rtvalue);											// 通过uid查找用户
	void getUserByName(std::string name, Json::Value& rtvalue);												// 通过昵称查找用户
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
	std::map<short, funCallBack> _fun_callbacks;			// 根据请求id有不同的回调函数
	std::map<int, std::shared_ptr<UserInfo>> _users;							// 储存记录过的用户信息
};

