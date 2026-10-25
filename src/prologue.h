#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace cge {
inline std::size_t PrologueInstruction(std::span<const unsigned char> code) noexcept {
    std::size_t at=0;
    if(code.empty())return 0;
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
}
