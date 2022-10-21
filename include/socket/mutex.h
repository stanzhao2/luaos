

/************************************************************************************
**
** Copyright 2021 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
************************************************************************************/

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

/***********************************************************************************/

class atomic_lock final {
  std::atomic_flag _flag = ATOMIC_FLAG_INIT;

public:
  inline void lock()
  {
    while (!try_lock()) std::this_thread::yield();
  }
  inline void unlock()
  {
    _flag.clear(std::memory_order_release);
  }
  inline bool try_lock()
  {
    return !_flag.test_and_set(std::memory_order_acquire);
  }

public:
  atomic_lock() = default;
  atomic_lock(const atomic_lock&) = delete;
  atomic_lock& operator= (const atomic_lock&) = delete;
};

/***********************************************************************************/

//#define lock_type std::mutex
#define lock_type atomic_lock
#define local_unique_lock std::unique_lock<lock_type>

/***********************************************************************************/
