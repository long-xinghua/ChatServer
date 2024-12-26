#include "ChatGrpcClient.h"

ChatConPool::ChatConPool(size_t poolSize, std::string host, std::string port) :
	_poolSize(poolSize), _host(host), _port(port), _b_stop(false) {
	for (int i = 0; i < _poolSize; i++) {
		std::shared_ptr<Channel> channel = grpc::CreateChannel(_host + ":" + _port, grpc::InsecureChannelCredentials());
		_connections.push(ChatService::NewStub(channel));
	}
}

ChatConPool::~ChatConPool() {
	std::lock_guard<std::mutex> lock(_mutex);
	close();
	while (!_connections.empty()) {
		_connections.pop();
	}
}

std::unique_ptr<ChatService::Stub> ChatConPool::getConnection() {
	std::unique_lock<std::mutex> lock(_mutex);
	_cond.wait(lock, [this]() {
		if (!_b_stop) {
			if (_connections.empty()) {
				return false;
			}
		}
		return true;
		});
	if (_b_stop) {
		return nullptr;
	}
	auto stub = std::move(_connections.front());
	_connections.pop();
	return stub;

}

bool ChatConPool::returnConnection(std::unique_ptr<ChatService::Stub> stub) {
	std::unique_lock<std::mutex> lock(_mutex);
	if (_b_stop) {
		return true;
	}
	_connections.push(std::move(stub));
	_cond.notify_one();
	return true;
}

void ChatConPool::close() {
	_b_stop = true;
	_cond.notify_all();
}



ChatGrpcClient::ChatGrpcClient() {
	auto& cfg = ConfigMgr::getInst();
	auto serverList = cfg["PeerServer"]["Servers"];

	std::vector<std::string> words;

	std::stringstream ss(serverList);
	std::string word;

	while (std::getline(ss, word, ',')) {	// std::getline读取输入流中的数据，遇到分隔符时停止，将读到的内容存入word
		words.push_back(word);
	}
	// 得到服务器名称后寻找各服务器详细信息
	for (auto& server : words) {
		if (cfg[server]["Name"].empty()) {
			continue;
		}
		// 对每个ChatServer都建立连接池进行连接
		_pools[cfg[server]["Name"]] = std::make_unique<ChatConPool>(5, cfg[server]["Host"], cfg[server]["RPCPort"]);
	}
}

ChatGrpcClient::~ChatGrpcClient() {
	
}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_ip, const AddFriendReq& req) {
	AddFriendRsp rsp;
	rsp.set_error(ErrorCodes::Success);
	Defer defer([&rsp, &req]() {
		rsp.set_applyuid(req.applyuid());
		rsp.set_touid(req.touid());
		});
	// 从和其他ChatServer的连接中找目标服务器
	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	// 拿到连接目标服务器的连接池
	auto& pool = find_iter->second;
	auto stub = pool->getConnection();
	Defer deferCon([this, &pool, &stub]() {
		pool->returnConnection(std::move(stub));
		});
	ClientContext context;					// 没太懂这个context
	Status status = stub->NotifyAddFriend(&context, req, &rsp);
	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	std::cout << "succeed to send NotifyAddFriend to target server:" << server_ip << std::endl;
	return rsp;
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req) {
	AuthFriendRsp rsp;
	rsp.set_error(ErrorCodes::Success);

	Defer defer([&rsp, &req]() {
		rsp.set_fromuid(req.fromuid());
		rsp.set_touid(req.touid());
		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->getConnection();
	Status status = stub->NotifyAuthFriend(&context, req, &rsp);
	Defer defercon([&stub, this, &pool]() {
		pool->returnConnection(std::move(stub));
		});

	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}

	return rsp;
}

bool ChatGrpcClient::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userInfo) {
	return true;
	//todo...
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue) {
	TextChatMsgRsp rsp;
	return rsp;
	//todo...
}