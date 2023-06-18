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

#ifndef LIBSHARAKU_WORKQ_INTERVALTIMER_HPP
#define LIBSHARAKU_WORKQ_INTERVALTIMER_HPP

#include <vector>
#include <workq++.hpp>
#include <co-routine.hpp>

namespace sharaku {
namespace workque {

  class intervaltimer : protected sharaku::workque::coroutine {
    std::vector<std::function<void(void)>> func_lists_;
    std::chrono::milliseconds interval_;

   public:
    intervaltimer(workque *wq, nice_t nice = 0)
    : sharaku::workque::coroutine {wq, nice}
    {
    }

    intervaltimer& with_interval(std::chrono::milliseconds interval) {
      interval_ = interval;
      return *this;
    }

    intervaltimer& push(std::function<void(void)> func) {
      func_lists_.push_back(func);
      return *this;
    }

    void start(std::chrono::milliseconds ms = std::chrono::milliseconds(0)) {
      push_for(
        ms,
        [this]() -> sharaku::workque::coroutine::result {
          exec();
          return sharaku::workque::coroutine::result::next;
        }
      );
      push_for(
        interval_,
        [this]() -> sharaku::workque::coroutine::result {
          exec();
          return sharaku::workque::coroutine::result::retry;
        }
      );

      sharaku::workque::coroutine::start();
    }

    using sharaku::workque::coroutine::stop;
    using sharaku::workque::coroutine::suspend;
    using sharaku::workque::coroutine::resume;
   protected:
    void exec() {
      for (auto &func : func_lists_) {
        func();
      }
    }
  };


}
}

#endif // LIBSHARAKU_WORKQ_INTERVALTIMER_HPP
