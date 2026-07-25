#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>

namespace EnvironmentalEngine {
	class ThreadPool {
	public:
		ThreadPool(size_t n) {
			for (size_t i = 0; i < n; i++) {
				workers.emplace_back([this] {
					while (true) {
						std::function<void()> task;
						{
							std::unique_lock<std::mutex> lock(queueMutex);
							cv.wait(lock, [this] { return stop || !tasks.empty(); });
							if (stop && tasks.empty()) return;
							task = std::move(tasks.front());
							tasks.pop();
						}
						task();
					}
					});
			}
		}
		~ThreadPool() {
			{
				std::lock_guard<std::mutex> lock(queueMutex);
				stop = true;
			}

			cv.notify_all();

			for (std::thread& w : workers) {
				w.join();
			}
		}

		template<class F, class... Args>
		auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
			using Ret = std::invoke_result_t< F, Args... > ;
			auto task = std::make_shared<std::packaged_task<Ret()>>(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...));
			std::future<Ret> fut = task->get_future();
			{
				std::lock_guard<std::mutex> lock(queueMutex);
				tasks.emplace([task] { (*task)(); });
			}
			cv.notify_one();
			return fut;
		}
		
	private:
		std::vector<std::thread> workers;
		std::queue<std::function<void()>> tasks;
		std::mutex queueMutex;
		std::condition_variable cv;
		bool stop = false;
	};
}