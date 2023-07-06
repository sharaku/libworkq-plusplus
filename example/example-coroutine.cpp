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
#include <stdio.h>
#include "../include/workq++.hpp"
#include "../include/co-routine.hpp"

int
main(void)
{
  sharaku::workque::workque scheduler;

  sharaku::workque::coroutine co_in1(&scheduler, 0);
  co_in1
   .push([]() -> sharaku::workque::coroutine::result {
      printf("co_in1::function1\n");
      return sharaku::workque::coroutine::result::next;
    })
   .push([](){
      printf("co_in1::function2\n");
      return sharaku::workque::coroutine::result::next;
    });

  sharaku::workque::coroutine co_in2(&scheduler, 0);
  co_in2
   .push([]() -> sharaku::workque::coroutine::result {
      printf("co_multi::co_in2::function1\n");
      return sharaku::workque::coroutine::result::next;
    })
   .push([](){
      printf("co_multi::co_in2::function2\n");
      return sharaku::workque::coroutine::result::next;
    });

  sharaku::workque::coroutine co_in3(&scheduler, 0);
  co_in3
   .push([]() -> sharaku::workque::coroutine::result {
      printf("co_multi::co_in3::function1\n");
      return sharaku::workque::coroutine::result::next;
    })
   .push([](){
      printf("co_multi::co_in3::function2\n");
      return sharaku::workque::coroutine::result::next;
    });


  sharaku::workque::coroutine_parallel co_multi(&scheduler, 0);
  co_multi
   .push([]() -> sharaku::workque::coroutine::result {
      printf("co_multi::function1\n");
      return sharaku::workque::coroutine::result::next;
    })
   .push([](){
      printf("co_multi::function2\n");
      return sharaku::workque::coroutine::result::next;
    })
   .push(&co_in2)
   .push(&co_in3);

  sharaku::workque::coroutine co(&scheduler, 0);
  co
   .push([]() -> sharaku::workque::coroutine::result {
      printf("co::function1\n");
      return sharaku::workque::coroutine::result::next;
    })
   .push(&co_in1)
   .push(&co_multi)
   .push([](){
      printf("co::function2\n");
      return sharaku::workque::coroutine::result::next;
    })
   .push([](){
      printf("co::function3\n");
      exit(0);
      return sharaku::workque::coroutine::result::end;
    })
   .start();

  scheduler.run();
  return 0;
}
