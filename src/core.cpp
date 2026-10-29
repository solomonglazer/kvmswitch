#include "core.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace kvm {
Options parse(const std::vector<std::string>& args) {
    if(args.empty() || args==std::vector<std::string>{"--help"}) return {};
    Options o;
    std::size_t first=1;
    if(args[0]=="list") o.action=Action::list;
    else if(args[0]=="doctor") o.action=Action::doctor;
    else if(args.size()>=2 && args[0]=="switch" && args[1]=="usb-c") {
        o.action=Action::switch_usb_c; first=2;
    } else throw Error(Code::usage,"Expected list, doctor, or switch usb-c; see --help.");
    for(auto i=first;i<args.size();++i) {
        if(o.action!=Action::switch_usb_c) throw Error(Code::usage,"This command takes no flags.");
        if(args[i]=="--dry-run" && !o.dry_run) o.dry_run=true;
        else if(args[i]=="--confirm-experimental" && !o.confirm) o.confirm=true;
        else throw Error(Code::usage,"Unknown or repeated option: "+args[i]);
    }
    if(o.dry_run && o.confirm) throw Error(Code::usage,"Choose dry-run or confirmed test.");
    return o;
}
bool supported(const Monitor& m) {
    constexpr std::array<std::uint8_t,8> header{0,255,255,255,255,255,255,0};
    if(!std::equal(header.begin(),header.end(),m.edid.begin())) return false;
    unsigned sum=0; for(auto b:m.edid) sum+=b;
    // GSM PNP manufacturer, LG 38WR85QC-W product IDs documented by monmux.
    const auto product=static_cast<unsigned>(m.edid[10])|(static_cast<unsigned>(m.edid[11])<<8);
    return sum%256==0 && m.edid[8]==0x1e && m.edid[9]==0x6d &&
        (product==0x77d3 || product==0x77d4);
}
const Monitor& select(const std::vector<Monitor>& monitors) {
    const Monitor* chosen=nullptr;
    for(const auto& m:monitors) if(supported(m)) {
        if(chosen) throw Error(Code::ambiguous,"Multiple matching LG monitors; connect only one for this test.");
        chosen=&m;
    }
    if(!chosen) throw Error(Code::unsupported,"No active LG 38WR85QC-W with validated EDID found.");
    if(!chosen->display_port || !chosen->unique_route)
        throw Error(Code::identity,"Test requires one unambiguous active DisplayPort route (no cloning).");
    return *chosen;
}
std::string execute(const Options& o, Backend& b) {
    if(o.action==Action::help) return help();
    auto monitors=b.discover();
    if(o.action==Action::list) {
        std::ostringstream out;
        for(const auto& m:monitors) out<<m.source<<" model="<<(supported(m)?"LG 38WR85QC-W":"unsupported/unreadable EDID")
            <<" product=0x"<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(m.edid[11])
            <<std::setw(2)<<unsigned(m.edid[10])<<std::dec<<" displayport="<<m.display_port
            <<" unique-route="<<m.unique_route<<'\n';
        return monitors.empty()?"No active monitors.\n":out.str();
    }
    const auto m=select(monitors);
    const auto route=b.probe(m);
    const std::string plan="monitor="+m.source+" model=LG 38WR85QC-W target=usb-c\n"+route+
        "\nI2C device=0x6E source/register=0x50 payload=84 03 F4 00 D1 9C (33 kHz)\n";
    if(o.action==Action::doctor) return plan+"status=ready-for-experimental-test; no monitor-control writes; switching unverified\n";
    if(o.dry_run) return plan+"status=dry-run; no monitor-control writes\n";
    if(!o.confirm) throw Error(Code::usage,"Windows switching is experimental. Run --dry-run, then add --confirm-experimental for one write.");
    // Re-enumerate immediately before writing; reject a hotplug or route change.
    const auto fresh=b.discover(); const auto& now=select(fresh);
    if(now.source!=m.source || now.device_path!=m.device_path || now.edid!=m.edid)
        throw Error(Code::identity,"Monitor identity or route changed; no command sent.");
    b.probe(now);
    b.send(now,Packet{});
    return plan+"status=sent; picture and USB migration require visual verification\n";
}
std::string help() {
    return "kvmswitch 0.1.0 - LG 38WR85QC-W Windows to Mac command test\n"
        "  kvmswitch list\n  kvmswitch doctor\n  kvmswitch switch usb-c --dry-run\n"
        "  kvmswitch switch usb-c --confirm-experimental\n"
        "Keep the Mac awake on USB-C and the monitor joystick accessible.\n"
        "Exit codes: 0 success, 2 usage, 3 unsupported, 4 ambiguous, 5 NVAPI,\n"
        "6 identity/route, 7 write failed, 8 discovery failed.\n";
}
}
