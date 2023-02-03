
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

#include <unzip.h>
#include <zip.h>
#include <time.h>
#include <set>
#include <map>
#include <xxtea.h>
#include <conv.h>

#include "luaos.h"
#include "luaos_thread_local.h"

static std::set<std::string> lualist;
static std::map<std::string, std::string> lua_romfiles;

/*******************************************************************************/

/* Initialize lua file list */
static void lua_build_list(const char* dir)
{
  dir_eachof(dir, "lua", [](const char* path, const tinydir_file& file)
  {
    if (!file.is_dir) {
      while (*path)
      {
        if (*path == '.' || *path == LUA_DIRSEP[0]) {
          path++;
          continue;
        }
        else {
          break;
        }
      }
      lualist.insert(std::string(path) + LUA_DIRSEP + file.name);
    }
  });
}

static void os_temp_path(size_t size, char* output)
{
#ifdef OS_WINDOWS
  GetTempPathA((int)size, output);
#else
  strcpy(output, "/tmp");
#endif
}

static void os_temp_filename(char* path, const char* name, int nx, char* output)
{
#ifdef OS_WINDOWS
  GetTempFileNameA(path, name, nx, output);
#else
  strcat(output, "/~luac.tmp");
#endif
}

static void os_remove_file(const char* fname)
{
#ifdef OS_WINDOWS
  DeleteFileA(fname);
#else
  remove(fname);
#endif
}

/* compile and encrypt lua file */
static bool lua_encode(const char* filename, std::string& data)
{
  char tmppath[_TINYDIR_PATH_MAX];
  char tmpfile[_TINYDIR_PATH_MAX];
  os_temp_path(sizeof(tmppath), tmppath);
  os_temp_filename(tmppath, "~luac", 0, tmpfile);

  char strcmd[_TINYDIR_PATH_MAX];
  sprintf(strcmd, ".%sluac -o %s %s", LUA_DIRSEP, tmpfile, filename);
  if (system(strcmd) != 0) {
    return false;
  }

  FILE *fp = fopen(tmpfile, "rb");
  if (!fp) {
    return false;
  }

  data.clear();
  char buffer[8192];
  while (!feof(fp))
  {
    size_t n = fread(buffer, 1, sizeof(buffer), fp);
    data.append(buffer, n);
  }

  fclose(fp);
  os_remove_file(tmpfile);
  if (!data.empty())
  {
    size_t size = data.size();
    size = (size >> 2) << 2;
    size = xxtea::encrypt(data.c_str(), (int)size, (char*)data.c_str(), 0);
  }
  return !data.empty();
}

