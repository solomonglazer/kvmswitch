#include "backend.h"
#include <iostream>
int main(int argc, char** argv) {
    try {
        const auto options=kvm::parse(std::vector<std::string>(argv+1,argv+argc));
        auto backend=make_backend();
        std::cout<<kvm::execute(options,*backend);
        return 0;
    } catch(const kvm::Error& e) {
        std::cerr<<(e.code==kvm::Code::write?"status=write-failed error=":"status=refused error=")
            <<e.what()<<'\n'; return static_cast<int>(e.code);
    } catch(const std::exception& e) {
        std::cerr<<"status=error error="<<e.what()<<'\n'; return 8;
    }
}
