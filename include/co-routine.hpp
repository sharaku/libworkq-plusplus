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

#include <atomic>
#include <vector>
#include <workq++.hpp>

namespace sharaku {
namespace workque {
  class coroutine {
   public:
    // 連続してスケジューラを実行する
    // 登録関数の戻り値によって制御できる.
    enum class result : int {
      end = -2,           // IDLE状態にする
      submit = -1,        // PCをカウントアップしてsuspend状態にする
      retry = 0,          // 同じ処理を再実行する
      next = 1,           // 次の処理を実行する
    };

    enum class status : int {
      idle,
      active,
      suspend,
    };

   protected:
    /// 状態
    status st = status::idle;
    /// 実行位置
    uint64_t pc_ = 0;
    /// スケジュール中のイベントスケジューラ
    std::shared_ptr<event> ev_;
    /// デフォルトで使用するworkq
    workque *wq_ = nullptr;
    /// デフォルトで使用する優先度
    nice_t nice_ = 0;
    /// 実行中のカウント
    std::atomic<uint64_t> counter_{0};

    /// ルーチンの1パラメータ
    struct coroutine_paramss {
      /// 使用するworkq
      workque *wq = nullptr;
      /// 優先度
      nice_t nice = 0;
      /// ディレイ秒数
      std::chrono::milliseconds ms = std::chrono::milliseconds(0);
      /// スケジュールする関数
      std::function<void(void)> func = nullptr;
      /// スケジュール中のイベントスケジューラ
      std::shared_ptr<event> ev = nullptr;

      /// コンストラクタ
      coroutine_paramss() {
      };

      coroutine_paramss(workque *wq_, nice_t nice_, std::chrono::milliseconds ms_, std::function<void(void)> func_) {
        wq = wq_;
        nice = nice_;
        ms = ms_;
        func = func_;
      }

      void start() {
        ev = std::make_shared<event>(0);
        ev->set_nice(nice);
        ev->set_function(func);
        if (ms != std::chrono::milliseconds(0)) {
          wq->push_for(ms, ev);
        } else {
          wq->push(ev);
        }
      }

      void cancel() {
        wq->cancel(ev);
      }
    };

    /// 実行対象のルーチンリスト
    std::vector<coroutine_paramss> routine_;

    /// subに対するmaster.
    /// thisが完了、masterの次を実行する.
    coroutine *master = nullptr;
   public:

    /**
     * @brief コンストラクタ
     *
     * デフォルトのniceを登録する.
     *
     * @param[in] wq 登録するworkq
     * @param[in] nice 登録するnice
     */
    coroutine(workque *wq, nice_t nice = 0) {
      ev_ = std::make_shared<event>(0);
      wq_ = wq;
      nice_ = nice;
    }

    /**
     * @brief デフォルトのniceを登録する.
     *
     * デフォルトのniceを登録する.
     *
     * @param[in] nice 登録するnice
     * @return 自身への参照
     */
    coroutine& with_nice(nice_t nice) {
      nice_ = nice;
      return *this;
    }

    /**
     * @brief デフォルトのworkqを登録する.
     *
     * デフォルトのworkqを登録する.
     *
     * @param[in] wq 登録するworkq
     * @return 自身への参照
     */
    coroutine& with_workque(workque *wq) {
      wq_ = wq;
      return *this;
    }

    /**
     * @brief 処理ルーチンを登録する.
     *
     * 処理ルーチンを登録する.
     *
     * @param[in] func 登録する関数オブジェクト
     * @return 自身への参照
     */
    virtual coroutine& push(std::function<result(void)> func) {
      routine_.emplace_back(wq_, nice_, std::chrono::milliseconds(0),
        [this, func]() {
          ++ counter_;
          complete_(func());
        }
      );
      return *this;
    }

    /**
     * @brief 指定秒数後に実行を行う処理ルーチンを登録する.
     *
     * 指定秒数後に実行を行う処理ルーチンを登録する.
     *
     * @param[in] ms ディレイミリ秒
     * @param[in] func 登録する関数オブジェクト
     * @return 自身への参照
     */
    virtual coroutine& push_for(std::chrono::milliseconds ms, std::function<result(void)> func) {
      routine_.emplace_back(wq_, nice_, ms,
        [this, func]() {
          ++ counter_;
          complete_(func());
        }
      );
      return *this;
    }

