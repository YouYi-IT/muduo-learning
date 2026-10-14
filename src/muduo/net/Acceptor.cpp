#include "Acceptor.h"
#include "InetAddress.h"
#include "Logger.h"

#include <sys/socket.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        LOG_FATAL("listen socket fail");
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      accept_socket_(createNonblocking()),  // socket
      accept_channel_(loop, accept_socket_.getFd()),
      listenning_(false)
{
    accept_socket_.setReuseAddr(true);
    accept_socket_.setReusePort(reuseport);
    accept_socket_.bindAddress(listenAddr);  // bind
    // TcpServer::start()  ->  Acceptor::listen
    // 如果有新用户连接，要执行一个回调（connfd =》channel -> subloop）
    // accept_channel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
    accept_channel_.setReadCallback([this](Timestamp receive_time){
        handleRead();
    });
}

Acceptor::~Acceptor()
{
    accept_channel_.disableAll();
    accept_channel_.remove();
}

// listenfd有事件发生了，就是有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = accept_socket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (new_connection_callback_)
        {
            new_connection_callback_(connfd, peerAddr);  // 轮询找到subloop，唤醒，分发当前新客户端的channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("accept error");
        if (errno == EMFILE)
        {
            LOG_ERROR("sockfd reached limit");
        }
    }
}

void Acceptor::setNewConnectionCallback(const NewConnectionCallback& cb)
{
    new_connection_callback_ = cb;
}

bool Acceptor::listenning() const
{
    return listenning_;
}

void Acceptor::listen()
{
    listenning_ = true;
    accept_socket_.listen();
    accept_channel_.enableReading();
}