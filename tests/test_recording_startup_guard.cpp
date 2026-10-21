#include "../native/Source/RecordingStartupGuard.h"
#include "../native/Source/RecordingLimits.h"
#include <stdexcept>
void require(bool ok){if(!ok)throw std::runtime_error("Recording startup guard regression");}
int main(){
    require(fengyin::recordingMaxSeconds==1800);
    require(!fengyin::recordingLimitReached(17999999999LL));
    require(fengyin::recordingLimitReached(18000000000LL));
    require(fengyin::recordingLimitReached(18000000001LL));
    fengyin::RecordingStartupGuard guard;
    require(!guard.timedOut(20000,false,false,true));
    require(guard.timedOut(20001,false,false,true));
    require(!guard.timedOut(60000,false,true,true)); // early helper failure wins
    require(!guard.timedOut(60000,false,false,false)); // exited helper, not timeout
    require(!guard.timedOut(60000,true,false,true)); // save has its own deadline
    guard.observe(true);
    for(int n=0;n<21600;++n){
        guard.observe(false); // temporary missing state must not regress
        require(!guard.timedOut(n*1000.0,false,false,true));
    }
    require(!guard.timedOut(60000,false,true,true)); // mid-recording failure
    guard.reset(); // begin after preparation starts a new handshake
    require(guard.timedOut(20001,false,false,true));
    guard.observe(true);
    require(!guard.timedOut(21600000,false,false,true));
}
