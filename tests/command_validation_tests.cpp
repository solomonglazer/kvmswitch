#include "core.h"
#include <functional>
#include <iostream>

namespace {
int checks=0;
void check(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
kvm::Monitor lg() {
    kvm::Monitor m;
    m.source="DISPLAY-test"; m.device_path=L"test-monitor";
    m.display_port=true; m.unique_route=true;
    m.edid={0,255,255,255,255,255,255,0,0x1e,0x6d,0xd3,0x77};
    unsigned sum=0; for(auto b:m.edid) sum+=b;
    m.edid[127]=static_cast<std::uint8_t>(0u-sum);
    return m;
}
struct Fake final : kvm::Backend {
    std::vector<kvm::Monitor> monitors{lg()};
    int discoveries=0,probes=0,writes=0;
    bool hotplug=false,fail_probe=false,fail_write=false;
    std::vector<kvm::Monitor> discover() override {
        ++discoveries;
        auto copy=monitors;
        if(hotplug && discoveries>1 && !copy.empty()) copy[0].device_path=L"replacement";
        return copy;
    }
    std::string probe(const kvm::Monitor&) override {
        ++probes;
        if(fail_probe) throw kvm::Error(kvm::Code::backend,"missing NVAPI");
        return "mock route";
    }
    void send(const kvm::Monitor&,const kvm::Packet& p) override {
        ++writes;
        check(p.device==0x6e && p.source==0x50,"Wrong LG addresses");
        check(p.payload==std::array<std::uint8_t,6>{0x84,3,0xf4,0,0xd1,0x9c},"Wrong USB-C command");
        unsigned checksum=p.device^p.source;
        for(auto b:p.payload) checksum^=b;
        check(checksum==0,"Incorrect checksum");
        if(fail_write) throw kvm::Error(kvm::Code::write,"write failure");
    }
};
void refusal(kvm::Code code,const std::function<void()>& action) {
    try { action(); } catch(const kvm::Error& e) { check(e.code==code,"Wrong refusal category"); return; }
    throw std::runtime_error("Expected refusal");
}
void run(Fake& b,std::vector<std::string> args) { kvm::execute(kvm::parse(args),b); }
}
int main() {
    try {
        check(kvm::supported(lg()),"Known EDID rejected");
        auto invalid=lg(); invalid.edid[20]^=1;
        check(!kvm::supported(invalid),"Bad EDID checksum accepted");
        invalid=lg(); invalid.edid[8]=0;
        invalid.edid[127]=static_cast<std::uint8_t>(invalid.edid[127]+0x1e);
        check(!kvm::supported(invalid),"Foreign manufacturer accepted");
        invalid=lg(); invalid.edid[10]=0xd4; --invalid.edid[127];
        check(kvm::supported(invalid),"USB-C product identity rejected");
        invalid=lg(); invalid.edid[10]=0xd5; invalid.edid[127]-=2;
        check(!kvm::supported(invalid),"Unknown product accepted");
        for(const auto& args:std::vector<std::vector<std::string>>{{"list"},{"doctor"},{"switch","usb-c","--dry-run"}}) {
            Fake b; run(b,args); check(b.writes==0,"Read-only command wrote to hardware");
        }
        Fake b;
        refusal(kvm::Code::usage,[&]{run(b,{"switch","usb-c"});});
        check(b.writes==0,"Unconfirmed command wrote");
        for(auto args:std::vector<std::vector<std::string>>{
            {"switch","209"},{"switch","dp"},{"switch","usb-c","--value","210"},
            {"switch","usb-c","--dry-run","--confirm-experimental"},
            {"switch","usb-c","--dry-run","--dry-run"},{"list","--confirm-experimental"}})
            refusal(kvm::Code::usage,[&]{kvm::parse(args);});
        const std::vector<std::string> send{"switch","usb-c","--confirm-experimental"};
        for(int scenario=0;scenario<7;++scenario) {
            Fake f; kvm::Code expected=kvm::Code::identity;
            if(scenario==0) { f.monitors.clear(); expected=kvm::Code::unsupported; }
            if(scenario==1) { f.monitors.push_back(lg()); expected=kvm::Code::ambiguous; }
            if(scenario==2) f.monitors[0].display_port=false;
            if(scenario==3) f.monitors[0].unique_route=false;
            if(scenario==4) f.hotplug=true;
            if(scenario==5) { f.fail_probe=true; expected=kvm::Code::backend; }
            if(scenario==6) { f.monitors[0].edid={}; expected=kvm::Code::unsupported; }
            refusal(expected,[&]{run(f,send);}); check(f.writes==0,"Refusal wrote to hardware");
        }
        Fake good;
        auto result=kvm::execute(kvm::parse(send),good);
        check(good.writes==1 && good.discoveries==2,"Write must occur once after re-enumeration");
        check(result.find("status=sent")!=std::string::npos,"Missing sent result");
        check(result.find("status=switched")==std::string::npos,"Claimed unverified switch");
        Fake failed; failed.fail_write=true;
        refusal(kvm::Code::write,[&]{run(failed,send);});
        check(failed.writes==1,"Failed write was retried");
        std::cout<<checks<<" checks passed; no hardware backend used\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
