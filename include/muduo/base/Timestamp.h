#pragma once

#include <iostream>
#include <string>

namespace muduo::base
{

class Timestamp
{
public:
    Timestamp();
    // explicit 防止Timestamp和参数里面的类型（比如int64_t）发生隐式类型转换
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_;
};

}  // namespace muduo::base