#pragma once

#include <boost/asio.hpp>

#include <memory>
#include <thread>
#include <vector>

namespace wsgi_boost {
    typedef std::shared_ptr<boost::asio::io_service> io_service_ptr;

    class io_service_pool {
    private:
        std::vector<io_service_ptr> m_io_services;
        std::vector<boost::asio::io_service::work> m_works;
        std::vector<std::thread> m_threads;
        unsigned int m_next_io_service = 0;

    public:
        explicit io_service_pool(unsigned int pool_size = 0) {
            if (pool_size == 0) {
                unsigned int threads_hint = std::thread::hardware_concurrency();
                if (threads_hint > 0)
                    pool_size = threads_hint;
                else
                    pool_size = 1;
            }
            for (unsigned int i = 0; i < pool_size; ++i) {
                io_service_ptr io_service = std::make_shared<boost::asio::io_service>(1);
                auto work = boost::asio::io_service::work{*io_service};
                m_io_services.emplace_back(io_service);
                m_works.emplace_back(work);
            }
        }

        io_service_pool(const io_service_pool&) = delete;
        io_service_pool& operator=(const io_service_pool&) = delete;

        // Run IO services
        void run() {
            m_threads.clear();
            for (size_t i = 1; i < m_io_services.size(); ++i) {
                m_threads.emplace_back([this, i]() {
                    this->m_io_services[i]->run();
                });
            }
            m_io_services[0]->run();
            for (auto& t : m_threads)
                t.join();
        }

        // Stop IO services
        void stop() {
            for (auto& io : m_io_services)
                io->stop();
        }

        // Reset IO services
        void reset() {
            for (auto& io : m_io_services) {
                if (io->stopped())
                    io->reset();
            }
        }

        // Get IO service
        io_service_ptr get_io_service() {
            io_service_ptr io_service = m_io_services[m_next_io_service];
            ++m_next_io_service;
            if (m_next_io_service == m_io_services.size())
                m_next_io_service = 0;
            return io_service;
        }

        // Get pool size
        size_t size() const { return m_io_services.size(); }
    };
} // namespace wsgi_boost
