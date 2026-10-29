#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace kvm {
enum class Code { ok=0, usage=2, unsupported=3, ambiguous=4, backend=5, identity=6, write=7, discovery=8 };
struct Error : std::runtime_error {
    Code code;
    Error(Code c, const std::string& message) : std::runtime_error(message), code(c) {}
};
enum class Action { help, list, doctor, switch_usb_c };
struct Options { Action action=Action::help; bool dry_run=false; bool confirm=false; };
struct Monitor {
    std::string source;
    std::wstring device_path;
    std::array<std::uint8_t,128> edid{};
    bool display_port=false;
    bool unique_route=false;
};
struct Packet {
    std::uint8_t device=0x6e;
    std::uint8_t source=0x50;
    std::array<std::uint8_t,6> payload{0x84,0x03,0xf4,0x00,0xd1,0x9c};
};
Options parse(const std::vector<std::string>& args);
bool supported(const Monitor& monitor);
const Monitor& select(const std::vector<Monitor>& monitors);
class Backend {
public:
    virtual ~Backend()=default;
    virtual std::vector<Monitor> discover()=0;
    // Probe only: route, driver and EDID checks; never a monitor-control write.
    virtual std::string probe(const Monitor& monitor)=0;
    virtual void send(const Monitor& monitor, const Packet& packet)=0;
};
std::string execute(const Options& options, Backend& backend);
std::string help();
}
