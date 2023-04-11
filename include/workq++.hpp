/* --
 *
 * MIT License
 * 
 * Copyright (c) 2014-2023 Abe Takafumi
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef LIBSHARAKU_WORKQ_PLUSPLUS_HPP
#define LIBSHARAKU_WORKQ_PLUSPLUS_HPP

#include <functional>
#include <deque>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace libsharaku {
namespace workque {

	// 処理優先度の型
	using nice_t = uint32_t;

	//イベント
	class event {
	 private:
		std::function<void(void)> func_ = nullptr;
		nice_t priority = 0;

	 public:
		event() = delete;
		event(event&) = delete;
		event(event&&) = delete;

		template<typename ... Args>
		event(nice_t prio, std::function<void(Args ...)> func, Args ... args) {
			priority = prio;
			func_ = [this, func, args ...]() {
				func(args ...);
			};
		}
		// nice値を取得
		operator nice_t() {
			return priority;
		}
		// 登録された処理を実行
		void operator()() {
			func_();
		}
	};

	namespace __internal__::workque {
		using event = libsharaku::workque::event;

		class workque_fifo_internal___ {
		 protected:
			std::deque<std::shared_ptr<event>> fifo_;
		 public:
			void push(std::shared_ptr<event> ev) {
				fifo_.push_back(ev);
			}
			bool erase(std::shared_ptr<event> &ev) {
				for (auto it = fifo_.begin(); it != fifo_.end(); it ++) {
					if (*it == ev) {
						fifo_.erase(it);
						return true;
					}
				}
				return false;
			}
			std::shared_ptr<event> pop() {
				std::shared_ptr<event> ev = fifo_.front();
				fifo_.pop_front();
				return ev;
			}
			size_t size() {
				size_t sz = fifo_.size();
				return sz;
			}
		};

		class workque_timer_list_internal___ {
		 protected:
			std::multimap<std::chrono::steady_clock::time_point, std::shared_ptr<event>> timer_list_;

		 public:
			std::shared_ptr<event> push(std::chrono::nanoseconds ms, std::shared_ptr<event> &ev) {
				std::chrono::steady_clock::time_point tp = std::chrono::steady_clock::now() + ms;
				timer_list_.insert(std::make_pair(tp, ev));
				return ev;
			}
			bool erase(std::shared_ptr<event> &ev) {
				for (auto it = timer_list_.begin(); it != timer_list_.end(); it ++) {
					if (it->second == ev) {
						timer_list_.erase(it);
						return true;
					}
				}
				return false;
			}
			std::chrono::steady_clock::time_point top_timeout() {
				if (size()) {
					auto it = timer_list_.begin();
					return (it->first);
				} else {
					return std::chrono::steady_clock::time_point();
				}
			}
			size_t size() {
				size_t sz = timer_list_.size();
				return sz;
			}
			void timerevent(std::function<void(std::shared_ptr<event>)> cb) {
				std::chrono::steady_clock::time_point tp_now = std::chrono::steady_clock::now();
				// リストをループし, 現在時刻よりもタイムアウト時間が前のものに対して
				// リストから抜いてコールバックする.
				for (auto it = timer_list_.begin(); it != timer_list_.end();) {
					std::chrono::steady_clock::time_point tp = it->first;
					if (tp <= tp_now) {
						std::shared_ptr<event> ev = it->second;
						it = timer_list_.erase(it);
						cb(ev);
					} else {
						// 以降は検索対象がない.
						break;
					}
				}
			}
		};

		template<nice_t PRIORITY = 1>
		class workque_internal___ {
		 protected:
			static const size_t PRIORITY_SIZE = PRIORITY + 1;
			workque_fifo_internal___ fifo[PRIORITY_SIZE];
			workque_timer_list_internal___ timer_list_;

			std::mutex mtx_;
			std::condition_variable cond_;

			virtual size_t event_counts() {
				size_t count = 0;
				for (nice_t pri = 0; pri < PRIORITY_SIZE; pri++) {
					if (fifo[pri].size()) {
						count++;
					}
				}
				return count;
			}
			bool pop_event_nolock(std::shared_ptr<event> &ev) {
				for (nice_t pri = 0; pri < PRIORITY_SIZE; pri++) {
					if (fifo[pri].size()) {
						ev = fifo[pri].pop();
						return true;
					}
				}
				return false;
			}
			void exec_timerevent(void) {
				std::unique_lock<std::mutex> lock(mtx_);
				timer_list_.timerevent(
					[this](std::shared_ptr<event> ev) {
						fifo[nice_t(*ev)].push(ev);
					}
				);
			}
			virtual void empty_wait(void) {
				std::unique_lock<std::mutex> lock(mtx_);
				if (0 == event_counts()) {
					if (0 != timer_list_.size()) {
						std::chrono::steady_clock::time_point tp = timer_list_.top_timeout();
						cond_.wait_until(lock, tp);
					} else {
						cond_.wait(lock);
					}
				}
			}
			virtual void exec(void) {
				std::shared_ptr<event> ev;
				bool is_get = false;
				{
					std::unique_lock<std::mutex> lock(mtx_);
					is_get = pop_event_nolock(ev);
				}
				if (is_get) {
					(*ev)();
				}
			}
			template<typename Callable, typename ... Args>
			std::shared_ptr<event> push(nice_t&& prio, Callable&& call, Args &&... args) {
				if (prio < PRIORITY) {
					std::function<void(Args ...)> func = call;
					std::shared_ptr<event> ev = std::shared_ptr<event>(new event{ prio, func, args ... });
					std::unique_lock<std::mutex> lock(mtx_);
					fifo[prio].push(ev);
					// 待っている物を1つスケジュール
					cond_.notify_one();
					return ev;
				} else {
					return std::shared_ptr<event>(nullptr);
				}
			}
			template<typename Callable, typename ... Args>
			std::shared_ptr<event> push_for(nice_t&& prio, std::chrono::milliseconds ms, Callable&& call, Args &&... args) {
				if (prio < PRIORITY) {
					std::function<void(Args ...)> func = call;
					std::shared_ptr<event> ev = std::shared_ptr<event>(new event{ prio, func, args ... });
					std::unique_lock<std::mutex> lock(mtx_);
					timer_list_.push(std::forward<std::chrono::milliseconds>(ms), ev);

					// 待っている物を1つスケジュール. 空振りしてもよい.
					cond_.notify_one();
					return ev;
				} else {
					return std::shared_ptr<event>(nullptr);
				}
			}
			void cancel(std::shared_ptr<event>& ev) {
				std::unique_lock<std::mutex> lock(mtx_);
				nice_t prio = nice_t(ev);
				bool released = timer_list_.erase(ev);
				if (!released) {
					fifo[prio].erase(ev);
				}
			}
			void quit() {
				// 待っている物をすべてスケジュール
				// これにより, wait()がすべてスケジュールされる
				cond_.notify_all();
			}
			void wait_and_exec(void) {
				empty_wait();
				exec_timerevent();
				exec();
			}
		};
	}

	// workque処理 （優先度, スレッド数指定可能）
	template<uint32_t THREADS = 1, nice_t PRIORITY = 1>
	class workque : protected __internal__::workque::workque_internal___<PRIORITY> {
	 private:
		std::shared_ptr<std::thread> thread_[THREADS];
		std::atomic<bool>	is_quit_{false};
	 public:
		using __internal__::workque::workque_internal___<PRIORITY>::push;
		using __internal__::workque::workque_internal___<PRIORITY>::push_for;
		using __internal__::workque::workque_internal___<PRIORITY>::cancel;

		// 全メインループ破棄
		void quit() {
			is_quit_.store(true);
			__internal__::workque::workque_internal___<PRIORITY>::quit();
		}

		// メインループ
		void run() {
			is_quit_.store(false);
			for (; is_quit_.load() == false;) {
				__internal__::workque::workque_internal___<PRIORITY>::wait_and_exec();
			}
		}
		void operator()(void) {	run(); }

		// スレッド生成
		void start() {
			// 指定数分threadを生成
			for (uint32_t i = 0; i < THREADS; i++) {
				thread_[i] = std::shared_ptr<std::thread>(new std::thread(
					[this]() { run(); }
				));
			}
		}
		// スレッド終了まで待つ
		void wait() {
			for (uint32_t i = 0; i < THREADS; i++) {
				thread_[i]->join();
				thread_[i] = nullptr;
			}
		}
		// スレッド破棄
		void stop() {
			quit();
			wait();
		}
	};

	// シンプルなworkque処理
	class simple_workque : protected workque<1, 1> {
	 public:
		template<typename Callable, typename ... Args>
		std::shared_ptr<event> push(Callable&& call, Args &&... args) {
			return workque::push(nice_t(0), call, args ...);
		}
		template<typename Callable, typename ... Args>
		std::shared_ptr<event> push_for(std::chrono::milliseconds ms, Callable&& call, Args &&... args) {
			return workque::push_for(nice_t(0), ms, call, args ...);
		}

		using workque<1, 1>::cancel;
		using workque<1, 1>::quit;
		using workque<1, 1>::run;
		using workque<1, 1>::operator();
		using workque<1, 1>::start;
		using workque<1, 1>::wait;
		using workque<1, 1>::stop;
	};

} // namespace workque
} // namespace libsharaku

#endif // LIBSHARAKU_WORKQ_PLUSPLUS_HPP
