#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"

using namespace std;

LogicSystem::LogicSystem() :_b_stop(false) {
	registerCallBacks();
	_worker_thread = std::thread(&LogicSystem::dealMsg, this);		// 创建工作线程，专门从消息队列中取出数据进行处理（根据会话的智能指针和消息内容就能进行相应处理）
}																	// 这里只创建了一个工作线程，如果处理速度较慢可以多创建几个

LogicSystem::~LogicSystem() {
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::postMsgToQue(shared_ptr < LogicNode> msg) {
	std::unique_lock<std::mutex> unique_lk(_mutex);		// 由于要访问_msg_que，先加锁
	// 如果放的很快处理的又很慢，_msg_que的大小会长得很快，应该判断当_msg_que满了的时候就不再添加新数据节点了
	if (_msg_que.size() >= 100) {
		std::cout << " _msg_que is full, can not push more msgs" << std::endl;
		return;
	}
	_msg_que.push(msg);					// 把msg扔到_msg_que消息队列
	// 本来是_msg_que.size() == 1，但_msg_que.size() >= 1合理点吧
	if (_msg_que.size() >= 1) {
		unique_lk.unlock();
		_consume.notify_one();
	}
}

void LogicSystem::dealMsg() {
	for (;;) {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		//判断队列为空则用条件变量阻塞等待，并释放锁
		while (_msg_que.empty() && !_b_stop) {		// 应该可以_consume.wait(unique_lk, [this](){return !_msg_que.empty() || _b_stop});
			_consume.wait(unique_lk);
		}

		//判断是否为关闭状态，把所有逻辑执行完后则退出循环
		if (_b_stop) {
			while (!_msg_que.empty()) {		// 依次弹出队列中所有消息都处理完了再退出
				auto msg_node = _msg_que.front();
				cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);	// 找到msg_id对应的回调函数
				if (call_back_iter == _fun_callbacks.end()) {	// 找不到就扔了不处理了
					_msg_que.pop();
					std::cout << "can not find corresponding handler" << std::endl;
					continue;
				}
				call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
					std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));	// 执行回调函数FunCallBack
				_msg_que.pop();
			}
			break;
		}

		//如果没有停服，且说明队列中有数据，处理队列中第一个消息节点
		auto msg_node = _msg_que.front();
		cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			std::cout << "msg id [" << msg_node->_recvnode->_msg_id << "] handler not found" << std::endl;
			continue;
		}
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::registerCallBacks() {		
	// 如果消息id类型为登录，则调用loginHandler
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::loginHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	// 消息类型为查找用户
	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::searchInfo, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::addFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	/*_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatTextMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);*/

}

void LogicSystem::loginHandler(shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	std::cout << "user login uid is  " << uid << " user token  is "<< token << endl;

	Json::Value  rtvalue;	// 回复给客户端的消息
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->send(return_str, MSG_CHAT_LOGIN_RSP);
		std::cout << "sended login response" << std::endl;
		});
	// 从redis获取uid和token的对应信息
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::getInstance()->get(token_key, token_value);
	if (!success) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	std::cout << "token given by user: " << token << ", token in redis: " << token_value << std::endl;
	//std::cout << "length of token: " << token.size() << ", length of token_value: " << token_value.size() << std::endl;
	if (token != token_value) {							// 之前这里token_value写成了token_key导致找了半天错。。。。。。。。。。。。。 
		std::cout << "token invalid!" << std::endl;
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return;
	}
	rtvalue["error"] = ErrorCodes::Success;

	// 从redis中查找用户信息
	std::string base_key = USER_BASE_INFO + uid_str;	
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = getBaseInfo(base_key, uid, user_info);
	if (!b_base) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	rtvalue["uid"] = uid;
	rtvalue["name"] = user_info->name;
	rtvalue["passwd"] = user_info->passwd;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
	// defer会自动发送给客户端的回包，在这无需手动发送

	// 从数据库获取好友申请列表

	// 从数据库获取好友列表

	auto serverName = ConfigMgr::getInst()["SelfServer"]["Name"];
	auto res = RedisMgr::getInstance()->hGet(LOGIN_COUNT, serverName);	// 从redis中查询当前服务器连接的客户端数量
	int count = 0;
	if (!res.empty()) {
		count = stoi(res);
	}
	else {
		std::cout << "Can't find Login Count" << std::endl;
	}
	count++;	// 给连接数加一
	std::string count_str = std::to_string(count);
	bool suc = RedisMgr::getInstance()->hSet(LOGIN_COUNT, serverName, count_str);

	// 给session绑定用户uid
	session->setUserId(uid);

	//为用户设置登录ip server的名字
	std::string ipkey = USERIPPREFIX + uid_str;
	RedisMgr::getInstance()->set(ipkey, serverName);

	// 用户uid和session绑定，方便踢人
	UserMgr::getInstance()->setUserSession(uid, session);

	std::cout << "Login success!" << std::endl;
}

