#pragma once
#include "core.h"
#include <memory>
std::unique_ptr<kvm::Backend> make_backend();
