#pragma once
// Estado compartido entre el hilo de trabajo (descargas) y la interfaz.
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <cstdint>

struct TaskCtx {
    std::atomic<bool> cancel{false};
    std::atomic<uint64_t> done{0};
    std::atomic<uint64_t> total{0};

    void setStatus(const std::string& s) {
        std::lock_guard<std::mutex> l(m_);
        status_ = s;
    }
    std::string status() {
        std::lock_guard<std::mutex> l(m_);
        return status_;
    }
    void reset() { done = 0; total = 0; }

    // Para net::download
    std::function<bool(double, double)> dlProgress() {
        return [this](double now, double tot) {
            done = (uint64_t)now;
            total = (uint64_t)tot;
            return !cancel.load();
        };
    }
    // Para zip::extract
    std::function<bool(uint64_t, uint64_t)> exProgress() {
        return [this](uint64_t now, uint64_t tot) {
            done = now;
            total = tot;
            return !cancel.load();
        };
    }

private:
    std::mutex m_;
    std::string status_;
};
