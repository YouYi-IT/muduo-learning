#pragma once

// 用户用muduo时会导入这个头文件，所有不要前置声明，避免用户还得导入文件
#include <muduo/base/noncopyable.h>
#include <muduo/net/Acceptor.h>
#include <muduo/net/Callback.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/InetAddress.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace muduo::net
{

class TcpServer : public muduo::base::noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePort
    };

    TcpServer(EventLoop* loop, const InetAddress& listen_addr, const std::string& name_arg,
              Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback cb);
    void setConnectionCallback(const ConnectionCallback cb);
    void setMessageCallback(const MessageCallback cb);
    void setWriteCompleteCallback(const WriteCompleteCallback cb);

    // 设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress& peer_addr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;
    const std::string ip_port_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;  // 运行在mainloop，任务是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> thread_pool_;  // one loop per thread

    ConnectionCallback connection_callback_;         // 有新连接时的回调
    MessageCallback message_callback_;               // 有读写消息时的回调
    WriteCompleteCallback write_complete_callback_;  // 消息发送完成的回调

    ThreadInitCallback thread_init_callback_;  // loop线程初始化的回调
    std::atomic_int started_;

    int next_conn_id_;
    ConnectionMap connections_;  // 保存所有连接
};

}  // namespace muduo::net