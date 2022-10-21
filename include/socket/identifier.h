
/********************************************************************************
** 
** Copyright 2021-2022 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
** 
********************************************************************************/

#pragma once

#include <list>
#include "mutex.h"

/*******************************************************************************/

class identifier final
{
public:
  typedef int value_type;
  inline identifier()
    : _value(generator::instance()->next()) {
  }
  inline ~identifier()
  {
    generator::instance()->push(_value);
  }
  inline operator int() const
  {
    return _value;
  }
  inline value_type value() const
  {
    return _value;
  }
  inline static value_type max_value()
  {
    return 0xffff;
  }
  identifier(const identifier&) = delete;
  identifier& operator= (const identifier&) = delete;

private:
  class generator final
  {
  public:
    static generator* instance()
    {
      static generator gen(1);
      return &gen;
    }
    value_type next()
    {
      value_type id = 0;
      local_unique_lock lock(_mutex);
      if (_next < max_value() + 1) {
        id = _next++;
      }
      else if (!_free.empty()) {
        id = _free.front();
        _free.pop_front();
      }
      assert(id > 0);
      return id;
    }
    void push(value_type id)
    {
      assert(id > 0);
      local_unique_lock lock(_mutex);
      _free.push_back(id);
    }

  private:
    lock_type _mutex;
    value_type _next;
    std::list<value_type> _free;
    generator(value_type init) : _next(init) { }
  };
  const value_type _value;
};

/*******************************************************************************/
