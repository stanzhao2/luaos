

#include "luaos.h"

/*******************************************************************************/

static const std::string strbuild("~build");
static std::string curpath(strbuild);

/*******************************************************************************/

static const char* find_file(lua_State* L, const char* name)
{
  char path[1024];
  lua_getglobal(L, LUA_LOADLIBNAME);
  lua_getfield(L, -1, "path");
  strcpy(path, luaL_checkstring(L, -1));
  lua_pop(L, 2);

  static char filename[1024];
  size_t i = 0, j = 0;
  size_t namelen = strlen(name);

  while (path[i])
  {
    const char chr = path[i++];
    if (chr == '?')
    {
      memcpy(filename + j, name, namelen);
      j += namelen;
      continue;
    }
    if (chr == ';')
    {
      filename[j] = 0;
      int result = _access(filename, 0);
      if (result == 0) {
        return filename;
      }
      j = 0;
      continue;
    }
    filename[j++] = chr;
  }
  filename[j] = 0;
  int result = _access(filename, 0);
  return result == 0 ? filename : nullptr;
}

static int copyfile(const char* src, const char* target)
{
  FILE* fpr = fopen(src, "rb");
  if (!fpr) {
    luaos_error("%s open error\n", src);
    return 1;
  }
  char buffer[8192];
  std::string data;
  while (!feof(fpr)) {
    auto size = fread(buffer, 1, sizeof(buffer), fpr);
    data.append(buffer, size);
  }
  fclose(fpr);

  FILE* fpw = fopen(target, "wb");
  if (!fpw) {
    luaos_error("%s open error\n", target);
    return 1;
  }
  fwrite(data.c_str(), 1, data.size(), fpw);
  fclose(fpw);
  return 0;
}

static int luaos_build(const char* path, const tinydir_file& file)
{
  if (file.is_dir)
  {
    char folder[256];
    snprintf(folder, sizeof(folder), "%s/%s/%s", strbuild.c_str(), path, file.name);
    dir::make(folder);
    return 0;
  }
  char filename[1024];
  snprintf(filename, sizeof(filename), "%s/%s", path, file.name);
  char target[1024];
  snprintf(target, sizeof(target), "%s/%s", strbuild.c_str(), filename);
  /* not lua file */
  if (_tinydir_strcmp(file.extension, "lua")) {
    if (copyfile(filename, target) == 0) {
      luaos_trace("%s copy OK\n", filename);
      return 0;
    }
    luaos_error("%s copy error\n", filename);
    return 1;
  }
  char strcmd[_TINYDIR_PATH_MAX];
  sprintf(strcmd, "luac -o %s %s", target, filename);
  if (system(strcmd) == 0) {
    luaos_trace("%s compile OK\n", filename);
    return 1;
  }
  luaos_trace("%s compile error\n", filename);
  return 0;
}

static int luaos_build(const char* path)
{
  _tinydir_char_t fdir[_TINYDIR_PATH_MAX];
  _tinydir_strcpy(fdir, path);

  static thread_local int count = 0;
  tinydir_dir tdir;
  tinydir_open(&tdir, fdir);
  while (tdir.has_next)
  {
    tinydir_file file;
    tinydir_readfile(&tdir, &file);
    tinydir_next(&tdir);

    if (file.is_dir == 0) {
      count += luaos_build(fdir, file);
      continue;
    }
    if (_tinydir_strcmp(file.name, ".") == 0) {
      continue;
    }
    if (_tinydir_strcmp(file.name, "..") == 0) {
      continue;
    }
    if (_tinydir_strcmp(file.name, strbuild.c_str()))
    {
      count += luaos_build(fdir, file);
      char newdir[_TINYDIR_PATH_MAX];
      sprintf(newdir, "%s/%s", fdir, file.name);

      std::string oldpath(curpath);
      curpath = strbuild + "/" + std::string(newdir);
      luaos_build(newdir);
      curpath = oldpath;
    }
  }
  tinydir_close(&tdir);
  return count;
}

int luaos_compile(lua_State* L, const char* filename)
{
  char* realname = (char*)find_file(L, filename);
  if (!realname) {
    luaos_error("%s not found\n", filename);
    return 0;
  }
  for (size_t i = 0; realname[i]; i++) {
    if (realname[i] == '\\') {
      realname[i] = '/';
    }
  }
  dir::make(curpath.c_str());
  std::string folder(strbuild + "/");
  const char* p = filename;
  while (*p) {
    char c = *p;
    if (c == '/') {
      dir::make(folder.c_str());
    }
    folder += c;
    p++;
  }
  char filepath[1024];
  const char* pos = strrchr(realname, '/');
  memcpy(filepath, realname, pos - realname);
  filepath[pos - realname] = 0;
  return luaos_build(filepath);
}

/*******************************************************************************/
