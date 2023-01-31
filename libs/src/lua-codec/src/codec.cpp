

#include <lua.hpp>
#include <lua_wrapper.h>

#include "codec.h"

#define ENCODER_NAME "codec-encoder"
#define DECODER_NAME "codec-decoder"

static int new_encoder(lua_State* L)
{
  encoder* p_encoder = new encoder();
  if (!p_encoder) {
    return 0;
  }
  encoder** userdata = lexnew_userdata<encoder*>(L, ENCODER_NAME);
  if (!userdata) {
    delete p_encoder;
    return 0;
  }
  *userdata = p_encoder;
  return 1;
}

static int new_decoder(lua_State* L)
{
  decoder* p_decoder = new decoder();
  if (!p_decoder) {
    return 0;
  }
  decoder** userdata = lexnew_userdata<decoder*>(L, DECODER_NAME);
  if (!userdata) {
    delete p_decoder;
    return 0;
  }
  *userdata = p_decoder;
  return 1;
}

static encoder** check_encoder(lua_State* L)
{
  encoder** self = lexget_userdata<encoder*>(L, 1, ENCODER_NAME);
  if (!self || !*self) {
    return nullptr;
  }
  return self;
}

static decoder** check_decoder(lua_State* L)
{
  decoder** self = lexget_userdata<decoder*>(L, 1, DECODER_NAME);
  if (!self || !*self) {
    return nullptr;
  }
  return self;
}

static int encoder_gc(lua_State* L)
{
  encoder** self = check_encoder(L);
  if (self) {
    delete *self;
    *self = nullptr;
  }
  return 0;
}

static int decoder_gc(lua_State* L)
{
  decoder** self = check_decoder(L);
  if (self) {
    delete *self;
    *self = nullptr;
  }
  return 0;
}

static int encoder_close(lua_State* L)
{
  return encoder_gc(L);
}

static int decoder_close(lua_State* L)
{
  return decoder_gc(L);
}

static int codec_encode(lua_State* L)
{
  encoder** self = check_encoder(L);
  if (!self) {
    return 0;
  }
  size_t size = 0;
  const char* data = luaL_checklstring(L, 2, &size);
  bool encrypt = lexget_optboolean(L, 3, false);
  bool compress = lexget_optboolean(L, 4, false);

  encoder* p_encoder = *self;
  std::string packet = p_encoder->encode(data, size, encrypt, compress);
  lua_pushlstring(L, packet.c_str(), packet.size());
  return 1;
}

static int codec_decode(lua_State* L)
{
  decoder** self = check_decoder(L);
  if (!self) {
    return 0;
  }
  size_t size = 0;
  const char* data = luaL_checklstring(L, 2, &size);
  bool decrypt = lexget_optboolean(L, 3, false);

  if (!lua_isfunction(L, 4)) {
    luaL_argerror(L, 4, "must be a function");
  }

  decoder* p_decoder = *self;
  p_decoder->write(data, size, decrypt);

  int error = p_decoder->decode([L](const char* data, size_t size) {
    lua_pushvalue(L, 4);
    lua_pushinteger(L, 0);
    lua_pushlstring(L, data, size);
    lua_pcall(L, 2, 0, 0);
  });
  if (error != 0)
  {
    lua_pushvalue(L, 4);
    lua_pushinteger(L, error);
    lua_pushlstring(L, data, size);
    lua_pcall(L, 2, 0, 0);
    return 0;
  }
  lua_pushinteger(L, (lua_Integer)p_decoder->size());
  return 1;
}

/*******************************************************************************/

static int encoder_metatable(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "__gc",       encoder_gc      },
    { "close",      encoder_close   },
    { "encode",     codec_encode    },
    { NULL,         NULL            }
  };

  lexnew_metatable(L, ENCODER_NAME, methods);
  lua_pop(L, 1);
  return 0;
}

static int decoder_metatable(lua_State* L)
{
  struct luaL_Reg methods[] = {
    { "__gc",       decoder_gc      },
    { "close",      decoder_close   },
    { "decode",     codec_decode   },
    { NULL,         NULL           }
  };

  lexnew_metatable(L, DECODER_NAME, methods);
  lua_pop(L, 1);
  return 0;
}

/*******************************************************************************/

extern "C" int luaopen_codec_encoder(lua_State* L)
{
  encoder_metatable(L);
  struct luaL_Reg methods[] = {
    { "new",          new_encoder   },
    { NULL,           NULL          },
  };
  lua_newtable(L);
  luaL_setfuncs(L, methods, 0);
  return 1;
}

extern "C" int luaopen_codec_decoder(lua_State* L)
{
  decoder_metatable(L);
  struct luaL_Reg methods[] = {
    { "new",          new_decoder   },
    { NULL,           NULL          },
  };
  lua_newtable(L);
  luaL_setfuncs(L, methods, 0);
  return 1;
}

/*******************************************************************************/
