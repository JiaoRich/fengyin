"""Compile and execute the production route planner and rollback transaction."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class RealtekOutputPolicy(unittest.TestCase):
    def test_policy_and_every_partial_write_failure(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        program = r'''
#include "audio-engine/windows/RealtekOutputPolicy.h"
#include <cassert>
#include <map>
#include <tuple>
using namespace fengyin::audioengine;
int main() {
 constexpr unsigned E=0x80000000u;
 std::vector<AsioRouteNode> nodes {
  {0,-1,-1,E,false,false,true}, {0,0,-1,E,false,false,true},
  {0,0,0,E,false,true,true}, // previously selected virtual cable
  {1,-1,-1,8,true,false,true}, {1,0,-1,8,true,false,true},
  {1,0,0,8,true,true,true}, {1,0,1,8,true,true,true}, // arbitrary output IDs
  {1,0,2,E|8,true,false,true}, // input must not stay enabled
  {1,0,3,8,true,true,false}, // unplugged output is not enabled
  {2,-1,-1,0,false,false,true}, {2,0,-1,0,false,false,true},
  {2,0,0,0,false,true,true} // HDMI not selected
 };
 std::vector<AsioRouteChange> plan;
 assert(planRealtekOutputs(nodes,E,plan));
 using Key=std::tuple<long,long,long>;
 auto key=[](const AsioRouteNode& n){return Key(n.device,n.interfaceIndex,n.pin);};
 std::map<Key,unsigned> original;
 for(auto n:nodes) original[key(n)]=n.flags;
 for(int fail=-1;fail<(int)plan.size();++fail) {
  auto state=original;int writes=0;bool rollback=false;
  auto write=[&](auto n,unsigned value){state[key(n)]=value;return writes++!=fail;};
  auto read=[&](auto n,unsigned& value){value=state[key(n)];return true;};
  bool ok=applyRealtekPlan(plan,write,read,rollback);
  assert(rollback);
  if(fail>=0) {assert(!ok);assert(state==original);}
  else {
   assert(ok);
   for(auto& n:nodes) {
    n.flags=state[key(n)];
    if(n.pin>=0) assert(bool(n.flags&E)==(n.realtek&&n.output&&n.available));
   }
   std::vector<AsioRouteChange> again;
   assert(planRealtekOutputs(nodes,E,again)&&again.empty()); // idempotent
   for(auto& n:nodes) n.flags=original[key(n)];
  }
 }
 auto missing=nodes;
 for(auto& n:missing) n.realtek=false;
 assert(!planRealtekOutputs(missing,E,plan)&&plan.empty());
 // A newly enumerated headphone is enabled; a removed one is not retained.
 nodes.back()={1,0,4,0,true,true,true};
 assert(planRealtekOutputs(nodes,E,plan));
 bool found=false;for(auto c:plan) if(c.node.pin==4) found=bool(c.desired&E);
 assert(found);
 bool rollback=true;
 assert(!applyRealtekPlan(plan,[](auto,unsigned){return false;},
   [](auto,unsigned&){return false;},rollback));
 assert(!rollback); // never silently claim a failed rollback succeeded
}
'''
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / 'policy.cpp'
            binary = Path(temp) / 'policy'
            source.write_text(program, encoding='utf-8')
            subprocess.run([compiler, '-std=c++17', '-I', str(ROOT), str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    unittest.main()
