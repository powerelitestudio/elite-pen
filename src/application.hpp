#pragma once

#include <memory>

namespace elite_pen::win {

class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace elite_pen::win
