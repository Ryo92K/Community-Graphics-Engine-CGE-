#pragma once
#include <array>
#include <cmath>
namespace cge {
class KeyboardPresses {
    std::array<bool,256> down_{};
public:
    bool Observe(unsigned code,float value,float held) noexcept {
        if(code==0 || code>=down_.size() || !std::isfinite(value) || !std::isfinite(held) || value<0 || held<0)return false;
        const bool wasDown=down_[code];down_[code]=value>0;
        return down_[code] && !wasDown && held==0;
    }
};
}
