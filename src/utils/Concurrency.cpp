//
// Created by Yeo Shu Heng on 17/6/25.
//

#include <thread>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_policy.h>
#elif __linux__
#include <pthread.h>
#include <sched.h>
#endif

void spin_wait(std::chrono::nanoseconds target_duration) {
    using clock = std::chrono::steady_clock;
    auto start = clock::now();
    while (clock::now() - start < target_duration) {
    }
}

void set_affinity(std::thread &to_pin, int core) {
#ifdef __APPLE__
    thread_affinity_policy policy;
    policy.affinity_tag = core;
    const thread_port_t mach_thread = mach_thread_self();
    thread_policy_set(mach_thread,
        THREAD_AFFINITY_POLICY,
        reinterpret_cast<thread_policy_t>(&policy),
        THREAD_AFFINITY_POLICY_COUNT);
    mach_port_deallocate(mach_task_self(), mach_thread);
#elif __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    pthread_t pthread_id = to_pin.native_handle();
    int rc = pthread_setaffinity_np(pthread_id, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        perror("pthread_setaffinity_np");
    }
#else
    (void)to_pin; (void)core;
#endif
}
