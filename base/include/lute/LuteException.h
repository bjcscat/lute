#pragma once

#include <stdexcept>

class LuteException : public std::runtime_error
{
public:
    explicit LuteException(const std::string& exception)
        : std::runtime_error{exception}
    {
    }
};
