

#include "luaos.h"
#include "luaos_compile.h"

/*******************************************************************************/

static eth::encoder encoder;
static eth::decoder decoder;
static std::mutex   fmutex;
static std::string  fromname;
static std::map<std::string, std::string> fluadata;

/*******************************************************************************/

static std::string readfile(const char* filename)
{
  std::string data;
  FILE* fp = fopen(filename, "rb");
  if (fp){
    char buffer[8192];
    while (!feof(fp)) {
      size_t n = fread(buffer, 1, sizeof(buffer), fp);
      data.append(buffer, n);
    }
    fclose(fp);
  }
  return data;
}

static int luaos_build(const char* filename, FILE* fp)
{
  std::string data(filename, strlen(filename) + 1);
  data.append(readfile(filename));
  if (data.empty()) {
    luaos_error("Can't open input file: %s\n", filename);
    return 0;
  }
  encoder.encode(0, data.c_str(), data.size(), true, true, [fp](const char* data, size_t size) {
    fwrite(data, 1, size, fp);
  });
  luaos_trace("%s compile OK\n", filename);
  return 0;
}

static int luaos_build(const char* path, const char* output, FILE* fp, const std::set<std::string>& exts)
{
  int count = 0;
  _tinydir_char_t fdir[_TINYDIR_PATH_MAX];
  _tinydir_strcpy(fdir, path);

  tinydir_dir tdir;
  tinydir_open(&tdir, fdir);
  char filename[_TINYDIR_PATH_MAX];

  while (tdir.has_next)
  {
    tinydir_file file;
    tinydir_readfile(&tdir, &file);
    tinydir_next(&tdir);

    if (file.is_dir == 0) {
      if (fromname == file.name) {
        continue;
      }
      auto iter = exts.find(file.extension);
      if (iter == exts.end()) {
        iter = exts.find("*");
      }
      if (iter != exts.end()) {
        count++;
        snprintf(filename, sizeof(filename), "%s/%s", path, file.name);
        luaos_build(filename, fp);
      }
      continue;
    }
    if (_tinydir_strcmp(file.name, ".") == 0) {
      continue;
    }
    if (_tinydir_strcmp(file.name, "..") == 0) {
      continue;
    }
    char newdir[_TINYDIR_PATH_MAX];
    sprintf(newdir, "%s/%s", fdir, file.name);
    count += luaos_build(newdir, output, fp, exts);
  }

  tinydir_close(&tdir);
  return count;
}

static void makefiledir(const char* filename)
{
  std::string dirname;
  while (*filename) {
    if (*filename == '/') {
      if (dirname.size() && dirname != ".") {
        dir::make(dirname.c_str());
      }
    }
    dirname += *filename++;
  }
}

static void unpackfile(const char* infile, const char* name, const char* data, size_t size)
{
  std::string root;
  const char* pos = strrchr(infile, '/');
  if (pos) {
    root.append(infile, pos - infile + 1);
  }
  if (name[0] == '.' && name[1] == '/') {
    name += 2;
  }
  std::string fullname(root + name);
  makefiledir(fullname.c_str());
  FILE* fp = fopen(fullname.c_str(), "rb");
  if (fp)
  {
    std::string file;
    char buffer[8192];
    while (!feof(fp)) {
      size_t n = fread(buffer, 1, sizeof(buffer), fp);
      file.append(buffer, n);
    }
    fclose(fp);
    if (size == file.size()) {
      if (memcmp(data, file.c_str(), size) == 0) {
        return;
      }
    }
  }
  luaos_trace("Unpacking: %s\n", fullname.c_str());
  fp = fopen(fullname.c_str(), "wb");
  if (!fp) {
    luaos_error("Can't create file: %s", fullname.c_str());
    return;
  }
  fwrite(data, 1, size, fp);
  fclose(fp);
}

int luaos_parse(lua_State* L, const char* filename)
{
  std::string data = readfile(filename);
  if (data.empty()) {
    luaos_error("Cat't open input file: %s\n\n", filename);
    return -1;
  }
  int count = 0;
  size_t size = decoder.write(data.c_str(), data.size());
  int result = decoder.decode([&](const char* data, size_t size, const eth::decoder::header* head) {
    const char* name = data;
    data = data + strlen(name) + 1;
    size = size - strlen(name) - 1;

    const char* pos = strrchr(name, '.');
    if (pos && _tinydir_strcmp(pos + 1, "lua") == 0) {
      count++;
      fluadata[std::string(name)] = std::string(data, size);
      return;
    }
    unpackfile(filename, name, data, size);
  });
  size = decoder.size();
  if (size || result != 0) {
    luaos_error("%s parse error\n\n", filename);
    return -1;
  }
  fromname = filename;
  return count;
}

int luaos_loadlua(lua_State* L, const char* filename)
{
  std::string temp(filename);
  for (size_t i = 0; i < temp.size(); i++) {
    if (temp[i] == '\\') {
      temp[i] = '/';
    }
  }
  filename = temp.c_str();

  std::unique_lock<std::mutex> lock(fmutex);
  auto iter = fluadata.find(filename);
  if (iter == fluadata.end()) {
    return 0;
  }
  lua_pushlstring(L, fromname.c_str(), fromname.size());
  lua_pushlstring(L, iter->second.c_str(), iter->second.size());
  return 2;
}

int luaos_compile(lua_State* L, const char* filename, const std::set<std::string>& exts)
{
  FILE* fp = fopen(filename, "wb");
  if (!fp) {
    luaos_error("Can't open output file: %s\n", filename);
    return 0;
  }
  fromname = filename;
  int count = luaos_build(".", fromname.c_str(), fp, exts);
  fclose(fp);
  return count;
}

/*******************************************************************************/
