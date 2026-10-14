#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callback.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

// TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
// => TcpConnection设置回调 => Channel => Poller => Channel回调操作
class TcpConnection:public noncopyable,public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,const std::string& name,int sockfd,const InetAddress& local_addr,const InetAddress& peer_addr);
    ~TcpConnection();

    EventLoop* getLoop() const;
    const std::string& getName() const;
    const InetAddress& localAddress() const;
    bool connected() const;

    void setConnectionCallback(const ConnectionCallback cb);
    void setMessageCallback(const MessageCallback cb);
    void setWriteCompleteCallback(const WriteCompleteCallback cb);
    void setHighWaterMarkCallback(const HighWaterMarkCallback cb,size_t highwater_mark);
    void setCloseCallback(const CloseCallback cb);

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

    // 发送数据
    // void send(const void* message,int len);
    void send(const std::string& str);
    
    // 关闭连接
    void shutdown();

private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    void handleRead(Timestamp receive_time);
    void handleWrite();
    void handleClose();
    void handleError();

    // 发送数据
    void sendInLoop(const void* message,size_t len);

    // 关闭连接
    void shutdownInLoop();

    EventLoop* loop_;   // 这里肯定不是mainloop
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 和acceptor类似    Acceptor => mainloop   TcpConnection => subloop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress local_addr_;
    const InetAddress peer_addr_;

    ConnectionCallback connection_callback_;        // 有新连接时的回调
    MessageCallback message_callback_;              // 有读写消息时的回调
    WriteCompleteCallback write_complete_callback_; // 消息发送完成的回调
    HighWaterMarkCallback highwater_mark_callback_;
    CloseCallback close_callback_;

    size_t highwater_mark_;

    Buffer input_buffer_;
    Buffer output_buffer_;
};