    /**
     * @brief 処理ルーチンを登録する.
     *
     * 処理ルーチンを登録する.
     *
     * @param[in] sub 登録するコルーチン
     * @return 自身への参照
     */
    virtual coroutine& push(coroutine *sub) {
      // sub側にthisを登録する
      sub->master = this;
      routine_.emplace_back(wq_, nice_, std::chrono::milliseconds(0),
        [this, sub]() {
          ++ counter_;
          sub->start();
          submit_();
          // sub側が完了する際に, this->next_(1); が呼び出される
        }
      );
      return *this;
    }

    /**
     * @brief 指定秒数後に実行を行う処理ルーチンを登録する.
     *
     * 指定秒数後に実行を行う処理ルーチンを登録する.
     *
     * @param[in] ms ディレイミリ秒
     * @param[in] sub 登録するコルーチン
     * @return 自身への参照
     */
    virtual coroutine& push_for(std::chrono::milliseconds ms, coroutine *sub) {
      // sub側にthisを登録する
      sub->master = this;
      routine_.emplace_back(wq_, nice_, ms,
        [this, sub]() {
          ++ counter_;
          sub->start();
          submit_();
          // sub側が完了する際に, this->next_(1); が呼び出される
        }
      );
      return *this;
    }

    /**
     * @brief 処理ルーチンを実行する.
     *
     * 実行中のものがある場合は, 状態のみ変更する.
     */
    virtual void start() {
      st = status::active;
      if (counter_.load() == 0) {
        if (routine_.size() > pc_) {
          routine_[pc_].start();
        }
      }
    }

    /**
     * @brief 処理ルーチンを停止する.
     *
     * 実行中の処理ルーチンを停止する.
     */
    virtual void stop() {
      routine_[pc_].cancel();
      end_();
    }

    /**
     * @brief 状態をsuspendにする.
     *
     * 状態を変更することで, complete_が呼ばれたときにそこで一時停止する.
     */
    virtual void suspend() {
      st = status::suspend;
    }

    /**
     * @brief suspendの状態をactiveにする.
     *
     * コルーチンを再開します.
     */
    virtual void resume() {
      if (st == status::suspend) {
        routine_[pc_].start();
      }
    }

   protected:

    /**
     * @brief コルーチン外に処理を移管するための処理.
     *
     * コルーチン外に処理を移管する場合に使用します.
     */
    virtual void submit_() {
    }

    /**
     * @brief ルーチン成功時に呼び出す.
     *
     * ルーチン成功時に呼び出す.
     *
     * @param[in] ret ルーチンの終了コード
     */
    virtual void complete_(result ret) {
      -- counter_;
      if (ret == result::end) {
        end_();
      } else if (ret == result::submit) {
        submit_();
      } else {
        // retry or next
        next_(static_cast<int>(ret));
      }
    }

    /**
     * @brief コルーチンの次を実行する.
     *
     * コルーチンの次の実行を行います. 次が登録されていない場合は終了する.
     *
     * @param[in] add_pc PCに対して加算する値
     */
    virtual void next_(int add_pc) {
      if (routine_.size() > (pc_ + add_pc)) {
        pc_ += add_pc;

        // suspendの場合は次に行かない
        if (st == status::active) {
          routine_[pc_].start();
        }
      } else {
        end_();
      }
    }

    /**
     * @brief コルーチン終了時に呼び出す.
     *
     * コルーチン終了時に呼び出す. これにより, 呼び出し元の次のルーチンを実行する.
     */
    virtual void end_() {
      st = status::idle;
      pc_ = 0;
      if (master) {
        master->complete_(result::next);
      }
    }
  };

  /// 並列で実行する
  class coroutine_parallel : public coroutine {
   protected:
    /// スケジュール中のカウント
    std::atomic<uint64_t> sched_counter_{0};

    using coroutine::suspend;
    using coroutine::resume;

   public:
    /**
     * @brief コンストラクタ
     *
     * デフォルトのniceを登録する.
     *
     * @param[in] wq 登録するworkq
     * @param[in] nice 登録するnice
     */
    coroutine_parallel(workque *wq, nice_t nice = 0)
     : coroutine(wq, nice)
    {}

    /**
     * @brief 処理ルーチンを登録する.
     *
     * 処理ルーチンを登録する.
     *
     * @param[in] func 登録する関数オブジェクト
     * @return 自身への参照
     */
    virtual coroutine& push(std::function<result(void)> func) {
      ++ sched_counter_;
      return coroutine::push(func);
    }

