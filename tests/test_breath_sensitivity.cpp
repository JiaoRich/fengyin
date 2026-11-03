#include "BreathSensitivity.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    using B = fengyin::BreathSensitivity;
    for (int s=-100;s<=100;++s)
    {
        assert(B::apply(0,s)==0 && B::apply(1,s)==1);
        float previous=-1;
        for(int i=0;i<=10000;++i)
        {
            const auto x=i/10000.0f,y=B::apply(x,s);
            assert(std::isfinite(y) && y>=0 && y<=1 && y+1e-6f>=previous);
            if(s==0 || x<=.1f)assert(y==x);
            if(s>0)assert(y+1e-6f>=x);if(s<0)assert(y<=x+1e-6f);
            if(s<100)assert(B::apply(x,s+1)+1e-6f>=y);
            previous=y;
        }
        for(int i=0;i<127;++i)
        {
            const auto a=B::apply(i/127.0f,s),b=B::apply((i+1)/127.0f,s);
            assert(std::round(b*127)-std::round(a*127)<=3);
        }
        for(float x:{.1f,.35f,.5f,.75f})
        {
            constexpr float e=.0001f;
            const auto left=(B::apply(x,s)-B::apply(x-e,s))/e;
            const auto right=(B::apply(x+e,s)-B::apply(x,s))/e;
            assert(std::abs(left-right)<.02f && left>=-.01f && right>=-.01f);
        }
    }
    for(int i=0;i<128;++i)assert(B::apply(i/127.0f,0)==i/127.0f);
    assert(std::abs(B::apply(.5f,100)-.82f)<1e-6f);
    assert(std::abs(B::apply(.5f,-100)-.35f)<1e-6f);
    assert(B::apply(std::numeric_limits<float>::quiet_NaN(),100)==0);
    assert(B::apply(-1,100)==0 && B::apply(2,100)==1);
    assert(B::apply(.5f,1000)==B::apply(.5f,100));
    assert(B::apply(.5f,-1000)==B::apply(.5f,-100));
    float maximumSlope=0;
    for(int i=1;i<10000;++i)maximumSlope=std::max(maximumSlope,
        (B::apply((i+1)/10000.0f,100)-B::apply(i/10000.0f,100))*10000);
    assert(maximumSlope<2.5f);
    std::cout<<"201 strengths x 10001 inputs; endpoints, monotonicity, identity, quiet toe, C1 joins, MIDI steps; max easy slope="<<maximumSlope<<"\n";
}
