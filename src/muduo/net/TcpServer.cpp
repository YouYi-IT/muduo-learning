#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <functional>
#include <string.h>

EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("mainloop is null");
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listen_addr, const std::string& name_arg, Option option)
    : loop_(CheckLoopNotNull(loop)),
      ip_port_(listen_addr.toIpPort()),
      name_(name_arg),
      acceptor_(new Acceptor(loop, listen_addr, option)),
      thread_pool_(new EventLoopThreadPool(loop, name_)),
      connection_callback_(),
      message_callback_(),
      next_conn_id_(1)
{
    acceptor_->setNewConnectionCallback([this](int sockfd, const InetAddress& peerAddr)
                                        { newConnection(sockfd, peerAddr); });
}

// TcpConnection 对象内部有一个 Channel，注册在某个 EventLoop 上
// 如果 TcpServer 析构时直接让 connections_ 里的 shared_ptr 析构，TcpConnection 可能会在非所属 EventLoop 线程中被销毁，导致 Channel 的移除操作出现线程安全问题
// 所以必须把“清理连接”这件事投递到该连接所属的 EventLoop 线程去执行
TcpServer::~TcpServer()
{
    for(auto& item:connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop([conn](){
            conn->connectDestroyed();
        });
    }
}

void TcpServer::setThreadInitCallback(const ThreadInitCallback cb)
{
    thread_init_callback_ = std::move(cb);
}
void TcpServer::setConnectionCallback(const ConnectionCallback cb)
{
    connection_callback_ = std::move(cb);
}
void TcpServer::setMessageCallback(const MessageCallback cb)
{
    message_callback_ = std::move(cb);
}
void TcpServer::setWriteCompleteCallback(const WriteCompleteCallback cb)
{
    write_complete_callback_ = std::move(cb);
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    thread_pool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_++ == 0)  // 防止一个TcpServer对象被start多次
    {
        thread_pool_->start(thread_init_callback_);
        // loop_->runInLoop([this](){acceptor_->listen();});
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peer_addr)
{
    // 轮询算法，选择一个subloop，来管理channel
    EventLoop* ioloop = thread_pool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ip_port_.data(), next_conn_id_);
    std::string conn_name = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s", name_.data(), conn_name.data(),
             peer_addr.toIpPort().data());

    // 通过sockfd获取其绑定本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local,sizeof(local));
    socklen_t addrlen=sizeof(local);
    if(::getsockname(sockfd,(sockaddr*)&local,&addrlen)<0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress local_addr(local);

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(std::make_shared<TcpConnection>(ioloop,conn_name,sockfd,local_addr,peer_addr));
    connections_[conn_name]=conn;
    // TcpServer => TcpConnection => Channel => Poller => notify channel调用回调
    conn->setConnectionCallback(connection_callback_);
    conn->setMessageCallback(message_callback_);
    conn->setWriteCompleteCallback(write_complete_callback_);
    
    conn->setCloseCallback([this](const TcpConnectionPtr& conn1){
        removeConnection(conn1);
    });
    ioloop->runInLoop([conn](){
        conn->connectEstablished();
    });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop([this,conn](){
        removeConnectionInLoop(conn);
    });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s",name_.data(),conn->getName().data());
    
    size_t n=connections_.erase(conn->getName());
    EventLoop* ioloop=conn->getLoop();
    // 和TcpConnection种的 self = shared_from_this() 一样，用的是智能指针，让lambda捕获，计数器加1，保证能完成回调，而不是在TcpConnection中用this这个普通指针
    ioloop->queueInLoop([conn](){
        conn->connectDestroyed();
    });
}