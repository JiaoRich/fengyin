#include "../native/Source/BleDiscoveryQueue.h"
#include <cassert>
int main() {
    using namespace fengyin;
    BleDiscoveryQueue q;
    for(int i=0;i<10000;++i)q.offer({1,0,"",false,double(i)});
    assert(q.size()==1 && q.received==10000 && q.merged==9999);
    q.offer({1,0,"Wind",true,10001});q.offer({1,0,"",false,10002});
    auto device=q.take(11000);assert(device && device->midi && device->name=="Wind" && q.maxWaitMs==11000);
    assert(q.empty());
    for(unsigned i=0;i<128;++i)q.offer({i,0,"",false,0});
    q.offer({999,1,"random",true,1});assert(q.size()==128 && q.dropped==1);
    device=q.take(2);assert(device->address==999 && device->addressType==1);
    q.clear();assert(q.size()==0 && q.received==0 && q.maxWaitMs==0);
    q.offer({7,0,"public",false,1});q.offer({7,1,"random",false,1});assert(q.size()==2);
    assert(q.take(2)->addressType==0);assert(q.take(2)->addressType==1);assert(!q.take(2));
    for(unsigned i=0;i<128;++i)q.offer({i,0,"",true,0});
    q.offer({999,0,"",false,1});assert(q.size()==128 && q.dropped==1);
    for(int i=0;i<128;++i)assert(q.take(2)->midi);
}
