
#include "twheel.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

/***********************************************************************************/

static timenode* timelist_clear(timelist* l)
{
  struct timenode* ret = l->head.next;
  l->head.next = 0;
  l->tail = &l->head;
  return ret;
}

static void timelist_add(timelist* l, timenode* n)
{
  l->tail->next = n;
  n->next = 0;
  l->tail = n;
}

static void timewheel_add_node(twheel_t* ptw, timenode* node)
{
  size_t curr_time = ptw->curr_time;
  size_t delay_time = node->time;
  //judge time zone to near-far put
  if ((delay_time | TIME_NEAR_MASK) == (curr_time | TIME_NEAR_MASK)) {
    timelist_add(&ptw->_near[(delay_time & TIME_NEAR_MASK)], node);
  }
  else {
    size_t i;
    size_t t1 = curr_time >> TIME_NEAR_SHIFT;
    size_t t2 = delay_time >> TIME_NEAR_SHIFT;
    for (i = 0; i < MAX_FAR; i++) {
      if ((t1 | TIME_FAR_MASK) == (t2 | TIME_FAR_MASK)) {
        break;
      }
      t1 >>= TIME_FAR_SHIFT;
      t2 >>= TIME_FAR_SHIFT;
    }
    timelist_add(&ptw->_far[i][(t2 & TIME_FAR_MASK)], node);
  }
}

static void timewheel_shift(twheel_t* ptw)
{
  if (++ptw->curr_time > MAX_TIME) {
    return;
  }
  size_t t = ptw->curr_time;
  if (!(t & TIME_NEAR_MASK)) {
    t >>= TIME_NEAR_SHIFT;
    size_t level = 0;
    while (t) {
      if (t & TIME_FAR_MASK) {
        timenode* next = timelist_clear(&(ptw->_far[level][(t & TIME_FAR_MASK)]));
        while (next) {
          timenode* temp = next->next;
          timewheel_add_node(ptw, next);
          next = temp;
        }
        break;
      }
      t >>= TIME_FAR_SHIFT;
      level++;
    }
  }
}

static void timewheel_execute(twheel_t* ptw)
{
  timenode* next = timelist_clear(&(ptw->_near[(ptw->curr_time & TIME_NEAR_MASK)]));
  while (next) {
    if (next->callback.func) {
      next->callback.func(next->callback.arg);
      next->callback.arg = 0;
    }
    timenode* temp = next;
    next = next->next;
    if (ptw->free_total > MAX_NODE_CACHE) {
      free(temp);
    }
    else { //add node to free list
      ptw->free_total++;
      temp->next = ptw->freenode;
      ptw->freenode = temp;
    }
  }
}

twheel_t* timewheel_create(size_t t)
{
  twheel_t* ptw = (twheel_t*)malloc(sizeof(twheel_t));
  if (!ptw) {
    return nullptr;
  }
  memset(ptw, 0, sizeof(twheel_t));
  ptw->need_update = 0;
  ptw->start_time = t;
  ptw->curr_time = 0;
  ptw->freenode = 0;
  ptw->free_total = 0;

  for (size_t i = 0; i < TIME_NEAR; i++) {
    timelist_clear(&ptw->_near[i]);
  }
  for (size_t i = 0; i < MAX_FAR; i++) {
    for (size_t j = 0; j < TIME_FAR; j++) {
      timelist_clear(&ptw->_far[i][j]);
    }
  }
  return ptw;
}

void timewheel_release(twheel_t* ptw)
{
  if (!ptw) {
    return;
  }
  for (size_t i = 0; i < TIME_NEAR; i++) {
    auto p = timelist_clear(&ptw->_near[i]);
    while (p) {
      timenode* temp = p;
      p = p->next;
      free(temp);
    }
  }
  for (size_t i = 0; i < MAX_FAR; i++) {
    for (size_t j = 0; j < TIME_FAR; j++) {
      auto p = timelist_clear(&ptw->_far[i][j]);
      while (p) {
        timenode* temp = p;
        p = p->next;
        free(temp);
      }
    }
  }
  while (ptw->freenode) {
    timenode* temp = ptw->freenode;
    ptw->freenode = ptw->freenode->next;
    free(temp);
  }
  free(ptw);
}

void timewheel_update(twheel_t* ptw, size_t t)
{
  if (ptw->start_time == 0) {
    ptw->start_time = t;
  }
  if (t < ptw->start_time) {
    return;
  }
  size_t diff = t - ptw->start_time;
  if (diff < ptw->curr_time) {
    return;
  }
  diff = diff - ptw->curr_time;
  for (size_t i = 0; i < diff; i++) {
    timewheel_shift(ptw);
    timewheel_execute(ptw);
  }
}

bool timewheel_add_time(twheel_t* ptw, twfunction func, void* arg, size_t t)
{
  if (t <= 0 || t > MAX_DELAY_TIME) {
    return false;
  }
  size_t curr_time = ptw->curr_time;
  size_t delay_time = curr_time + t;

  timenode* node = ptw->freenode;
  if (node == 0) {
    node = (timenode*)malloc(sizeof(*node));
  }
  else {
    ptw->free_total--;
    ptw->freenode = ptw->freenode->next;
  }
  if (!node) {
    return false;
  }
  ptw->need_update = 1;
  memset(node, 0, sizeof(timenode));
  node->time = delay_time;
  node->callback.func = func;
  node->callback.arg = arg;
  node->next = 0;
  timewheel_add_node(ptw, node);
  return true;
}

/***********************************************************************************/
