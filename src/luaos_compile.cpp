

#include "luaos.h"
#include "luaos_compile.h"

/*******************************************************************************/

static bool is_debug = true;
static std::shared_ptr<eth::encoder> encoder;
static std::shared_ptr<eth::decoder> decoder;
static std::mutex   fmutex;
static std::string  fromname;
static std::map<std::string, std::string> fluadata;

/*******************************************************************************/

#ifndef _MSC_VER
static const char* permissions(const char *filename)
{
  struct stat st;
  static thread_local char modeval[128];
  if (stat(filename, &st) == 0)
  {
    mode_t perm = st.st_mode;
    modeval[0] = (perm & S_IRUSR) ? 'r' : '-';
    modeval[1] = (perm & S_IWUSR) ? 'w' : '-';
    modeval[2] = (perm & S_IXUSR) ? 'x' : '-';
    modeval[3] = (perm & S_IRGRP) ? 'r' : '-';
    modeval[4] = (perm & S_IWGRP) ? 'w' : '-';
    modeval[5] = (perm & S_IXGRP) ? 'x' : '-';
    modeval[6] = (perm & S_IROTH) ? 'r' : '-';
    modeval[7] = (perm & S_IWOTH) ? 'w' : '-';
    modeval[8] = (perm & S_IXOTH) ? 'x' : '-';
    modeval[9] = '\0';
    return modeval;     
  }
  else{
    return strerror(errno);
  }   
}

static int unpermissions(const char* permis)
{
  int modes[] = {
    S_IRUSR, S_IWUSR, S_IXUSR,
    S_IRGRP, S_IWGRP, S_IXGRP,
    S_IROTH, S_IWOTH, S_IXOTH
  };

  int result = 0;
  for (int i = 0; i < 9; i++)
  {
    const char c = permis[i];
    switch (c){
    case 'r': result |= modes[((i / 3) * 3)];
      break;
    case 'w': result |= modes[((i / 3) * 3) + 1];
      break;
    case 'x': result |= modes[((i / 3) * 3) + 2];
      break;
    }
  }
  return result;
}
#endif

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
  int opcode = 0;
  std::string data(filename, strlen(filename) + 1);
#ifndef _MSC_VER
  const char* permis = permissions(filename);
  if (permis) {
    opcode = 1;
    data.append(permis, strlen(permis) + 1);
  } else {
    luaos_error("Cat't get permissions: %s\n", filename);
    return 0;
  }
#endif
  data.append(readfile(filename));
  if (data.empty()) {
    luaos_error("Can't open input file: %s\n", filename);
    return 0;
  }
  encoder->encode(opcode, data.c_str(), data.size(), true, true, [fp](const char* data, size_t size) {
    fwrite(data, 1, size, fp);
  });
  luaos_trace("%s build OK\n", filename);
  return 1;
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
        iter = exts.find("all");
      }
      if (iter != exts.end()) {
        snprintf(filename, sizeof(filename), "%s/%s", path, file.name);
        count += luaos_build(filename, fp);
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

static int unpackfile(const char* infile, const char* name, const char* data, size_t size)
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
        return 0;
      }
    }
  }
  fp = fopen(fullname.c_str(), "wb");
  if (!fp) {
    luaos_error("Can't create file: %s\n", fullname.c_str());
    return 0;
  }
  fwrite(data, 1, size, fp);
  fclose(fp);
  return 1;
}

int luaos_is_debug(lua_State* L)
{
  return is_debug ? 1 : 0;
}

int luaos_export(lua_State* L, const char* filename, const char* key, bool all)
{
  if (!decoder) {
    decoder.reset(new eth::decoder(key, (key && key[0]) ? strlen(key) : 0));
  }
  std::string data = readfile(filename);
  if (data.empty()) {
    luaos_error("Cat't open input file: %s\n", filename);
    return -1;
  }
  int count = 0;
  size_t size = decoder->write(data.c_str(), data.size());
  int result = decoder->decode([&](const char* data, size_t size, const eth::decoder::header* head)
  {
    const char* name = data;
    data = data + strlen(name) + 1;
    size = size - strlen(name) - 1;

    const char* permis = nullptr;
    if (head->opcode) { /* linux */
      permis = data;
      data = data + strlen(permis) + 1;
      size = size - strlen(permis) - 1;
    }

    const char* pos = strrchr(name, '.');
    if (!all && pos) {
      if (_tinydir_strcmp(pos + 1, "lua") == 0) {
        count++;
        fluadata[std::string(name)] = std::string(data, size);
        is_debug = false;
        return;
      }
    }
    if (unpackfile(filename, name, data, size)) {
#ifndef _MSC_VER
      if (permis && permis[0]) {
        if (chmod(name, unpermissions(permis))) {
          luaos_error("Cat't set permissions: %s\n", filename);
          return;
        }
      }
#endif
      count++;
      luaos_trace("%s unpack OK\n", name);
    }
  });
  size = decoder->size();
  if (size || result != 0) {
    luaos_error("%s export error\n", filename);
    return -1;
  }
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

int luaos_compile(lua_State* L, const char* filename, const std::set<std::string>& exts, const char* key)
{
  if (!encoder) {
    encoder.reset(new eth::encoder(key, (key && key[0]) ? strlen(key) : 0));
  }
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
