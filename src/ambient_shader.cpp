#include "ambient_shader.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
namespace cge {
namespace {
using Words=std::vector<std::uint32_t>;
std::uint32_t Read(std::span<const std::uint8_t> b,std::size_t p) {
    std::uint32_t v{};if(p<=b.size() && b.size()-p>=4)std::memcpy(&v,b.data()+p,4);return v;
}
void Put(std::vector<std::uint8_t>& b,std::size_t p,std::uint32_t v){std::memcpy(b.data()+p,&v,4);}
std::array<std::uint32_t,4> Digest(std::span<const std::uint8_t> data) {
    std::array<std::uint32_t,4> state{0x67452301,0xefcdab89,0x98badcfe,0x10325476};
    constexpr unsigned rotations[]={7,12,17,22,5,9,14,20,4,11,16,23,6,10,15,21};
    static const auto constants=[] {
        std::array<std::uint32_t,64> k{};
        for(unsigned i=0;i<64;++i)k[i]=static_cast<std::uint32_t>(std::abs(std::sin(double(i+1)))*4294967296.0);
        return k;
    }();
    auto block=[&](std::span<const std::uint8_t> bytes) {
        auto a=state[0],b=state[1],c=state[2],d=state[3];
        for(unsigned i=0;i<64;++i) {
            std::uint32_t f{},j{};
            if(i<16){f=(b&c)|(~b&d);j=i;}
            else if(i<32){f=(d&b)|(~d&c);j=(5*i+1)%16;}
            else if(i<48){f=b^c^d;j=(3*i+5)%16;}
            else {f=c^(b|~d);j=(7*i)%16;}
            const auto next=b+std::rotl(a+f+constants[i]+Read(bytes,4*j),int(rotations[(i/16)*4+i%4]));
            a=d;d=c;c=b;b=next;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;
    };
    std::size_t offset=0;
    while(data.size()-offset>=64){block(data.subspan(offset,64));offset+=64;}
    const auto remainder=data.size()-offset;
    const auto bits=static_cast<std::uint32_t>(data.size()*8);
    std::array<std::uint8_t,64> tail{};
    if(remainder>=56) {
        std::memcpy(tail.data(),data.data()+offset,remainder);tail[remainder]=0x80;block(tail);tail.fill(0);
        std::memcpy(tail.data(),&bits,4);
    }else {
        std::memcpy(tail.data(),&bits,4);std::memcpy(tail.data()+4,data.data()+offset,remainder);tail[4+remainder]=0x80;
    }
    const auto end=(bits>>2)|1u;std::memcpy(tail.data()+60,&end,4);block(tail);return state;
}
struct Operand {
    unsigned begin{},end{},type{},dimensions{};
    std::array<unsigned,3> indices{};
    bool direct{true};
};
bool ParseOperand(const Words& words,unsigned& cursor,unsigned end,Operand& result,unsigned recursion=0) {
    if(recursion>8 || cursor>=end)return false;
    result.begin=cursor;const auto token=words[cursor++];
    result.type=(token>>12)&255;result.dimensions=(token>>20)&3;
    auto extension=token;
    while(extension&0x80000000u){if(cursor>=end)return false;extension=words[cursor++];}
    for(unsigned d=0;d<result.dimensions;++d) {
        const auto representation=(token>>(22+3*d))&7;
        if(representation>4)return false;
        if(representation!=2) {
            const unsigned count=representation==1 || representation==4?2u:1u;
            if(count>end-cursor)return false;
            result.indices[d]=words[cursor];cursor+=count;
            if(count==2)result.direct=false;
        }
        if(representation>=2) {
            result.direct=false;Operand relative;
            if(!ParseOperand(words,cursor,end,relative,recursion+1))return false;
        }
    }
    if(result.type==4 || result.type==5) {
        const auto components=token&3;
        if(components!=1 && components!=2)return false;
        const unsigned count=(components==1?1u:4u)*(result.type==5?2u:1u);
        if(count>end-cursor)return false;cursor+=count;
    }
    result.end=cursor;return true;
}
unsigned Dest(unsigned type,unsigned mask){return 2u|(mask<<4)|(type<<12)|(1u<<20);}
unsigned Source(unsigned type){return 2u|4u|(0xe4u<<4)|(type<<12)|(1u<<20);}
unsigned Scalar(unsigned type,unsigned component){return 2u|8u|(component<<4)|(type<<12)|(1u<<20);}
void Emit(Words& out,unsigned opcode,std::initializer_list<unsigned> operands) {
    out.push_back(opcode|(static_cast<unsigned>(operands.size()+1)<<24));out.insert(out.end(),operands);
}
struct Instruction {unsigned offset{},length{},row{99};Operand destination;};
}
AmbientShader AddAmbientOcclusion(std::span<const std::uint8_t> source) {
    AmbientShader result;
    auto fail=[&](const char* why){result.bytecode.clear();result.status=why;return result;};
    if(source.size()<36 || source.size()>4*1024*1024 || Read(source,0)!=0x43425844 ||
        Read(source,20)!=1 || Read(source,24)!=source.size())return fail("Unsupported shader container");
    const unsigned count=Read(source,28);
    if(!count || count>64 || 32+std::size_t(count)*4>source.size())return fail("Invalid shader chunks");
    const auto checksum=Digest(source.subspan(20));
    if(std::memcmp(checksum.data(),source.data()+4,16))return fail("Shader checksum mismatch");
    struct Chunk {unsigned tag{};std::vector<std::uint8_t> data;};
    std::vector<Chunk> chunks;unsigned program=UINT_MAX;
    for(unsigned c=0;c<count;++c) {
        const auto offset=Read(source,32+4*c);
        if(offset<32+count*4 || offset>source.size() || source.size()-offset<8)return fail("Invalid shader chunk offset");
        const auto size=Read(source,offset+4);
        if(size>source.size()-offset-8)return fail("Truncated shader chunk");
        const auto tag=Read(source,offset);
        if(tag==0x52444853 || tag==0x58454853){if(program!=UINT_MAX)return fail("Multiple shader programs");program=c;}
        chunks.push_back({tag,{source.begin()+offset+8,source.begin()+offset+8+size}});
    }
    if(program==UINT_MAX)return fail("Shader program unavailable");
    const auto& bytes=chunks[program].data;
    if(bytes.size()<8 || bytes.size()%4)return fail("Invalid shader program length");
    Words words(bytes.size()/4);std::memcpy(words.data(),bytes.data(),bytes.size());
    if((words[0]>>16)!=0 || (words[0]&0xff)!=0x50 || words[1]!=words.size())return fail("Requires pixel shader model 5.0");
    unsigned temps=UINT_MAX,position=UINT_MAX,firstCode=UINT_MAX,rows=0;
    bool positionDeclared=false;
    for(auto& chunk:chunks)if(chunk.tag==0x4e475349) {
        auto& signature=chunk.data;
        if(signature.size()<8)return fail("Invalid input signature");
        const auto entries=Read(signature,0);
        if(entries>32 || 8+std::size_t(entries)*24>signature.size())return fail("Invalid input signature entries");
        for(unsigned e=0;e<entries;++e) {
            const auto offset=8+e*24;
            if(Read(signature,offset+8)==1 && Read(signature,offset+12)==3 && (Read(signature,offset+20)&3)==3) {
                if(position!=UINT_MAX)return fail("Multiple position inputs");
                position=Read(signature,offset+16);
                signature[offset+21]|=3;
            }
        }
    }
    std::vector<Instruction> instructions;
    std::array<Words,3> normals;
    for(unsigned i=2;i<words.size();) {
        const auto op=words[i]&2047;
        unsigned length=(words[i]>>24)&127;
        if(op==53){if(i+1>=words.size())return fail("Truncated custom data");length=words[i+1];}
        if(!length || length>words.size()-i)return fail("Invalid shader instruction");
        Instruction instruction{i,length};
        if(op==104) {
            if(length!=2 || temps!=UINT_MAX || words[i+1]>4093)return fail("Unsupported temporary declaration");
            temps=words[i+1];
        }
        if(op==100 && length==4 && (words[i+1]>>12&255)==1 && (words[i+1]>>20&3)==1 && words[i+3]==1){if(position!=words[i+2])return fail("Position declaration mismatch");positionDeclared=true;}
        if(op==89 && length==4 && words[i+2]==13)return fail("Reserved occlusion constant slot occupied");
        if(op==88) {
            unsigned at=i+1;Operand resource;
            if(!ParseOperand(words,at,i+length,resource) || resource.type!=7 || resource.dimensions!=1 || resource.indices[0]>=126)
                return fail("Reserved occlusion resource slot occupied");
        }
        if(op<88 && op!=53 && firstCode==UINT_MAX)firstCode=i;
        if(op==17 && !(words[i]&0x80002000u)) {
            unsigned at=i+1;Operand destination,a,b;
            if(!ParseOperand(words,at,i+length,destination) || !ParseOperand(words,at,i+length,a) ||
                !ParseOperand(words,at,i+length,b) || at!=i+length)return fail("Unsupported ambient instruction operands");
            for(const auto* cb:{&a,&b}) {
                if(!cb->direct || cb->type!=8 || cb->dimensions!=2 || cb->indices[0]!=2 || cb->indices[1]<11 || cb->indices[1]>13)continue;
                const auto& normal=cb==&a?b:a;
                const auto token=words[destination.begin],mask=(token>>4)&15;
                if(destination.type!=0 || destination.dimensions!=1 || (token&15)!=2 || !std::has_single_bit(mask) ||
                    words[cb->begin]!=(2u|4u|(0xe4u<<4)|(8u<<12)|(2u<<20)))
                    return fail("Unsupported ambient matrix layout");
                const auto row=cb->indices[1]-11;
                if(rows&(1u<<row))return fail("Repeated ambient matrix row");
                rows|=1u<<row;instruction.row=row;instruction.destination=destination;
                normals[row]={words.begin()+normal.begin,words.begin()+normal.end};
            }
        }
        instructions.push_back(instruction);i+=length;
    }
    if(rows!=7 || temps==UINT_MAX || position==UINT_MAX || firstCode==UINT_MAX || position>=32)
        return fail("Directional ambient signature unavailable");
    if(normals[0]!=normals[1] || normals[1]!=normals[2])return fail("Ambient normal operands differ");
    Words out{words[0],0};
    for(const auto& instruction:instructions) {
        const auto i=instruction.offset;
        if(i==firstCode) {
            Emit(out,88u|(3u<<11),{Source(7),126,0x5555});
            if(!positionDeclared)Emit(out,100u|(4u<<11),{Dest(1,3),position,1});
            Emit(out,89,{0x208e46,13,1});
            Emit(out,54,{Dest(0,1),temps+1,0x4001,0x3f800000});
            Emit(out,31u|(1u<<18),{0x20800a,13,0});
            Emit(out,54,{Dest(0,15),temps,0x4001,0});
            Emit(out,27,{Dest(0,3),temps,Source(1),position});
            Emit(out,45,{Dest(0,15),temps+1,Source(0),temps,Source(7),126});
            Emit(out,50,{Dest(0,1),temps+1,Scalar(0,0),temps+1,0x4001,0xbf800000,0x4001,0x3f800000});
            Emit(out,21,{});
        }
        if((words[i]&2047)==100 && instruction.length==4 && words[i+2]==position) {
            out.push_back(words[i]);out.push_back(words[i+1]|0x30u);out.push_back(position);out.push_back(words[i+3]);
        }else if((words[i]&2047)==104){out.push_back(words[i]);out.push_back(temps+2);}
        else out.insert(out.end(),words.begin()+i,words.begin()+i+instruction.length);
        if(instruction.row<3) {
            const auto& destination=instruction.destination;
            const auto token=words[destination.begin],reg=destination.indices[0];
            Emit(out,56,{token,reg,Source(0),reg,Scalar(0,0),temps+1});
        }
    }
    out[1]=static_cast<unsigned>(out.size());
    chunks[program].data.resize(out.size()*4);std::memcpy(chunks[program].data.data(),out.data(),out.size()*4);
    auto& output=result.bytecode;output.resize(32+4*count);Put(output,0,0x43425844);Put(output,20,1);Put(output,28,count);
    for(unsigned c=0;c<count;++c) {
        Put(output,32+4*c,static_cast<unsigned>(output.size()));
        const auto start=output.size();output.resize(start+8+chunks[c].data.size());
        Put(output,start,chunks[c].tag);Put(output,start+4,static_cast<unsigned>(chunks[c].data.size()));
        std::memcpy(output.data()+start+8,chunks[c].data.data(),chunks[c].data.size());
    }
    Put(output,24,static_cast<unsigned>(output.size()));const auto updated=Digest(std::span<const std::uint8_t>(output).subspan(20));
    std::memcpy(output.data()+4,updated.data(),16);result.ambientRows=3;result.status="Directional ambient occlusion shader ready";return result;
}
}
