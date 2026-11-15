#pragma once
namespace fengyin
{
struct DjSourceProfile
{
    bool enabled=false;
    float trackGain=1, hardwareGain=1;
    bool directToMaster=false;
    friend bool operator==(const DjSourceProfile&,const DjSourceProfile&)=default;
};
}