    /**
     * @brief 指定秒数後に実行を行う処理ルーチンを登録する.
     *
     * 指定秒数後に実行を行う処理ルーチンを登録する.
     *
     * @param[in] ms ディレイミリ秒
     * @param[in] func 登録する関数オブジェクト
     * @return 自身への参照
     */
    virtual coroutine& push_for(std::chrono::milliseconds ms, std::function<result(void)> func) {
      ++ sched_counter_;
      return coroutine::push_for(ms, func);
    }

    /**
     * @brief 処理ルーチンを登録する.
     *
     * 処理ルーチンを登録する.
     *
     * @param[in] sub 登録するコルーチン
     * @return 自身への参照
     */
    virtual coroutine& push(coroutine *sub) {
      ++ sched_counter_;
      return coroutine::push(sub);
    }

    /**
     * @brief 指定秒数後に実行を行う処理ルーチンを登録する.
     *
     * 指定秒数後に実行を行う処理ルーチンを登録する.
     *
     * @param[in] ms ディレイミリ秒
     * @param[in] sub 登録するコルーチン
     * @return 自身への参照
     */
    virtual coroutine& push_for(std::chrono::milliseconds ms, coroutine *sub) {
      ++ sched_counter_;
      return coroutine::push_for(ms, sub);
    }

    /**
     * @brief 処理ルーチンを実行する.
     *
     * 実行中のものがある場合は, 状態のみ変更する.
     */
    virtual void start() {
      // 登録しているものをすべて実行
      st = status::active;
      if (counter_.load() == 0) {
        for (auto &routine : routine_) {
          routine.start();
        }
      }
    }

    /**
     * @brief 処理ルーチンを停止する.
     *
     * 実行中の処理ルーチンを停止する.
     */
    virtual void stop() {
      // 登録しているものをすべてキャンセル実行
      for (auto &routine : routine_) {
        routine.cancel();
      }
      end_();
    }

   protected:
    /**
     * @brief ルーチン成功時に呼び出す.
     *
     * ルーチン成功時に呼び出す.
     *
     * @param[in] ret ルーチンの終了コード
     */
    virtual void complete_(result ret) {
      -- counter_;
      -- sched_counter_;
      if (sched_counter_.load() == 0) {
        end_();
      }
    }
  };

  /// Keyに対するルーチンを登録できる
  template<class KEY>
  class coroutine_switch : public coroutine {
   protected:
    using coroutine::push;
    using coroutine::push_for;
    using coroutine::suspend;
    using coroutine::resume;

    coroutine_paramss routine_;
    std::shared_ptr<event> ev_ = nullptr;
    workque *exec_wq_ = nullptr;

    /// Keyに対する実行関数
    std::map<KEY, coroutine::coroutine_paramss> case_map_;

   public:
    /**
     * @brief コンストラクタ
     *
     * デフォルトのniceを登録する.
     *
     * @param[in] wq 登録するworkq
     * @param[in] nice 登録するnice
     */
    coroutine_switch(workque *wq, nice_t nice = 0)
     : coroutine(wq, nice)
    {}

    virtual coroutine_switch<KEY>& switch_function(std::function<KEY(void)> func) {
      routine_.wq = wq_;
      routine_.nice = nice_;
      routine_.func = [this, func]() {
        ++ counter_;
        KEY result = func();
        -- counter_;
        auto it = case_map_.find(result);
        if (it != case_map_.end()) {
          // 次をスケジュール
          it->second.start();
        } else {
          // ここで終了
          end_();
        }
      };
      return *this;
    }
    virtual coroutine_switch<KEY>& then(KEY key, std::function<result(void)> func) {
      case_map_.insert(
        std::make_pair(key,
          coroutine_paramss(wq_, nice_, std::chrono::milliseconds(0), [this, func]() {
            ++ counter_;
            complete_(func());
          })
        )
      );
      return *this;
    }
    virtual coroutine_switch<KEY>& then(KEY key, coroutine *sub) {
      case_map_.insert(
        std::make_pair(key,
          coroutine_paramss(wq_, nice_, std::chrono::milliseconds(0), [this, sub]() {
            ++ counter_;
            sub->start();
            submit_();
            // sub側が完了する際に, this->next_(1); が呼び出される
          })
        )
      );
      return *this;
    }

    /**
     * @brief 処理ルーチンを実行する.
     *
     * 実行中のものがある場合は, 状態のみ変更する.
     */
    virtual void start() {
      routine_.start();
    }

    /**
     * @brief 処理ルーチンを停止する.
     *
     * 実行中の処理ルーチンを停止する.
     */
    virtual void stop() {
      routine_.cancel();
    }

   protected:
  };
}
}

#endif // LIBSHARAKU_WORKQ_COROUTINE_HPP
