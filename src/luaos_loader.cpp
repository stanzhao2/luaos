
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

#include <string.h>
#include <assert.h>
#include <malloc.h>

/***********************************************************************************/

#include "luaos.h"
#include "luaos_loader.h"

typedef struct {
  const char* data; //data buffer
  size_t size;      //data length
} file_buffer;

#define LTYPE(i) luaL_typename(L, i)

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM == 501
# define LUA_LOADER_NAME "loaders"
# define LUA_LOAD(a, b, c, d) lua_load(a, b, c, d)
#else
# define LUA_LOADER_NAME "searchers"
# define LUA_LOAD(a, b, c, d) lua_load(a, b, c, d, 0)
#endif

/***********************************************************************************/

static lua_CFunction hook_loader = 0;

/* private / internal */
static const char* skipBOM(const char* buff, size_t* size)
{
  const char *p = "\xEF\xBB\xBF";  /* UTF-8 BOM mark */
  for (; (*size) > 0 && *p != '\0'; buff++, (*size)--) {
    if (*buff != *p++) break;
  }
  return buff;
}

/* private / internal */
static const char* lua_reader(lua_State* L, void* ud, size_t* size)
{
  file_buffer* fb = (file_buffer*)ud;
  (void)L;  /* not used */

  if (fb->size == 0) return NULL;
  fb->data = skipBOM(fb->data, &fb->size);

  *size = fb->size;
  fb->size = 0;
  return fb->data;
}

/* private / internal */
static int is_fullname(const char* filename)
{
  char c1, c2, c3;
  if (strlen(filename) < 3) {
    return 0;
  }
  c1 = filename[0];
  c2 = filename[1];
  c3 = filename[2];

  if (LUA_DIRSEP[0] == '/') { //linux
    return (c1 == '/');
  }
  if (c2 != ':' || c3 != LUA_DIRSEP[0]) {
    return 0;
  }
  return (c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z');
}

/* private / internal */
static int ll_loader(lua_State* L, const char* buff, size_t size, const char* name)
{
  file_buffer fb;
  fb.data = buff;
  fb.size = size;
  return LUA_LOAD(L, lua_reader, &fb, name);
}

/* private / internal */
static int ll_loadbuffer(lua_State* L, const char* buff, size_t size, const char* filename)
{
#ifdef OS_WINDOWS
  std::string modename(filename);
  char* p = (char*)modename.c_str();
  for (; *p; p++) {
    if (*p == '\\') *p = '/';
  }
  filename = modename.c_str();
#endif

  char luaname[1024];
  int topindex = lua_gettop(L);
  snprintf(luaname, sizeof(luaname), "@%s", filename);

  if (ll_loader(L, buff, size, luaname) != LUA_OK) {
    lua_error(L);
  }
  lua_pushfstring(L, "%s", luaname);
  int topnow = lua_gettop(L);
  return topnow - topindex;
}

/* private / internal */
static int ll_loadfile(lua_State* L, const char* filename)
{
  int result;
  size_t filesize, readed;
  char* buffer = 0, c;
  FILE* fp = fopen(filename, "r");

  if (!fp)
  {
    if (!hook_loader) {
      return 0;
    }
    lua_pushcfunction(L, hook_loader);
    lua_pushstring(L, filename);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
      lua_pop(L, 1);
      return 0;
    }
    //check result type
    if (lua_type(L, -1) != LUA_TSTRING) {
      lua_pop(L, 1);
      return 0;
    }
    //get result
    buffer = (char*)luaL_checklstring(L, -1, &readed);
  }
  else
  {
    c = (char)fgetc(fp);
    if (c == LUA_SIGNATURE[0])
    {
      /* reopen in binary mode */
      fp = freopen(filename, "rb", fp);
      if (!fp) {
        return 0;
      }
    }
    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    buffer = (char*)malloc(filesize);

    if (!buffer)
    {
      fclose(fp);
      return 0;
    }
    fseek(fp, 0, SEEK_SET);
    readed = fread(buffer, 1, filesize, fp);
    fclose(fp);
  }
  if (!buffer) {
    return 0;
  }
  result = ll_loadbuffer(L, buffer, readed, filename);
  if (fp) {
    free(buffer);
  }
  else {
    lua_remove(L, -1 - result);
  }
  return result;
}

