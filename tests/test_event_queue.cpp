#include "broapps/event_queue.h"
#include "test_common.h"

#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>

int main() {
    using namespace broapps;

    // Test 1: Basic push and drain
    {
        MessageQueue<int> q;
        TEST_CHECK(q.empty());
        TEST_CHECK_EQ(q.size(), 0u);

        q.push(10);
        q.push(20);
        q.push(30);

        TEST_CHECK(!q.empty());
        TEST_CHECK_EQ(q.size(), 3u);

        auto items = q.drain();
        TEST_CHECK_EQ(items.size(), 3u);
        TEST_CHECK_EQ(items[0], 10);
        TEST_CHECK_EQ(items[1], 20);
        TEST_CHECK_EQ(items[2], 30);

        TEST_CHECK(q.empty());
        TEST_CHECK_EQ(q.size(), 0u);
    }

    // Test 2: Wake callback
    {
        MessageQueue<std::string> q;
        std::atomic<int> wake_count{0};

        q.set_wake([&wake_count]() {
            wake_count.fetch_add(1);
        });

        q.push("hello");
        q.push("world");

        TEST_CHECK_EQ(wake_count.load(), 2);
        auto items = q.drain();
        TEST_CHECK_EQ(items.size(), 2u);
    }

    // Test 3: wait_for
    {
        MessageQueue<int> q;

        // Should time out on empty
        auto start = std::chrono::steady_clock::now();
        bool ok = q.wait_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        TEST_CHECK(!ok);
        TEST_CHECK(elapsed.count() >= 40);

        // Producer thread pushes after 30ms
        std::thread producer([&q]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            q.push(42);
        });

        ok = q.wait_for(std::chrono::milliseconds(500));
        TEST_CHECK(ok);
        auto items = q.drain();
        TEST_CHECK_EQ(items.size(), 1u);
        TEST_CHECK_EQ(items[0], 42);

        producer.join();
    }

    // Test 4: Multi-producer concurrent pushes
    {
        MessageQueue<int> q;
        const int num_producers = 4;
        const int items_per_producer = 250;
        std::vector<std::thread> producers;

        for (int p = 0; p < num_producers; ++p) {
            producers.emplace_back([&q, p]() {
                for (int i = 0; i < items_per_producer; ++i) {
                    q.push(p * 1000 + i);
                }
            });
        }

        for (auto& t : producers) {
            t.join();
        }

        auto drained = q.drain();
        TEST_CHECK_EQ(drained.size(), static_cast<size_t>(num_producers * items_per_producer));
    }

    std::cout << "test_event_queue passed!" << std::endl;
    return 0;
}