bool LogicSystem::getBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userInfo) {
	std::string info_str = "";	// 储存redis中查询到的用户信息的字符串
	bool b_base = RedisMgr::getInstance()->get(base_key, info_str);
	if (b_base) {	// 如果在redis中查到了
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);

		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto passwd = root["passwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "searching user uid is  " << uid << ", name  is "
			<< name << ", passwd is " << passwd << ", email is " << email << endl;
		
		userInfo->uid = uid;
		userInfo->name = name;
		userInfo->passwd = passwd;
		userInfo->email = email;
		userInfo->nick = nick;
		userInfo->desc = desc;
		userInfo->sex = sex;
		userInfo->icon = icon;
		return true;
	}
	// redis中没有查到，到mysql里查
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::getInstance()->getUser(uid);
	if (user_info == nullptr) {
		return false;
	}
	userInfo = user_info;	// 直接赋值

	// 将查询到的数据写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = uid;
	redis_root["name"] = user_info->name;
	redis_root["passwd"] = user_info->passwd;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::getInstance()->set(base_key, redis_root.toStyledString());	// 以字符串的形式保存

}


void LogicSystem::searchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();	// 客户端传过来的可能是uid，也可能是昵称
	std::cout << "user SearchInfo uid is  " << uid_str << endl;

	Json::Value  rtvalue;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->send(return_str, ID_SEARCH_USER_RSP);
		});

	// 判断uid_str是否为纯数字，是的话说明为uid，不是的话说明为昵称
	bool b_digit = isPureDigit(uid_str);
	if (b_digit) {
		getUserByUid(uid_str, rtvalue);
	}
	else {
		getUserByName(uid_str, rtvalue);
	}
	return;
}

// 判断一个字符串是否全由数字组成
bool LogicSystem::isPureDigit(const std::string& s) {
	for (char c : s) {
		if (!std::isdigit(c)) {
			return false;
		}
	}
	return true;
}

