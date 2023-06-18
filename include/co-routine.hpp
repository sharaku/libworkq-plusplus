/* --
 *
 * MIT License
 * 
 * Copyright (c) 2023 Abe Takafumi
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

#ifndef LIBSHARAKU_WORKQ_COROUTINE_HPP
#define LIBSHARAKU_WORKQ_COROUTINE_HPP

#include <vector>
#include <workq++.hpp>

namespace sharaku {
namespace workque {

  class coroutine {
   public:
    // 連続してスケジューラを実行する
    // 登録関数の戻り値によって制御できる.
    enum class result : int {
      end = -2,     // IDLE状態にする
      submit = -1,  // PCをカウントアップしてsuspend状態にする
      retry = 0,    // 同じ処理を再実行する
      next = 1,     // 次の処理を実行する
    };

    enum class status : int {
      idle,
      active,
      suspend,
    };

   protected:
    status st = status::idle;
    uint64_t pc_ = 0;
    std::shared_ptr<event> ev_;

    workque *wq_ = nullptr;
    nice_t nice_ = 0;

    struct coroutine_paramss {
      workque *wq;
      nice_t nice;
      std::chrono::milliseconds ms;
      std::function<void(void)> func;
      coroutine_paramss(workque *wq_, nice_t nice_, std::chrono::milliseconds ms_, std::function<void(void)> func_) {
        wq = wq_;
        nice = nice_;
        ms = ms_;
        func = func_;
      }
    };
    std::vector<coroutine_paramss> routine_;

   public:

    coroutine(workque *wq, nice_t nice = 0) {
      ev_ = std::make_shared<event>(0);
      wq_ = wq;
      nice_ = nice;
    }

    coroutine& with_nice(nice_t nice) {
      nice_ = nice;
      return *this;
    }

    coroutine& with_workque(workque *wq) {
      wq_ = wq;
      return *this;
    }

    /**
     * 処理ルーチンを登録する
     */
    coroutine& push(std::function<result(void)> func) {
      routine_.emplace_back(wq_, nice_, std::chrono::milliseconds(0),
        [this, func]() {
          result ret = func();
          if (ret == result::end) {
            idle_();
          } else if (ret == result::submit) {
            submit_();
          } else {
            next_(static_cast<int>(ret));
          }
        }
      );
      return *this;
    }

    /**
     * 指定秒数後に実行を行う処理ルーチンを登録する
     */
    coroutine& push_for(std::chrono::milliseconds ms, std::function<result(void)> func) {
      routine_.emplace_back(wq_, nice_, ms,
        [this, func]() {
          result ret = func();
          if (ret == result::end) {
            idle_();
          } else if (ret == result::submit) {
            submit_();
          } else {
            next_(static_cast<int>(ret));
          }
        }
      );
      return *this;
    }

    /**
     * 処理ルーチンを実行する
     */
    void start() {
      st = status::active;
      if (routine_.size() > pc_) {
        ev_->set_nice(routine_[pc_].nice);
        ev_->set_function(routine_[pc_].func);
        if (routine_[pc_].ms != std::chrono::milliseconds(0)) {
          routine_[pc_].wq->push_for(routine_[pc_].ms, ev_);
        } else {
          routine_[pc_].wq->push(ev_);
        }
      }
    }

    void stop() {
      routine_[pc_].wq->cancel(ev_);
      st = status::idle;
      pc_ = 0;
    }

    void suspend() {
      routine_[pc_].wq->cancel(ev_);
      st = status::suspend;
    }

    void resume() {
      if (st == status::suspend) {
        ev_->set_nice(routine_[pc_].nice);
        ev_->set_function(routine_[pc_].func);
        if (routine_[pc_].ms != std::chrono::milliseconds(0)) {
          routine_[pc_].wq->push_for(routine_[pc_].ms, ev_);
        } else {
          routine_[pc_].wq->push(ev_);
        }
      }
    }

   protected:

    void idle_() {
      st = status::idle;
      pc_ = 0;
    }

    void submit_() {
      st = status::suspend;
      next_(1);
    }

    void next_(int add_pc) {
        if (routine_.size() > (pc_ + add_pc)) {
          pc_ += add_pc;
        } else {
          st = status::idle;
          pc_ = 0;
        }

      if (st == status::active) {
        ev_->set_nice(routine_[pc_].nice);
        ev_->set_function(routine_[pc_].func);
        if (routine_[pc_].ms != std::chrono::milliseconds(0)) {
          routine_[pc_].wq->push_for(routine_[pc_].ms, ev_);
        } else {
          routine_[pc_].wq->push(ev_);
        }
      }
    }

  };


}
}

#endif // LIBSHARAKU_WORKQ_COROUTINE_HPP