/* private / internal */
static int ll_require(lua_State* L)
{
  char fullname[512], path[8192];
  size_t size, i = 0, j = 0;
  const char* name = luaL_checklstring(L, 1, &size);

  std::string filename(name);
  for (size_t i = 0; i < filename.size(); i++) {
    if (filename[i] == '.') {
      filename[i] = LUA_DIRSEP[0];
    }
  }
  name = filename.c_str();
  if (is_fullname(name)) {
    return ll_loadfile(L, name);
  }

  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_getfield(L, -1, "path");
  strcpy(path, luaL_checkstring(L, -1));
  lua_pop(L, 2);

  while (path[i])
  {
    const char chr = path[i++];
    if (chr == '?')
    {
      memcpy(fullname + j, name, size);
      j += size;
      continue;
    }
    if (chr == ';')
    {
      fullname[j] = 0;
      int result = ll_loadfile(L, fullname);
      if (result > 0) {
        return result;
      }
      j = 0;
      continue;
    }
    fullname[j++] = chr;
  }
  fullname[j] = 0;
  return j ? ll_loadfile(L, fullname) : 0;
}

/* private / internal */
static int ll_dofile(lua_State* L)
{
  lua_pushcfunction(L, lua_pcall_error);
  int error_fn_index = lua_gettop(L);

  if (ll_require(L) <= 0) {
    lua_pop(L, 1);
    return luaL_error(L, "module %s not found", luaL_checkstring(L, -1));
  }

  lua_rotate(L, -2, 1);
  lua_pcall(L, 0, LUA_MULTRET, error_fn_index);
  lua_pop(L, lua_gettop(L));
  return LUA_OK;
}

/***********************************************************************************/

/* export function */
int lua_new_loader(lua_State* L, lua_CFunction loader)
{
  hook_loader = loader;
  lua_getglobal(L, LUA_LOADLIBNAME); //-3
  lua_getfield(L, -1, LUA_LOADER_NAME); //-2
  lua_pushcfunction(L, ll_require); //-1
  lua_rawseti(L, -2, 2);
  lua_pop(L, 2);
  lua_register(L, "dofile", ll_dofile);
  return LUA_OK;
}

/***********************************************************************************/

/* export function */
int lua_dofile(lua_State* L, const char* filename)
{
  assert(filename);
  const char* pend;
  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_getfield(L, -1, LUA_LOADER_NAME);
  lua_rawgeti(L, -1, 2);

  if (lua_tocfunction(L, -1) != ll_require) {
    lua_pop(L, 3);
    return luaL_dofile(L, filename);
  }
  lua_pop(L, 3);

  lua_pushcfunction(L, lua_pcall_error);
  int error_fn_index = lua_gettop(L);

  lua_pushcfunction(L, ll_dofile);
  pend = strrchr(filename, '.');
  if (pend) {
    lua_pushlstring(L, filename, pend - filename);
  }
  else {
    lua_pushstring(L, filename);
  }
  int result = lua_pcall(L, 1, LUA_MULTRET, error_fn_index);
  lua_remove(L, (result == LUA_OK) ? -1 : -2);
  return result;
}

/***********************************************************************************/

/* export function */
int lua_pstack(lua_State* L)
{
  int topindex = lua_gettop(L);
  if (topindex < 1) {
    return 0;
  }
#ifdef _DEBUG
  printf("-------- top of stack -----------\n");
  for (int i = topindex; i > 0; i--)
  {
    int type = lua_type(L, i);
    switch (type)
    {
    case LUA_TNIL:
      printf(
        "#%02d: %s %s\n", i, LTYPE(i), "nil"
      );
      break;
    case LUA_TBOOLEAN:
      printf(
        "#%02d: %s %d\n", i, LTYPE(i)
        , lua_toboolean(L, i)
      );
      break;
    case LUA_TLIGHTUSERDATA:
      printf(
        "#%02d: %s 0x%p\n", i, LTYPE(i)
        , lua_touserdata(L, i)
      );
      break;
    case LUA_TNUMBER:
      printf(
        "#%02d: %s %lf\n", i, LTYPE(i)
        , lua_tonumber(L, i)
      );
      break;
    case LUA_TSTRING:
      printf(
        "#%02d: %s %s\n", i, LTYPE(i)
        , lua_tostring(L, i)
      );
      break;
    case LUA_TTABLE:
      printf(
        "#%02d: %s 0x%p\n", i, LTYPE(i)
        , lua_topointer(L, i)
      );
      break;
    case LUA_TFUNCTION:
      printf(
        "#%02d: %s 0x%p\n", i, LTYPE(i)
        , lua_topointer(L, i)
      );
      break;
    case LUA_TUSERDATA:
      printf(
        "#%02d: %s 0x%p\n", i, LTYPE(i)
        , lua_touserdata(L, i)
      );
      break;
    case LUA_TTHREAD:
      printf(
        "#%02d: %s 0x%p\n", i, LTYPE(i)
        , lua_tothread(L, i)
      );
      break;
    default:
      printf("#%02d: unknow type\n", i);
      break;
    }
  }
  printf("-------- bottom of stack --------\n\n");
#endif
  return topindex;
}

/***********************************************************************************/
