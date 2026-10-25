#include "renderer.h"
#include <d3d11.h>
#include <array>
#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace cge;
using Pixel=std::array<unsigned char,4>;
void Check(bool ok,const char* message){if(!ok){std::printf("FAIL: %s\n",message);std::exit(1);}}
int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"Fixture path");const std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
    auto write=[&](const char* name,const std::string& data){std::ofstream f(root/name,std::ios::binary);f<<data;};
    auto cube=[](unsigned n,int mode) {
        std::ostringstream s;s<<"TITLE \"CGE test\"\nLUT_3D_SIZE "<<n<<"\n";
        for(unsigned b=0;b<n;++b)for(unsigned g=0;g<n;++g)for(unsigned r=0;r<n;++r) {
            const float x=float(r)/(n-1),y=float(g)/(n-1),z=float(b)/(n-1);
            if(mode==1)s<<z<<' '<<x<<' '<<y;
            else if(mode==2)s<<x*0.5f<<' '<<y<<' '<<z;
            else if(mode==3)s<<x*y*z<<' '<<x*y*z<<' '<<x*y*z;
            else s<<x<<' '<<y<<' '<<z;
            s<<'\n';
        }
        return s.str();
    };
    const auto identity=cube(2,0);write("Identity.cube",identity);write("Swap.cube",cube(2,1));write("Half.cube",cube(2,2));write("Tetra.cube",cube(2,3));
    CubeLut parsed;std::string error;
    Check(LoadCube(root/"Identity.cube",parsed,error) && parsed.size==2 && parsed.values[1][0]==1 && parsed.values[4][2]==1,"Red-fastest cube ordering");
    write("Domain.cube","DOMAIN_MIN -1 -1 -1\nDOMAIN_MAX 1 1 1\n"+identity);
    Check(LoadCube(root/"Domain.cube",parsed,error) && parsed.minimum[0]==-1,"Domain headers");
    write("Range.cube","LUT_3D_INPUT_RANGE -1 1\n"+identity);
    Check(LoadCube(root/"Range.cube",parsed,error) && parsed.maximum[2]==1,"Resolve input range");
    write("Bom.CUBE",std::string("\xef\xbb\xbf")+"# test\n"+identity);
    Check(LoadCube(root/"Bom.CUBE",parsed,error),"UTF8 BOM and comments");
    for(const auto& bad:std::vector<std::string>{"", "LUT_1D_SIZE 2\n0 0 0\n1 1 1\n", "LUT_3D_SIZE 66\n", "LUT_3D_SIZE 1\n", "LUT_3D_SIZE 2 junk\n", "LUT_3D_SIZE 2\n0 0 0\n", identity+"1 1 1\n", "DOMAIN_MIN 1 0 0\n"+identity, "DOMAIN_MAX 0 1 1\n"+identity, "DOMAIN_MIN 0 0 0\nDOMAIN_MIN 0 0 0\n"+identity, "LUT_3D_SIZE 2\nLUT_3D_SIZE 2\n", "LUT_3D_SIZE 2\nnan 0 0\n", "LUT_3D_SIZE 2\n1e39 0 0\n", "LUT_3D_SIZE 2\n0 0 0 junk\n", identity+"DOMAIN_MIN 0 0 0\n", "LUT_3D_SIZE 2\nLUT_1D_SIZE 2\n",std::string(4097,' ')+identity}) {
        write("Bad.cube",bad);Check(!LoadCube(root/"Bad.cube",parsed,error) && !error.empty() && parsed.values.empty(),"Malformed LUT rejected without partial data");
    }
    std::filesystem::remove(root/"Missing.cube");
    Check(!LoadCube(root/"Missing.cube",parsed,error),"Missing LUT rejected");
    write("Oversize.cube","");std::filesystem::resize_file(root/"Oversize.cube",32*1024*1024+1);
    Check(!LoadCube(root/"Oversize.cube",parsed,error),"Oversized LUT rejected before parsing");
    std::filesystem::remove(root/"Oversize.cube");
    Check(ValidLutName("Warm Look-01.CUBE") && !ValidLutName("../a.cube") && !ValidLutName("a/b.cube") && !ValidLutName("a:stream.cube") && !ValidLutName("a.cube\nEnabled=true") && !ValidLutName("a.png") && !ValidLutName(" leading.cube"),"LUT filename validation");
    {std::ofstream f(root/L"\U0001f600.cube");f<<identity;}
    {std::ofstream f(root/L"\U0001f600.txt");f<<"ignored";}
    const auto listed=ListLuts(root);
    Check(std::find(listed.begin(),listed.end(),"Identity.cube")!=listed.end() &&
        std::all_of(listed.begin(),listed.end(),[](const auto& name){return ValidLutName(name);}),"Unsupported Unicode filenames are skipped without hiding valid LUTs");Check(std::find(listed.begin(),listed.end(),"Bom.CUBE")!=listed.end(),"Case-insensitive cube listing");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    Renderer renderer;Check(renderer.Initialize(device.Get(),error),error.c_str());renderer.SetLutDirectory(root);
    constexpr unsigned width=256,height=4;
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM}) {
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;d.Format=format;d.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> output,readback;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"Output");
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&readback)),"Readback");
        std::vector<Pixel> input(width*height);
        for(unsigned i=0;i<input.size();++i)input[i]={static_cast<unsigned char>(i%256),static_cast<unsigned char>((i*37)%256),static_cast<unsigned char>((i*71)%256),77};
        auto draw=[&](ColorSettings s,bool enabled=true) {
            auto bytes=input;if(format==DXGI_FORMAT_B8G8R8A8_UNORM)for(auto& p:bytes)std::swap(p[0],p[2]);
            context->UpdateSubresource(output.Get(),0,nullptr,bytes.data(),width*4,0);renderer.Draw(output.Get(),s,enabled);
            context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE m{};Check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)),"Readback map");
            std::vector<Pixel> result(input.size());for(unsigned y=0;y<height;++y)memcpy(result.data()+y*width,static_cast<unsigned char*>(m.pData)+y*m.RowPitch,width*4);context->Unmap(readback.Get(),0);
            if(format==DXGI_FORMAT_B8G8R8A8_UNORM)for(auto& p:result)std::swap(p[0],p[2]);
            for(const auto& p:result)Check(p[3]==77,"Alpha preserved");return result;
        };
        ColorSettings s;s.lut={true,1,"Identity.cube"};Check(draw(s)==input,"Identity LUT preserves arbitrary RGB and endpoints");
        for(unsigned n:{17u,33u,65u}) {
            write("Grid.cube",cube(n,0));renderer.ReloadLut();s.lut.file="Grid.cube";Check(draw(s)==input,"Non-power-of-two LUT sizes render correctly");
        }
        s.lut.file="Swap.cube";auto swapped=draw(s);
        for(unsigned i=0;i<input.size();++i)Check(swapped[i][0]==input[i][2] && swapped[i][1]==input[i][0] && swapped[i][2]==input[i][1],"3D channel permutation and axis order");
        s.lut.file="Tetra.cube";auto tetra=draw(s);
        for(unsigned i=0;i<input.size();++i) {
            const int expected=std::min({input[i][0],input[i][1],input[i][2]});
            for(unsigned c=0;c<3;++c)Check(std::abs(int(tetra[i][c])-expected)<=1,"Tetrahedral interpolation covers all channel orderings");
        }
        s.lut.file="Half.cube";s.lut.strength=0.5f;auto half=draw(s);
        for(unsigned i=0;i<input.size();++i)Check(std::abs(int(half[i][0])-input[i][0]*0.75f)<=1 && half[i][1]==input[i][1] && half[i][2]==input[i][2],"Strength blends only LUT result");
        s.lut.strength=0;Check(draw(s)==input,"Zero strength exact bypass");s.lut.strength=1;s.lut.enabled=false;Check(draw(s)==input,"Effect off exact bypass");s.lut.enabled=true;Check(draw(s,false)==input,"Master off exact bypass");
        s.lut.file="Domain.cube";auto domain=draw(s);
        for(unsigned i=0;i<input.size();++i)for(unsigned c=0;c<3;++c)Check(std::abs(int(domain[i][c])-(input[i][c]*0.5f+127.5f))<=1,"Per-channel domain maps input");
        s.lut.file="Missing.cube";Check(draw(s)==input && renderer.LutStatus().find("missing")!=std::string::npos,"Missing LUT bypass and diagnostic");
        s.lut.enabled=false;draw(s);s.lut.enabled=true;
        Check(draw(s)==input && renderer.LutStatus().find("missing")!=std::string::npos,"Cached failure survives disable and re-enable");
        write("Missing.cube",cube(2,2));Check(draw(s)==input,"Failures cached until explicit reload");renderer.ReloadLut();Check(draw(s)!=input,"Reload recovers newly installed LUT");
        std::filesystem::remove(root/"Missing.cube");renderer.ReloadLut();
        s.lut.file="Bad.cube";Check(draw(s)==input,"Malformed LUT preserves image");
        s.exposure=0.4f;s.lut.enabled=false;const auto graded=draw(s);s.lut.enabled=true;Check(draw(s)==graded,"Invalid LUT leaves other grading functional");
        s.lut.file="Identity.cube";Check(draw(s)==graded,"Identity LUT composes with other effects");
        s={};s.liftGammaGain.enabled=true;s.liftGammaGain.gainRed=0.5f;s.lut={true,1,"Swap.cube"};const auto ordered=draw(s);
        for(unsigned i=0;i<input.size();++i)Check(std::abs(int(ordered[i][1])-input[i][0]*0.5f)<=1,"LUT follows RGB grading");
    }
    std::puts("PASS: cube validation, domains, sizes, tetrahedral interpolation, strength, formats, reload, bypass and grading order.");
}
