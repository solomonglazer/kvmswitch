#include "backend.h"
namespace {
class Unsupported final : public kvm::Backend {
    std::vector<kvm::Monitor> discover() override {
        throw kvm::Error(kvm::Code::backend,"Hardware commands require 64-bit Windows and an NVIDIA driver.");
    }
    std::string probe(const kvm::Monitor&) override { return {}; }
    void send(const kvm::Monitor&,const kvm::Packet&) override {
        throw kvm::Error(kvm::Code::backend,"Unsupported platform.");
    }
};
}
std::unique_ptr<kvm::Backend> make_backend() { return std::make_unique<Unsupported>(); }
