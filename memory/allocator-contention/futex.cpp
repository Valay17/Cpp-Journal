#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <thread>
#include <vector>
#include <sys/resource.h>

/*
SpscQueue is a lock-free single-producer single-consumer ring buffer: one
thread only calls push, one thread only calls pop, and each side talks to
the other through one atomic index. Nothing in it ever blocks.

The program sends 1,000,000 nodes through it from a producer thread to a
consumer thread, in one of two ways.

"malloc" is the usual way. The producer builds each node with new and
pushes the pointer, and the consumer pops it and calls delete. Every node
is allocated on one thread and freed on the other.

"pool" is the fix. All the nodes the queue can hold are allocated once
before the threads start, and the consumer hands each node back through a
second queue instead of calling delete. The producer takes its next node
from that second queue. After startup neither thread calls the allocator.

Node<N> is exactly N bytes, so the node size is a command-line argument.
The consumer adds up the values it receives and the program checks the sum,
so a run that lost or repeated a node would fail. Voluntary context
switches come from getrusage: a thread that sleeps on a lock gives up the
CPU voluntarily, which makes this a count of waits that needs no tracing.
*/

template <class T, size_t Cap>
class SpscQueue {
    T slots[Cap];
    alignas(64) std::atomic<size_t> head{0};   // consumer advances
    alignas(64) std::atomic<size_t> tail{0};   // producer advances
public:
    bool push(T v) {
        size_t t = tail.load(std::memory_order_relaxed);
        if (t - head.load(std::memory_order_acquire) == Cap) return false;
        slots[t % Cap] = v;
        tail.store(t + 1, std::memory_order_release);
        return true;
    }
    bool pop(T& v) {
        size_t h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        v = slots[h % Cap];
        head.store(h + 1, std::memory_order_release);
        return true;
    }
};

constexpr size_t kCap = 1024;
constexpr long kCount = 1'000'000;

template <size_t N>
struct Node {
    long value;
    char pad[N - sizeof(long)];
};

template <size_t N>
long run(bool use_pool) {
    using NodeT = Node<N>;
    static SpscQueue<NodeT*, kCap> queue;
    static SpscQueue<NodeT*, kCap + 2> free_nodes;   // nodes the consumer hands back

    // Every node that can be in flight: kCap in the queue, one held by each thread.
    std::vector<NodeT*> storage;
    if (use_pool) {
        for (size_t i = 0; i < kCap + 2; ++i) {
            NodeT* n = static_cast<NodeT*>(::operator new(sizeof(NodeT)));
            storage.push_back(n);
            free_nodes.push(n);
        }
    }

    long sum = 0;
    std::thread consumer([&] {
        for (long i = 0; i < kCount; ++i) {
            NodeT* n;
            while (!queue.pop(n)) {}
            sum += n->value;
            if (use_pool) {
                n->~NodeT();
                while (!free_nodes.push(n)) {}   // hand the node back, no delete
            } else {
                delete n;                        // free from another thread
            }
        }
    });
    std::thread producer([&] {
        for (long i = 0; i < kCount; ++i) {
            NodeT* n;
            if (use_pool) {
                while (!free_nodes.pop(n)) {}    // no allocator call
                new (n) NodeT{i, {}};            // construct in place
            } else {
                n = new NodeT{i, {}};            // new calls malloc
            }
            while (!queue.push(n)) {}
        }
    });
    producer.join();
    consumer.join();

    for (NodeT* n : storage) ::operator delete(n);
    return sum;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <malloc|pool> <64|128|256|512>\n", argv[0]); return 1; }
    bool use_pool = std::string(argv[1]) == "pool";
    int bytes = atoi(argv[2]);

    long sum;
    switch (bytes) {
        case 64:  sum = run<64>(use_pool);  break;
        case 128: sum = run<128>(use_pool); break;
        case 256: sum = run<256>(use_pool); break;
        case 512: sum = run<512>(use_pool); break;
        default: fprintf(stderr, "node size must be 64, 128, 256 or 512\n"); return 1;
    }

    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    long expected = kCount * (kCount - 1) / 2;
    printf("%s, %d-byte nodes: sum %s, voluntary context switches %ld\n",
           argv[1], bytes, sum == expected ? "ok" : "WRONG", ru.ru_nvcsw);
}
