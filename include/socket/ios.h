

#pragma once

/********************************************************************************/

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#ifndef ASIO_NO_DEPRECATED
#define ASIO_NO_DEPRECATED
#endif

#include <asio.hpp>
using namespace asio;

/********************************************************************************/
namespace io {
/********************************************************************************/

class identifiers final {
  friend class identifier;
  int _next;
  std::mutex _mutex;
  std::list<int> _free;

private:
  typedef std::shared_ptr<identifiers> value;

  int pop() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (_next < 0xffff) {
      return ++_next;
    }
    int id = 0;
    if (!_free.empty()) {
      id = _free.front();
      _free.pop_front();
    }
    return id;
  }

  void push(int id) {
    std::unique_lock<std::mutex> lock(_mutex);
    _free.push_back(id);
  }

  static value instance() {
    static value __instance(new identifiers());
    return __instance;
  }

  identifiers() : _next(0) {}
};

/********************************************************************************/

class identifier final {
  int _id;
  identifiers::value _pool;
  identifier(const identifier&) = delete;

public:
  inline identifier() {
    _pool = identifiers::instance();
    _id = _pool->pop();
    assert(_id > 0);
  }

  virtual ~identifier() {
    _pool->push(_id);
  }

  typedef int value_type;
  inline int value() const { return _id; }
};

/********************************************************************************/

class service : public io_context {
  typedef io_context parent;
  typedef executor_work_guard<service::executor_type> work_guard_t;
  identifier _id;
  work_guard_t _work_guard;

  inline service()
    : parent()
    , _work_guard(make_work_guard(*this)) {
  }
  service(const service&) = delete;

public:
  template <typename Handler>
  void post(Handler handler) { ::post(*this, handler); }

  template <typename Handler>
  void dispatch(Handler handler) { ::dispatch(*this, handler); }

  inline int id() const { return _id.value(); }

public:
  typedef std::shared_ptr<service> value;
  static value create() { return value(new service()); }
};

/********************************************************************************/
} //end of namespace io
/********************************************************************************/
