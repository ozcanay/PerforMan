#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <chrono>

using namespace std;

static inline void stick_this_thread_to_core(int core_id);
static inline void* incrementLoop(void* arg);

struct BenchmarkData {
    long long iteration_count;
    int core_id;
};

pthread_barrier_t g_barrier;

int main(int argc, char** argv)
{
    if(argc != 3) {
        cout << "Usage: ./a.out <core_id> <core_id>" << endl;
        return EXIT_FAILURE;
    }

    cout << "================================================ STARTING ================================================" << endl;

    int core1 = std::stoi(argv[1]);
    int core2 = std::stoi(argv[2]);

    pthread_barrier_init(&g_barrier, nullptr, 2);

    const long long iteration_count = 100'000'000'000;

    BenchmarkData benchmark_data1{iteration_count, core1};
    BenchmarkData benchmark_data2{iteration_count, core2};

    pthread_t worker1, worker2;
    pthread_create(&worker1, nullptr, incrementLoop, static_cast<void*>(&benchmark_data1));
    cout << "Created worker1" << endl;
    pthread_create(&worker2, nullptr, incrementLoop, static_cast<void*>(&benchmark_data2));
    cout << "Created worker2" << endl;

    pthread_join(worker1, nullptr);
    cout << "Joined worker1" << endl;
    pthread_join(worker2, nullptr);
    cout << "Joined worker2" << endl;

    return EXIT_SUCCESS;
}

static inline void stick_this_thread_to_core(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores) {
        cerr << "Core " << core_id << " is out of assignable range.\n";
        return;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();

    int res = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

    if(res == 0) {
        cout << "Thread bound to core " << core_id << " successfully." << endl;
    } else {
        cerr << "Error in binding this thread to core " << core_id << '\n';
    }
}

static inline void* incrementLoop(void* arg)
{
    BenchmarkData* arg_ = static_cast<BenchmarkData*>(arg);
    int core_id = arg_->core_id;
    long long iteration_count = arg_->iteration_count;

    stick_this_thread_to_core(core_id);

    cout << "Thread bound to core " << core_id << " will now wait for the barrier." << endl;
    pthread_barrier_wait(&g_barrier);
    cout << "Thread bound to core " << core_id << " is done waiting for the barrier." << endl;

    long long data = 0; /// if I do not mark this variable as volatile, compiling with -Ofast will completely eliminate this for loop.
    long long i;

    cout << "Thread bound to core " << core_id << " will now increment private data " << iteration_count / 1'000'000'000.0 << " billion times." << endl;
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for(i = 0; i < iteration_count; ++i) {
        ++data;
        __asm__ volatile("": : :"memory");
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    unsigned long long elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

    cout << "Elapsed time: " << elapsed_time << " ms, core: " << core_id << ", iteration_count: " << iteration_count << ", data value: " << data << ", i: " << i << endl;

    return nullptr;
}
