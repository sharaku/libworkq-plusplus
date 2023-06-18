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
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace sharaku {
namespace workque {

  // 処理優先度の型
  using nice_t = uint32_t;

  //イベントクラス
  class event {
   private:
    std::function<void(void)> func_ = nullptr;
    nice_t nice_ = 0;

   public:
    event() = delete;
    event(nice_t nice) {
      nice_ = nice;
    }
    event(nice_t nice, std::function<void(void)> func) {
      nice_ = nice;
      func_ = func;
    }

    event& set_nice(nice_t nice) {
      nice_ = nice;
      return *this;
    }

    event& set_function(std::function<void(void)> func) {
      func_ = func;
      return *this;
    }

    // nice値を取得
    nice_t get_nice() {
      return nice_;
    }

    // 登録された処理を実行
    void operator()() {
      if (func_) {
        func_();
      }
    }
  };

  namespace __internal__::workque {
    using event = sharaku::workque::event;

    // FIFOの管理を行うクラス
    class workque_fifo_internal___ {
     protected:
      std::vector< std::deque<std::shared_ptr<event>> > fifo_;
      std::multimap<std::chrono::steady_clock::time_point, std::shared_ptr<event>> timer_list_;

     public:
      // eventを登録する
      void push(std::shared_ptr<event> ev) {
        if (fifo_.size() < ev->get_nice() + 1) {
          fifo_.resize(ev->get_nice() + 1);
        }
        fifo_[ev->get_nice()].push_back(ev);
      }

      // 時間指定でeventを登録する
      void push_for(std::chrono::nanoseconds ms, std::shared_ptr<event> ev) {
        std::chrono::steady_clock::time_point tp = std::chrono::steady_clock::now() + ms;
        timer_list_.insert(std::make_pair(tp, ev));
      }

      // FIFOの先頭から抜く
      std::shared_ptr<event> pop() {
        for (auto &fifo : fifo_) {
          if (fifo.size()) {
            // 一番優先度の高いものを取り出す
            std::shared_ptr<event> ev = fifo.front();
            fifo.pop_front();
            return ev;
          }
        }
        return std::shared_ptr<event>(nullptr);
      }

      // イベントをキューから抜く
      bool erase(std::shared_ptr<event> &ev) {
        // nice値のfifoに登録する
        for (auto &fifo : fifo_) {
          for (auto it = fifo.begin(); it != fifo.end(); it ++) {
            if (*it == ev) {
              fifo.erase(it);
              return true;
            }
          }
        }
        return false;
      }

      // タイマー待ちを行う時間を取得
      const std::chrono::steady_clock::time_point get_wait_time() {
        if (timer_list_.size()) {
          return (timer_list_.begin()->first);
        } else {
          while(1);
          return std::chrono::steady_clock::time_point();
        }
      }

      // タイマー待ちのものをFIFOへ積む
      void timeout() {
        std::chrono::steady_clock::time_point tp_now = std::chrono::steady_clock::now();
        // リストをループし, 現在時刻よりもタイムアウト時間が前のものに対して
        // リストから抜いてfifoへ入れる
        for (auto it = timer_list_.begin(); it != timer_list_.end();) {
          std::chrono::steady_clock::time_point tp = it->first;
          if (tp <= tp_now) {
            std::shared_ptr<event> ev = it->second;
            it = timer_list_.erase(it);
            push(ev);
          } else {
            // 以降は検索対象がない.
            break;
          }
        }
      }

      // FIFOをすべて破棄する
      void clear() {
        fifo_.clear();
        timer_list_.clear();
      }
    };

    // 排他, condition_variableを使用して待ち合わせる
    class workque_internal___ : protected workque_fifo_internal___{
     protected:

      // 排他, 待ち合わせ用のmutex
      std::mutex mtx_;

      // 排他, 待ち合わせ用のcondition_variable
      std::condition_variable cond_;

      // スケジュールするものがなければ待つ
      virtual std::shared_ptr<event> pop_and_wait(void) {
        for (;;) {
          timeout();

          std::unique_lock<std::mutex> lock(mtx_);
          std::shared_ptr<event> ev = pop();
          if (ev == nullptr) {
            const std::chrono::steady_clock::time_point timeo = get_wait_time();
            if (timeo == std::chrono::steady_clock::time_point()) {
              cond_.wait(lock);
            } else {
              cond_.wait_until(lock, timeo);
            }
          } else {
            return ev;
          }
        }
        return std::shared_ptr<event>(nullptr);
      }

    public:
      // 先頭を抜いて実行する
      virtual void exec(void) {
        std::shared_ptr<event> ev = pop_and_wait();
        (*ev)();
      }

      std::shared_ptr<event> push(std::shared_ptr<event> ev) {
        std::unique_lock<std::mutex> lock(mtx_);
        workque_fifo_internal___::push(ev);

        // 待っている物を1つスケジュール
        cond_.notify_one();
        return ev;
      }

      std::shared_ptr<event> push_for(std::chrono::milliseconds ms, std::shared_ptr<event> ev) {
        std::unique_lock<std::mutex> lock(mtx_);
        workque_fifo_internal___::push_for(ms, ev);

        // 待っている物を1つスケジュール. 空振りしてもよい.
        cond_.notify_one();
        return ev;
      }

      std::shared_ptr<event> push(nice_t&& nice, std::function<void(void)> &&func) {
        std::shared_ptr<event> ev = std::shared_ptr<event>(new event{nice, func});
        return workque_internal___::push(ev);
      }

      std::shared_ptr<event> push_for(std::chrono::milliseconds &&ms, nice_t&& nice, std::function<void(void)> &&func) {
        std::shared_ptr<event> ev = std::shared_ptr<event>(new event{nice, func});
        return workque_internal___::push_for(ms, ev);
      }

      std::shared_ptr<event> push(std::function<void(void)> &&func) {
        std::shared_ptr<event> ev = std::shared_ptr<event>(new event{0, func});
        return workque_internal___::push(ev);
      }

      std::shared_ptr<event> push_for(std::chrono::milliseconds &&ms, std::function<void(void)> &&func) {
        std::shared_ptr<event> ev = std::shared_ptr<event>(new event{0, func});
        return workque_internal___::push_for(ms, ev);
      }

      void cancel(std::shared_ptr<event>& ev) {
        std::unique_lock<std::mutex> lock(mtx_);
        workque_fifo_internal___::erase(ev);
      }

      void quit() {
        // 待っている物をすべてスケジュール
        // これにより, wait()がすべてスケジュールされる
        cond_.notify_all();
      }
    };
  }

  // workque処理 （優先度, スレッド数指定可能）
  class workque : protected __internal__::workque::workque_internal___ {
   private:
    std::vector<std::thread> threads_;
    std::atomic<bool> is_quit_{false};

   public:
    using __internal__::workque::workque_internal___::push;
    using __internal__::workque::workque_internal___::push_for;
    using __internal__::workque::workque_internal___::cancel;

    // メインループ
    void run() {
      is_quit_.store(false);
      for (; is_quit_.load() == false;) {
        __internal__::workque::workque_internal___::exec();
      }
    }
    void operator()(void) { run(); }

    // スレッド生成
    void start(uint32_t threads = 1) {
      // 指定数分threadを生成
      for (uint32_t i = 0; i < threads; i++) {
        threads_.emplace_back(
          std::thread([this]() {run();})
        );
      }
    }

    // 全メインループ破棄
    void quit() {
      is_quit_.store(true);
      __internal__::workque::workque_internal___::quit();
    }

    // スレッド終了まで待つ
    void wait() {
      for (auto &thread : threads_) {
        thread.join();
      }
      threads_.clear();
    }

    // スレッド破棄
    void stop() {
      quit();
      wait();
    }
  };

} // namespace workque
} // namespace sharaku

#endif // LIBSHARAKU_WORKQ_PLUSPLUS_HPP
