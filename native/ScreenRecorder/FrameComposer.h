#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Same nearest-neighbour crop/fit and rounded mask as the original recorder.
// Only geometry is cached, never window pixels. Stable-size frames allocate no
// storage, and opaque picture pixels are not first overwritten with background.
class CaptureFrameComposer {
public:
    bool render(const void* source,std::size_t stride,std::vector<uint32_t>& output,
                int w,int h,int left,int top,int cw,int ch,uint32_t bg,double sourceRadius) {
        if(w<=0||h<=0)return false;
        output.resize(static_cast<std::size_t>(w)*h);
        if(!source||cw<=0||ch<=0){std::fill(output.begin(),output.end(),bg);return false;}
        const Geometry next{w,h,left,top,cw,ch,sourceRadius};
        if(!validGeometry||!(geometry==next))prepare(next);
        std::fill_n(output.data(),static_cast<std::size_t>(oy)*w,bg);
        std::fill(output.begin()+static_cast<std::ptrdiff_t>(oy+dh)*w,output.end(),bg);
        const auto* bytes=static_cast<const unsigned char*>(source);
        for(int y=0;y<dh;++y){
            const auto& r=rows[static_cast<std::size_t>(y)];
            auto* dst=output.data()+static_cast<std::size_t>(oy+y)*w;
            const auto* src=reinterpret_cast<const uint32_t*>(bytes+static_cast<std::size_t>(r.sourceY)*stride);
            auto* picture=dst+ox;
            const auto* map=sourceX.data();
            if(dw==cw)std::memcpy(picture,src+left,static_cast<std::size_t>(dw)*4);
            else copyScaledRow(picture,src,map,dw);
            std::fill_n(dst,ox+r.begin,bg);
            std::fill(dst+ox+r.end,dst+w,bg);
        }
        return true;
    }
    std::size_t geometryBuildCount() const noexcept{return builds;}
private:
    // Capture staging memory and the two privately owned vectors cannot alias.
    // Make that fact visible to the compiler just as the original per-frame
    // freshly allocated x-map did; otherwise cached maps can inhibit SIMD.
    static void copyScaledRow(uint32_t* __restrict dst,const uint32_t* __restrict src,
                              const int* __restrict map,int count) noexcept {
        for(int x=0;x<count;++x)dst[x]=src[map[x]];
    }
    struct Geometry {
        int w,h,left,top,cw,ch;double radius;
        bool operator==(const Geometry&) const=default;
    } geometry{};
    struct Row {int sourceY,begin,end;};
    bool validGeometry=false;
    int dw=0,dh=0,ox=0,oy=0;
    std::size_t builds=0;
    std::vector<int> sourceX;
    std::vector<Row> rows;
    void prepare(Geometry next){
        geometry=next;validGeometry=true;++builds;
        const auto [w,h,left,top,cw,ch,sourceRadius]=next;
        const double scale=std::min(double(w)/cw,double(h)/ch);
        dw=std::max(1,int(cw*scale));dh=std::max(1,int(ch*scale));
        ox=(w-dw)/2;oy=(h-dh)/2;
        const double radius=sourceRadius*scale;
        sourceX.resize(static_cast<std::size_t>(dw));rows.resize(static_cast<std::size_t>(dh));
        for(int x=0;x<dw;++x)sourceX[static_cast<std::size_t>(x)]=left+std::min(cw-1,int(x/scale));
        for(int y=0;y<dh;++y){
            int begin=0,end=dw;
            if(radius>0&&(y<radius||y>=dh-radius)){
                const auto outside=[&](int x){
                    const double dx=std::max({radius-x-.5,0.0,x+.5-(dw-radius)});
                    const double dy=std::max({radius-y-.5,0.0,y+.5-(dh-radius)});
                    return dx*dx+dy*dy>radius*radius;
                };
                // Original mask only visited corner pixels. Preserve that
                // boundary exactly even for unusually small/collapsed windows.
                const int corner=std::min(dw,static_cast<int>(std::ceil(radius)));
                while(begin<corner&&outside(begin))++begin;
                while(end>begin&&end>dw-corner&&outside(end-1))--end;
            }
            rows[static_cast<std::size_t>(y)]={top+std::min(ch-1,int(y/scale)),begin,end};
        }
    }
};