/* write compressed file */
static bool lua_encode(zipFile zfile, const char* fnameinzip, const std::string& data)
{
  int nErr = 0;
  zip_fileinfo zinfo = { 0 };
  tm_zip tmz = { 0 };
  zinfo.tmz_date = tmz;
  zinfo.dosDate = 0;
  zinfo.internal_fa = 0;
  zinfo.external_fa = 0;

  char sznewfileName[256] = { 0 };
  strcat(sznewfileName, fnameinzip);
  if (data.empty()) {
    strcat(sznewfileName, LUA_DIRSEP);
  }

  nErr = zipOpenNewFileInZip(zfile, sznewfileName, &zinfo, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
  if (nErr == ZIP_OK) {
    if (!data.empty()) {
      nErr = zipWriteInFileInZip(zfile, data.c_str(), (unsigned int)data.size());
    }
  }

  zipCloseFileInZip(zfile);
  return nErr == ZIP_OK;
}

/* Compress all files in the specified path */
static bool lua_encode(zipFile zfile, const char* dir)
{
  bool result = true;
  lua_encode(zfile, dir, "");
  dir_eachof(dir, "lua", [&](const char* path, const tinydir_file& file)
  {
    if (file.is_dir) {
      return;
    }
    std::string data;
    std::string filename = std::string(path) + LUA_DIRSEP + file.name;

    if (!lua_encode(filename.c_str(), data)) {
      _printf(color_type::red, true, "%s compile failed\n");
      return;
    }

    if (!lua_encode(zfile, filename.c_str(), data)) {
      _printf(color_type::red, true, "%s compress failed\n");
      return;
    }

    _printf(
      color_type::normal, true, "%s compile OK\n", filename.c_str()
    );
  });
  return result;
}

/* Compress all files in the specified path */
static bool lua_encode(const char* romname)
{
#ifndef OS_WINDOWS
  if (access("./luac", 0) < 0) {
#else
  if (access(".\\luac.exe", 0) < 0) {
#endif
    _printf(color_type::red, true, "luac not found\n");
    return false;
  }
  bool result = true;
  zipFile zfile = zipOpen(romname, APPEND_STATUS_CREATE);
  if (!zfile) {
    _printf(
      color_type::red, true, "unable to create output file: %s\n", romname
    );
    return false;
  }

  const char* folders[] = {"lua", "usr", 0};
  for (int i = 0; folders[i]; i++) {
    if (!lua_encode(zfile, folders[i])) {
      result = false;
      break;
    }
  }

  zipClose(zfile, 0);
  return result;
}

static std::string get_tm_compile() {
  char szDateTime[128];
  const int  MONTH_PER_YEAR = 12;
  const char szEnglishMonth[MONTH_PER_YEAR][4] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  char szTmpDate[64]={0};
  char szTmpTime[32]={0};
  char szMonth[32]={0};
  int iYear,iMonth,iDay,iHour,iMin,iSec;
  sprintf(szTmpDate,"%s", __DATE__);
  sprintf(szTmpTime,"%s", __TIME__);

  sscanf(szTmpDate, "%s %d %d", szMonth, &iDay, &iYear);
  sscanf(szTmpTime, "%d:%d:%d", &iHour, &iMin, &iSec);
  for(int i=0;MONTH_PER_YEAR;i++) {
    if(strncmp(szMonth,szEnglishMonth[i], 3) == 0) {
      iMonth = i + 1;
      break;
    }
  }
  snprintf(szDateTime, sizeof(szDateTime), "%04d-%02d-%02d.%02d:%02d:%02d", iYear, iMonth, iDay, iHour, iMin, iSec);
  return szDateTime;
}

static bool lua_rom_unzip(const char* romname)
{
  lua_romfiles.clear();
  unzFile unzip = unzOpen(romname);
  if (unzip == nullptr) {
    return false;
  }
  unz_global_info* ginfo = new unz_global_info;
  auto result = unzGetGlobalInfo(unzip, ginfo);
  if (result != UNZ_OK) {
    delete ginfo;
    return false;
  }

  char zipname[256] = { 0 };
  zipname[0] = '.';
  zipname[1] = LUA_DIRSEP[0];
  char* ptr  = zipname + 2;

  unz_file_info* finfo = new unz_file_info;
  for (uLong i = 0; i < ginfo->number_entry; i++)
  {
    result = unzGetCurrentFileInfo(unzip, finfo, ptr, sizeof(zipname) - 2, 0, 0, 0, 0);
    if (result != UNZ_OK) {
      break;
    }
    for (size_t j = 0; zipname[j]; j++)
    {
#ifdef OS_WINDOWS
      if (zipname[j] == '/') { //is linux zip file
        zipname[j] = '\\';
      }
#else
      if (zipname[j] == '\\') { //is windows zip file
        zipname[j] = '/';
      }
#endif
    }
    result = unzOpenCurrentFile(unzip);
    if (result != UNZ_OK) {
      break;
    }
    std::string data;
    char buffer[8192];
    while (!unzeof(unzip))
    {
      int n = unzReadCurrentFile(unzip, buffer, sizeof(buffer));
      if (n > 0) {
        data.append(buffer, n);
      }
    }
    lua_romfiles[zipname] = std::move(data);
    unzCloseCurrentFile(unzip);
    unzGoToNextFile(unzip);
  }

  delete finfo;
  delete ginfo;
  unzClose(unzip);
  return true;
}

bool lua_compile(const char* romname)
{
  return lua_encode(romname);
}

bool lua_decode(const char* filename, const char* romname, std::string& data)
{
  data.clear();
  if (lua_romfiles.empty())
  {
    _printf(
      color_type::yellow, true, "please wait while the package is being expanded\n"
    );
    lua_rom_unzip(romname);
  }

  auto finded = lua_romfiles.find(filename);
  if (finded != lua_romfiles.end())
  {
    data = finded->second;
    if (!data.empty())
    {
      size_t size = data.size();
      size = (size >> 2) << 2;
      size = xxtea::decrypt(data.c_str(), (int)size, (char*)data.c_str(), 0);
    }
  }
  return !data.empty();
}

bool check_bom_header()
{
  bool ok = true;
  const char *bom = "\xEF\xBB\xBF";  /* UTF-8 BOM mark */
  lua_build_list(LUAOS_ROOT_PATH);

  auto iter = lualist.begin();
  for (; iter != lualist.end(); iter++)
  {
    bool this_ok = true;
    const char* filename = (*iter).c_str();
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
      continue;
    }

    unsigned char header[3] = { 0 };
    size_t n = fread(header, 1, 3, fp);
    if (memcmp(header, bom, 3) != 0) {
      this_ok = false;
    }

    if (this_ok == false)
    {
      std::string data;
      data.append((char*)header, n);

      while (!feof(fp))
      {
        char buffer[8192];
        n = fread(buffer, 1, sizeof(buffer), fp);
        data.append(buffer, n);
      }

      if (is_utf8(data.c_str(), data.size())) {
        this_ok = true;
      }
    }
    fclose(fp);
    if (this_ok) {
      continue;
    }
    ok = false;
    _printf(
      color_type::red, true, "%s check failed\n", filename
    );
  }
  return ok;
}

bool is_debug_mode(lua_State* L)
{
  bool result = false;
  lua_getglobal(L, "_DEBUG");
  if (lua_type(L, -1) == LUA_TBOOLEAN) {
    result = lua_toboolean(L, -1) ? true : false;
  }
  lua_pop(L, 1);
  return result;
}

LUALIB_API int lua_traceback(lua_State* L)
{
  std::string stack = luaL_checkstring(L, -1);
  bool showstack = false;

  for (int i = 1, j = 1; i < 100; i++)
  {
    lua_Debug ar;
    int result = lua_getstack(L, i, &ar);
    if (result == 0) {
      break;
    }
    if (lua_getinfo(L, "Sl", &ar))
    {
      if (ar.currentline < 0) {
        continue;
      }
      if (!showstack) {
        showstack = true;
        stack.append("\nstack traceback:");
      }
      char buffer[1024];
      sprintf(buffer, "\n\t#%02d: %s:%d: %s", j++, ar.short_src, ar.currentline, ar.what ? ar.what : "");
      stack.append(buffer);
    }
  }

  lua_pop(L, 1);
  lua_pushlstring(L, stack.c_str(), stack.size());
  return 1;
}

LUALIB_API int lua_pcall_error(lua_State* L)
{
  lua_traceback(L);
  _printf(color_type::red, true, "%s\n", luaL_checkstring(L, -1));
  return 1;
}

LUALIB_API int lua_os_files(lua_State* L)
{
  if (!lua_isfunction(L, 1)) {
    luaL_argerror(L, 1, "must be a function");
  }
  const char* dir = luaL_optstring(L, 2, ".");
  if (dir[0] == 0) {
    dir = ".";
  }
  const char* ext = luaL_optstring(L, 3, "*");
  if (ext[0] == 0) {
    ext = "*";
  }
  dir_eachof(dir, 0, 
    [L, ext](const char* path, const tinydir_file& file)
    {
      if (file.is_dir) {
        return;
      }
      if (_tinydir_stricmp(ext, "*")) {
        if (_tinydir_stricmp(file.extension, ext)) {
          return;
        }
      }
      while (*path)
      {
        if (*path == '.' || *path == LUA_DIRSEP[0]) {
          path++;
          continue;
        }
        else {
          break;
        }
      }
      std::string fullpath(path);
      fullpath += LUA_DIRSEP;
      fullpath += file.name;

      lua_pushcfunction(L, lua_pcall_error);
      int error_fn_index = lua_gettop(L);

      lua_pushvalue(L, 1);
      lua_pushlstring(L, fullpath.c_str(), fullpath.size());
      if (!file.extension) {
        lua_pushnil(L);
      }
      else {
        lua_pushstring(L, file.extension);
      }
      if (lua_pcall(L, 2, 0, error_fn_index) != LUA_OK) {
        lua_pop(L, 1); //pop error from stack
      }
      lua_pop(L, 1); //pop lua_pcall_error from stack
    }
  );
  return 0;
}

LUALIB_API int lua_os_id(lua_State* L)
{
  auto ios = this_thread().lua_reactor();
  lua_pushinteger(L, ios->id());
  return 1;
}

LUALIB_API int lua_os_typename(lua_State* L)
{
#if defined(os_windows)
  lua_pushstring(L, "windows");
#elif defined(os_bsdx)
  lua_pushstring(L, "bsd");
#elif defined(os_linux)
  lua_pushstring(L, "linux");
#elif defined(os_macx) || defined(os_mac9)
  lua_pushstring(L, "apple");
#endif
  return 1;
}

LUALIB_API int lua_is_utf8(lua_State* L)
{
  size_t size = 0;
  const char* data = luaL_checklstring(L, 1, &size);
  lua_pushboolean(L, is_utf8(data, size) ? 1 : 0);
  return 1;
}

void print_copyright()
{
  std::string logo = R"(
  '##:::::::'##::::'##::::'###:::::'#######:::'######::  (R)
   ##::::::: ##:::: ##:::'## ##:::'##.... ##:'##... ##:
   ##::::::: ##:::: ##::'##:. ##:: ##:::: ##: ##:::..::
   ##::::::: ##:::: ##:'##:::. ##: ##:::: ##:. ######::
   ##::::::: ##:::: ##: #########: ##:::: ##::..... ##:  __   ____________
   ##::::::: ##:::: ##: ##.... ##: ##:::: ##:'##::: ##:  | \\  |______  | 
   ########:. #######:: ##:::: ##:. #######::. ######:: .|  \\_|______  | 
  ........:::.......:::..:::::..:::.......::::......:::....................
  )";

  time_t now = time(0);
  struct tm* ptm = localtime(&now);
  char copyright[128];
  sprintf(copyright, "Copyright (C) 2021-%d, All right reserved\n\n", ptm->tm_year + 1900);
  logo += copyright;

  char version[128];
  sprintf(version, "  > + Depends: %s\n", LUA_RELEASE);
  logo += version;

  _printf(color_type::normal, false, logo.c_str());
  _printf(color_type::normal, false, "  > + LuaOS version number: v%s\n", LUAOS_VERSION);
#ifndef TLS_SSL_ENABLE
  _printf(color_type::normal, false, "  > + OpenSSL is disabled\n");
#endif
  _printf(color_type::normal, false, "  > + Final build at: %s\n\n", get_tm_compile().c_str());
}

/*******************************************************************************/
