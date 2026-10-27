#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <limits>
#include <algorithm>
namespace cge {
inline std::size_t RipPrologueInstruction(std::span<const unsigned char> code) noexcept {
    if(code.empty())return 0;
    const std::size_t prefix=code[0]>=0x40 && code[0]<=0x4f?1:0;
    if(code.size()<prefix+6)return 0;
    const auto op=code[prefix];
    return (op==0x8b || op==0x89 || op==0x8d) && (code[prefix+1]&0xc7)==0x05?prefix+6:0;
}
inline std::size_t PrologueInstruction(std::span<const unsigned char> code) noexcept {
    std::size_t at=0;
    if(code.empty())return 0;
    if(code.size()>=4 && code[0]==0xf3 && code[1]==0x0f && code[2]==0x1e && code[3]==0xfa)return 4;
    unsigned rex=0;
    if(code[at]>=0x40 && code[at]<=0x4f){rex=code[at++];if(at==code.size())return 0;}
    const auto op=code[at++];
    if(op>=0x50 && op<=0x57)return at;
    if(op==0x83 || op==0x81) {
        if(rex!=0x48 || at>=code.size() || code[at++]!=0xec)return 0;
        const std::size_t bytes=op==0x83?1:4;
        if(at+bytes>code.size() || (op==0x83 && code[at]>=128))return 0;
        return at+bytes;
    }
    if(op!=0x89 && op!=0x8b && op!=0x8d)return 0;
    if(at>=code.size())return 0;
    const auto modrm=code[at++];
    const unsigned mod=modrm>>6,rm=modrm&7;
    if(mod==3)return op==0x8d?0:at;
    if(mod==0 && rm==5)return 0;
    if(rm==4) {
        if(at>=code.size())return 0;
        const auto sib=code[at++];
        if(mod==0 && (sib&7)==5)return 0;
    }
    const std::size_t disp=mod==1?1:mod==2?4:0;
    return at+disp<=code.size()?at+disp:0;
}
inline std::size_t PrologueLength(std::span<const unsigned char> code,std::size_t minimum=14) noexcept {
    std::size_t length=0;
    while(length<minimum && length<code.size()) {
        const auto bytes=PrologueInstruction(code.subspan(length));
        if(!bytes)return 0;
        length+=bytes;
    }
    return length>=minimum?length:0;
}
inline std::size_t RelocatePrologue(std::span<const unsigned char> source,std::span<unsigned char> destination,std::size_t minimum=5) noexcept {
    std::array<unsigned char,32> copy{};
    std::size_t length=0;
    if(minimum>copy.size())return 0;
    while(length<minimum) {
        if(length>=source.size())return 0;
        const auto code=source.subspan(length);
        const auto relative=RipPrologueInstruction(code);
        const auto bytes=relative?relative:PrologueInstruction(code);
        if(!bytes || bytes>copy.size()-length || bytes>destination.size()-std::min(length,destination.size()))return 0;
        std::memcpy(copy.data()+length,code.data(),bytes);
        if(relative) {
            std::int32_t displacement{};std::memcpy(&displacement,code.data()+bytes-4,4);
            const auto target=reinterpret_cast<std::intptr_t>(code.data()+bytes)+displacement;
            const auto delta=target-reinterpret_cast<std::intptr_t>(destination.data()+length+bytes);
            if(delta<std::numeric_limits<std::int32_t>::min() || delta>std::numeric_limits<std::int32_t>::max())return 0;
            const auto adjusted=static_cast<std::int32_t>(delta);std::memcpy(copy.data()+length+bytes-4,&adjusted,4);
        }
        length+=bytes;
    }
    for(std::size_t at=0;at<length;) {
        const auto code=source.subspan(at);
        const auto relative=RipPrologueInstruction(code);
        if(relative) {
            std::int32_t displacement{};std::memcpy(&displacement,code.data()+relative-4,4);
            const auto target=reinterpret_cast<std::intptr_t>(code.data()+relative)+displacement;
            const auto start=reinterpret_cast<std::intptr_t>(source.data());
            if(target>=start && target<start+static_cast<std::intptr_t>(length))return 0;
        }
        at+=relative?relative:PrologueInstruction(code);
    }
    std::memcpy(destination.data(),copy.data(),length);return length;
}
inline bool NativeEntry(std::span<const unsigned char> code) noexcept {
    if(code.size()>=4 && code[0]==0xf3 && code[1]==0x0f && code[2]==0x1e && code[3]==0xfa)
        return NativeEntry(code.subspan(4));
    if(PrologueInstruction(code) || RipPrologueInstruction(code))return true;
    if(code.size()>=3 && code[0]==0x0f && code[1]==0x57 && (code[2]&0xc0)==0xc0)return true;
    return false;
}

}
