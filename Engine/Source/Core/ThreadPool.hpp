// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

#include <thread>
#include <mutex>
#include <future>
#include <utility>
#include <queue>
#include <algorithm>
#include <atomic>
#include <functional>

namespace Surge
{
    class SURGE_API ThreadPool
    {
    public:
        ThreadPool(Uint threadCount = std::thread::hardware_concurrency());
        ~ThreadPool();

        Uint GetThreadCount() { return mThreadCount; }

		template <typename T, typename F>
		void ParallelizeLoop(T firstIndex, T lastIndex, const F& loop, Uint numTasks = 0)
		{
			if (numTasks == 0)
				numTasks = mThreadCount;
			if (lastIndex < firstIndex)
				std::swap(lastIndex, firstIndex);
			size_t totalSize = lastIndex - firstIndex + 1;
			size_t blockSize = totalSize / numTasks;
			if (blockSize == 0)
			{
				blockSize = 1;
				numTasks = std::max((Uint)1, (Uint)totalSize);
			}
			std::atomic<Uint> blocks_running = 0;
			for (Uint t = 0; t < numTasks; t++)
			{
				T start = (T)(t * blockSize + firstIndex);
				T end = (t == numTasks - 1) ? lastIndex : (T)((t + 1) * blockSize + firstIndex - 1);
				blocks_running++;
				PushTask([&start, &end, &loop, &blocks_running] {
					for (T i = start; i <= end; i++)
						loop(i);
					blocks_running--;
				});
				while (blocks_running != 0)
				{
					std::this_thread::yield();
				}
			}
		}

		template <typename F>
		void PushTask(const F& task)
		{
			mTasksWaiting++;
			{
				const std::scoped_lock lock(mQueueMutex);
				mTasks.push(std::move(std::function<void()>(task)));
			}
		}

		template <typename F, typename... A>
		void PushTask(const F& task, const A &... args)
		{
			PushTask([task, args...]{ task(args...); });
		}

		void Reset(Uint threadCount = std::thread::hardware_concurrency());

		template <typename F, typename... A, typename E = std::enable_if_t<std::is_void_v<std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>>>>
		std::future<bool> Submit(const F& task, const A &... args)
		{
			std::shared_ptr<std::promise<bool>> promise(new std::promise<bool>);
			std::future<bool> future = promise->get_future();
			PushTask([task, args..., promise]{
				task(args...);
				promise->set_value(true);
			});
			return future;
		}

		template <typename F, typename... A, typename R = std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>, typename = std::enable_if_t<!std::is_void_v<R>>>
		std::future<R> Submit(const F& task, const A &... args)
		{
			std::shared_ptr<std::promise<R>> promise = std::make_shared<std::promise<R>>();
			std::future<R> future = promise->get_future();
			PushTask([task, args..., promise]{
				promise->set_value(task(args...));
			});
			return future;
		}

		void WaitForTasks();
    private:
        void CreateThreads();
        void DestroyThreads();

        bool PopTask(std::function<void()>& task);
        void Worker();

        std::atomic<bool> mRunning = true;
        std::atomic<Uint> mTasksWaiting = 0;
        mutable std::mutex mQueueMutex;
        std::queue<std::function<void()>> mTasks;
        Uint mThreadCount;
        std::unique_ptr<std::thread[]> mThreads;
    };
}