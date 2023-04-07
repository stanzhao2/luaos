

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

class service : public io_context {
  typedef io_context parent;
  typedef executor_work_guard<service::executor_type> work_guard_t;
  work_guard_t _work_guard;

  inline service()
    : parent()
    , _work_guard(make_work_guard(*this)) {
  }

public:
  template <typename Handler>
  void post(Handler handler) { ::post(*this, handler); }

  template <typename Handler>
  void dispatch(Handler handler) { ::dispatch(*this, handler); }

public:
  typedef std::shared_ptr<service> value;
  static value create() { return value(new service()); }
};

/********************************************************************************/
} //end of namespace io
/********************************************************************************/
