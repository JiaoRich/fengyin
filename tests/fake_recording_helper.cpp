// Test fixture only, never packaged. These bytes are deliberately NOT media;
// real PCM/H264 acceptance is checked independently by test_screen_capture_mux.
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
namespace fs=std::filesystem;
void write(const fs::path& p,const std::string& value){std::ofstream out(p,std::ios::binary);out<<value;}
int wmain(int argc,wchar_t** argv){
    if(argc<3)return 2;
    const std::wstring action=argv[1];
    if(action==L"--mux-session"){
        const fs::path folder=argv[2];write(folder/L"recording.mp4",std::string(4096,'m'));
        if(fs::exists(folder/L"fail-once")){fs::remove(folder/L"fail-once");write(folder/L"state.txt","failed: fixture failure");return 1;}
        write(folder/L"recording.wav",std::string(4096,'w'));write(folder/L"state.txt","saved");return 0;
    }
    const bool audio=action==L"audio";
    if(!audio&&(argc<6||std::wstring(argv[5])!=L"1800"))return 2;
    const fs::path folder=audio?fs::path(argv[2]).parent_path():fs::path(argv[3]);
    if(!audio)fs::create_directory(folder);
    const fs::path stop=audio?fs::path(argv[3]):folder/L"stop";
    const fs::path state=audio?fs::path(argv[4]):folder/L"state.txt";
    const bool armed=audio?argc==9:argc==7;
    const fs::path begin=audio&&armed?fs::path(argv[8]):folder/L"begin";
    if(armed){
        write(state,"ready");
        for(int n=0;n<1000&&!fs::exists(begin)&&!fs::exists(stop);++n){
            if(fs::exists(folder/L"inject-failure")){
                fs::remove(folder/L"inject-failure");write(state,"failed: injected preparation failure");return 1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if(fs::exists(stop)){write(state,"cancelled");return 0;}
        if(!fs::exists(begin)){write(state,"failed: preparation timeout");return 1;}
    }
    write(state,"recording");
    for(int n=0;n<6000&&!fs::exists(stop);++n){
        if(fs::exists(folder/L"inject-failure")){
            fs::remove(folder/L"inject-failure");write(folder/L"audio.wav",std::string(4096,'a'));
            write(state,"failed: injected encoder failure");return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if(!fs::exists(stop)){write(state,"failed: fixture timeout");return 1;}
    if(audio){write(fs::path(argv[2]),std::string(4096,'a'));write(state,"saved");}
    else{write(folder/L"audio.wav",std::string(4096,'a'));write(folder/L"video.partial.mp4",std::string(4096,'v'));
        write(folder/L"alignment.txt","0 20000000");write(state,"captured; test fixture");}
    return 0;
}
