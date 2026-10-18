#include "../native/ScreenRecorder/FrameComposer.h"
#include "../native/ScreenRecorder/CapturePerformance.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <random>
#include <chrono>
#include <sstream>

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
static void original(const void* source,std::size_t stride,std::vector<uint32_t>& out,
                     int w,int h,int left,int top,int cw,int ch,uint32_t bg,double sourceRadius){
    out.assign(static_cast<std::size_t>(w)*h,bg);if(cw<=0||ch<=0)return;
    const double scale=std::min(double(w)/cw,double(h)/ch);
    const int dw=std::max(1,int(cw*scale)),dh=std::max(1,int(ch*scale));
    const int ox=(w-dw)/2,oy=(h-dh)/2;const double radius=sourceRadius*scale;
    std::vector<int> sourceX(static_cast<std::size_t>(dw));
    for(int x=0;x<dw;++x)sourceX[x]=left+std::min(cw-1,int(x/scale));
    for(int y=0;y<dh;++y){
        const auto* row=reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(source)+(top+std::min(ch-1,int(y/scale)))*stride);
        auto* dst=out.data()+static_cast<std::size_t>(oy+y)*w+ox;
        if(dw==cw)std::memcpy(dst,row+left,static_cast<std::size_t>(dw)*4);
        else for(int x=0;x<dw;++x)dst[x]=row[sourceX[x]];
        if(radius>0&&(y<radius||y>=dh-radius)){
            const int corner=std::min(dw,int(std::ceil(radius)));
            for(int side=0;side<2;++side)for(int i=0;i<corner;++i){
                const int x=side?dw-1-i:i;
                const double dx=std::max({radius-x-.5,0.0,x+.5-(dw-radius)});
                const double dy=std::max({radius-y-.5,0.0,y+.5-(dh-radius)});
                if(dx*dx+dy*dy>radius*radius)dst[x]=bg;
            }
        }
    }
}

int main(){
    {
        CapturePerformance off(false);{CapturePerformance::Scope scope(&off,CapturePerformance::compose);}
        std::ostringstream out;off.write(out);assert(out.str().find("compose calls=0")!=std::string::npos);
        CapturePerformance on(true);{CapturePerformance::Scope scope(&on,CapturePerformance::compose);}
        out.str("");on.write(out);assert(out.str().find("compose calls=1")!=std::string::npos);
    }
    CaptureFrameComposer composer;std::vector<uint32_t> actual,expected;
    std::mt19937 random(123456);int cases=0;
    for(int cw=1;cw<=9;++cw)for(int ch=1;ch<=9;++ch)for(double radius:{0.,1.,28.,112.}){
        const int stride=cw+5;std::vector<uint32_t> source(static_cast<std::size_t>(stride)*(ch+1),0xffabcdef);
        for(const auto dimensions:{std::pair{2,2},std::pair{9,15},std::pair{15,9}}){
            const auto [w,h]=dimensions;
            original(source.data(),stride*4,expected,w,h,2,1,cw,ch,0xff112233,radius);
            composer.render(source.data(),stride*4,actual,w,h,2,1,cw,ch,0xff112233,radius);
            assert(actual==expected);++cases;
        }
    }
    for(int i=0;i<160;++i){
        const int cw=1+int(random()%1800),ch=1+int(random()%1100),left=int(random()%15),top=int(random()%15);
        const int stride=cw+left+int(random()%31),w=2+int(random()%1920),h=2+int(random()%1080);
        std::vector<uint32_t> source(static_cast<std::size_t>(stride)*(ch+top));
        for(auto& p:source)p=random();
        const double radius=i%3==0?0:28.0*(1+i%4);const uint32_t bg=random();
        original(source.data(),stride*4,expected,w,h,left,top,cw,ch,bg,radius);
        assert(composer.render(source.data(),stride*4,actual,w,h,left,top,cw,ch,bg,radius));
        assert(actual==expected);++cases;
        const auto builds=composer.geometryBuildCount(),capacity=actual.capacity();
        source[static_cast<std::size_t>(top)*stride+left]^=0x12345678;
        original(source.data(),stride*4,expected,w,h,left,top,cw,ch,bg^0x00ffffff,radius);
        composer.render(source.data(),stride*4,actual,w,h,left,top,cw,ch,bg^0x00ffffff,radius);
        assert(actual==expected&&builds==composer.geometryBuildCount()&&capacity==actual.capacity());
    }
    assert(!composer.render(nullptr,0,actual,12,14,0,0,0,0,0xff123456,0));
    assert(actual==std::vector<uint32_t>(12*14,0xff123456));
    // Relative timings are reported, not asserted (CI machines vary).
    for(const int size:{1920,2560}){
        const int sh=size*9/16;std::vector<uint32_t> source(static_cast<std::size_t>(size)*sh,0xff123456);
        const auto time=[&](auto draw){const auto t=std::chrono::steady_clock::now();for(int n=0;n<100;++n)draw();return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();};
        const auto old=time([&]{original(source.data(),size*4,expected,1920,1080,0,0,size,sh,0xff111111,28);});
        const auto now=time([&]{composer.render(source.data(),size*4,actual,1920,1080,0,0,size,sh,0xff111111,28);});
        assert(actual==expected);std::cout<<"sourceWidth="<<size<<" originalMs="<<old<<" cachedMs="<<now<<"\n";
    }
    std::cout<<"PASS "<<cases<<" crop/fit/stride/rounding/resize cases plus dynamic theme/pixel updates; exact pixels\n";
}
