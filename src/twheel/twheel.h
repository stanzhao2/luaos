

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

#include <stdint.h>
#include <string>

#define MAX_FAR sizeof(size_t)
#define TIME_NEAR_SHIFT 8
#define TIME_NEAR (1 << TIME_NEAR_SHIFT)
#define TIME_NEAR_MASK (TIME_NEAR - 1)
#define TIME_FAR_SHIFT 6
#define TIME_FAR (1 << TIME_FAR_SHIFT)
#define TIME_FAR_MASK (TIME_FAR - 1)
#define MAX_DELAY_TIME (MAX_FAR * 2 * 24 * 3600 * 1000)
#define MAX_TIME (((size_t)1 << (MAX_FAR * 6 + TIME_NEAR_SHIFT)) - MAX_DELAY_TIME - 1)
#define MAX_NODE_CACHE 8192

typedef void (*twfunction) (void* arg);

struct tw_callback {
  twfunction func;
  void* arg;
};

struct timenode {
  size_t time;
  struct tw_callback callback;
  struct timenode* next;
};

struct timelist {
  struct timenode head;
  struct timenode* tail;
};

struct twheel_t {
  size_t need_update;
  size_t free_total;
  size_t curr_time;
  size_t start_time;
  struct timenode* freenode;
  struct timelist _near[TIME_NEAR];
  struct timelist _far[MAX_FAR][TIME_FAR];
};

//user interface

twheel_t* timewheel_create(size_t t);

bool timewheel_add_time(twheel_t* ptw, twfunction func, void* arg, size_t t);

void timewheel_update(twheel_t* ptw, size_t t);

void timewheel_release(twheel_t* ptw);