void LogicSystem::getUserByUid(std::string uid_str, Json::Value& rtvalue) {
	rtvalue["error"] = ErrorCodes::Success;
	// 先从redis里面找
	std::string base_key = USER_BASE_INFO + uid_str;
	std::string info_str = "";
	bool success = RedisMgr::getInstance()->get(base_key, info_str);
	if (success) {	// 查找成功
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);

		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto passwd = root["passwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " passwd is " << passwd << " email is " << email << " icon is " << icon << endl;
		rtvalue["uid"] = uid;
		rtvalue["passwd"] = passwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;

	}
	// redis中查找失败再去mysql里找
	std::shared_ptr<UserInfo> userInfo = MysqlMgr::getInstance()->getUser(std::stoi(uid_str));
	if (userInfo == nullptr) {	// 查询出错或未找到用户
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	rtvalue["uid"] = userInfo->uid;
	rtvalue["passwd"] = userInfo->passwd;
	rtvalue["name"] = userInfo->name;
	rtvalue["email"] = userInfo->email;
	rtvalue["nick"] = userInfo->nick;
	rtvalue["desc"] = userInfo->desc;
	rtvalue["sex"] = userInfo->sex;
	rtvalue["icon"] = userInfo->icon;

	// 往redis中写入用户信息
	RedisMgr::getInstance()->set(base_key, rtvalue.toStyledString());
}

// 和getUserByUid类似
void LogicSystem::getUserByName(std::string name, Json::Value& rtvalue) {
	rtvalue["error"] = ErrorCodes::Success;
	std::string base_key = NAME_INFO + name;
	std::string userInfo_str = "";
	bool success = RedisMgr::getInstance()->get(base_key, userInfo_str);
	if (success) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(userInfo_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto passwd = root["passwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " passwd is " << passwd << " email is " << email << endl;

		rtvalue["uid"] = uid;
		rtvalue["passwd"] = passwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		return;
	}

	std::shared_ptr<UserInfo> userInfo = MysqlMgr::getInstance()->getUser(name);
	if (userInfo == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;	// 其实应该是昵称非法
		return;
	}
	rtvalue["uid"] = userInfo->uid;
	rtvalue["passwd"] = userInfo->passwd;
	rtvalue["name"] = userInfo->name;
	rtvalue["email"] = userInfo->email;
	rtvalue["nick"] = userInfo->nick;
	rtvalue["desc"] = userInfo->desc;
	rtvalue["sex"] = userInfo->sex;
	rtvalue["icon"] = userInfo->icon;

	RedisMgr::getInstance()->set(base_key, rtvalue.toStyledString());
}

void  LogicSystem::addFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Value rtvalue;
	Json::Value root;
	Json::Reader reader;
	
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session](){
		std::string rsp = rtvalue.toStyledString();
		session->send(rsp, MSG_IDS::ID_ADD_FRIEND_RSP);
		});

	reader.parse(msg_data, root);
	auto applyName = root["applyName"].asString();
	auto from_uid = root["uid"].asInt();
	auto to_uid = root["to_uid"].asInt();
	auto remark = root["remark"].asString();

	std::cout<<"user add friend, uid is " << from_uid << ", apply name is " << applyName 
			 << ", to_uid is " << to_uid << ", remark is " << remark << std::endl;

	bool success = MysqlMgr::getInstance()->addFriend(from_uid, to_uid);
	if (!success) {
		std::cout << "failed to add firend in mysql" << std::endl;
		rtvalue["error"] = ErrorCodes::MysqlErr;
	}

	// 通过redis查找目标用户所在server的地址（如果他在线的话）
	std::string to_str = std::to_string(to_uid);
	std::string to_ip_key = USERIPPREFIX + to_str;
	std::string ip_value = "";											// 存放目标用户所在服务器名称
	// 查找目标用户所在服务器
	bool b_ip = RedisMgr::getInstance()->get(to_ip_key, ip_value);
	// 没有查到说明用户不在线
	if(!b_ip) {
		std::cout << "target user offline" << std::endl;
		return;
	}

	// 查到了
	if (ip_value == ConfigMgr::getInst()["SelfServer"]["Name"]) {	// 如果就在本服务器
		// 通知对方该 好友申请 请求
		std::cout << "target user in the same server" << std::endl;
		Json::Value notice;
		notice["error"] = ErrorCodes::Success;
		notice["applyUid"] = from_uid;
		notice["applyName"] = applyName;
		notice["desc"] = "";
		std::string return_str = notice.toStyledString();
		// 返回给客户端？
		session->send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
	}
	else {															// 不在本服务器
		std::cout << "target user in server:" << ip_value << std::endl;
		std::string uid_str = std::to_string(from_uid);
		std::string base_key = USER_BASE_INFO + uid_str;
		auto fromUserInfo = std::make_shared<UserInfo>();
		bool b_info = this->getBaseInfo(base_key, from_uid, fromUserInfo);	// 查找本用户信息

		// 向目标用户所在的服务器发送grpc请求
		AddFriendReq add_req;
		add_req.set_applyuid(from_uid);
		add_req.set_touid(to_uid);
		add_req.set_name(applyName);
		add_req.set_desc("");
		if (b_info) {	// 如果查找到本用户信息的话就把查到的信息也加上
			add_req.set_icon(fromUserInfo->icon);
			add_req.set_nick(fromUserInfo->nick);
			add_req.set_sex(fromUserInfo->sex);
		}
		ChatGrpcClient::getInstance()->NotifyAddFriend(ip_value, add_req);

	}
	return;
}

