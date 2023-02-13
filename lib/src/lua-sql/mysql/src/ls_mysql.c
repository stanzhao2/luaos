/*
** LuaSQL, MySQL driver
** Authors:  Eduardo Quintao
** See Copyright Notice in license.html
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

#ifdef _MSC_VER
#include <winsock2.h>
#define NO_CLIENT_LONG_LONG
#include <time.h>
#else
#include <sys/time.h>
#endif

#include <mysql.h>
#include <errmsg.h>

#include "lua.h"
#include "lauxlib.h"

#include "../../luasql.h"

#define LUASQL_ENVIRONMENT_MYSQL "MySQL environment"
#define LUASQL_CONNECTION_MYSQL "MySQL connection"
#define LUASQL_CURSOR_MYSQL "MySQL cursor"
#define LUASQL_STMT_MYSQL "MySQL stmt"
#define LUASQL_STMT_CURSOR_MYSQL "MySQL stmt cursor"

/* For compat with old version 4.0 */
#if (MYSQL_VERSION_ID < 40100) 
#define MYSQL_TYPE_VAR_STRING   FIELD_TYPE_VAR_STRING 
#define MYSQL_TYPE_STRING       FIELD_TYPE_STRING 
#define MYSQL_TYPE_DECIMAL      FIELD_TYPE_DECIMAL 
#define MYSQL_TYPE_SHORT        FIELD_TYPE_SHORT 
#define MYSQL_TYPE_LONG         FIELD_TYPE_LONG 
#define MYSQL_TYPE_FLOAT        FIELD_TYPE_FLOAT 
#define MYSQL_TYPE_DOUBLE       FIELD_TYPE_DOUBLE 
#define MYSQL_TYPE_LONGLONG     FIELD_TYPE_LONGLONG 
#define MYSQL_TYPE_INT24        FIELD_TYPE_INT24 
#define MYSQL_TYPE_YEAR         FIELD_TYPE_YEAR 
#define MYSQL_TYPE_TINY         FIELD_TYPE_TINY 
#define MYSQL_TYPE_TINY_BLOB    FIELD_TYPE_TINY_BLOB 
#define MYSQL_TYPE_MEDIUM_BLOB  FIELD_TYPE_MEDIUM_BLOB 
#define MYSQL_TYPE_LONG_BLOB    FIELD_TYPE_LONG_BLOB 
#define MYSQL_TYPE_BLOB         FIELD_TYPE_BLOB 
#define MYSQL_TYPE_DATE         FIELD_TYPE_DATE 
#define MYSQL_TYPE_NEWDATE      FIELD_TYPE_NEWDATE 
#define MYSQL_TYPE_DATETIME     FIELD_TYPE_DATETIME 
#define MYSQL_TYPE_TIME         FIELD_TYPE_TIME 
#define MYSQL_TYPE_TIMESTAMP    FIELD_TYPE_TIMESTAMP 
#define MYSQL_TYPE_ENUM         FIELD_TYPE_ENUM 
#define MYSQL_TYPE_SET          FIELD_TYPE_SET
#define MYSQL_TYPE_NULL         FIELD_TYPE_NULL

#define mysql_commit(_) ((void)_)
#define mysql_rollback(_) ((void)_)
#define mysql_autocommit(_,__) ((void)_)

#endif

typedef struct {
	short      closed;
} env_data;

typedef struct {
	short      closed;
	int        env;                /* reference to environment */
	MYSQL     *my_conn;
} conn_data;

typedef struct {
	short      closed;
	int        conn;               /* reference to connection */
	int        numcols;            /* number of columns */
	int        colnames, coltypes; /* reference to column information tables */
	MYSQL_RES *my_res;
	MYSQL 	  *my_conn;
} cur_data;

#if defined(MYSQL_VERSION_ID) && (MYSQL_VERSION_ID >= 80000)
# define my_bool bool
#endif

typedef struct {
	unsigned long length;
	my_bool is_null;
	union number_un
	{
		double d;
		int64_t l;
	} willnumber;
	void* willbuffer;

}stmt_param_data;

typedef struct {
	short      closed;
	int        stmt;              /* reference to stmt */
	int        numcols;            /* number of columns */
	int        colnames, coltypes; /* reference to column information tables */
	MYSQL_RES* my_res;
	MYSQL* my_conn;
	MYSQL_STMT* my_stmt;
	MYSQL_BIND* my_bindwill;
	stmt_param_data* my_paramwill;
} stmt_cur_data;

typedef struct {
	short      closed;
	int        conn;               /* reference to connection */

	MYSQL 	  *my_conn;
	MYSQL_STMT*my_stmt;		
	MYSQL_BIND*my_bind;

	stmt_param_data* bind_param;
	int need_bind_count;			/*需要绑定的参数数量*/
	int used_bind_index;			/*已经绑定的参数位置*/
} stmt_data;

static void FreeParamData(stmt_param_data* data, int len)
{
	if (data)
	{
		for (int i = 0; i < len; i++)
		{
			if (data[i].willbuffer)
			{
				free(data[i].willbuffer);
			}
		}
		free(data);
	}
}

/*
** Check for valid environment.
*/
static env_data *getenvironment (lua_State *L) {
	env_data *env = (env_data *)luaL_checkudata (L, 1, LUASQL_ENVIRONMENT_MYSQL);
	luaL_argcheck (L, env != NULL, 1, "environment expected");
	luaL_argcheck (L, !env->closed, 1, "environment is closed");
	return env;
}


/*
** Check for valid connection.
*/
static conn_data *getconnection (lua_State *L) {
	conn_data *conn = (conn_data *)luaL_checkudata (L, 1, LUASQL_CONNECTION_MYSQL);
	luaL_argcheck (L, conn != NULL, 1, "connection expected");
	luaL_argcheck (L, !conn->closed, 1, "connection is closed");
	return conn;
}


/*
** Check for valid cursor.
*/
static cur_data *getcursor (lua_State *L) {
	cur_data *cur = (cur_data *)luaL_checkudata (L, 1, LUASQL_CURSOR_MYSQL);
	luaL_argcheck (L, cur != NULL, 1, "cursor expected");
	luaL_argcheck (L, !cur->closed, 1, "cursor is closed");
	return cur;
}

/*
** Check for valid stmt cursor.
*/
static stmt_cur_data* getstmtcursor(lua_State* L) {
	stmt_cur_data* stmt_cur = (stmt_cur_data*)luaL_checkudata(L, 1, LUASQL_STMT_CURSOR_MYSQL);
	luaL_argcheck(L, stmt_cur != NULL, 1, "stmt cursor expected");
	luaL_argcheck(L, !stmt_cur->closed, 1, "stmt cursor is closed");
	return stmt_cur;
}

/*
** Push the value of #i field of #tuple row.
*/
static void pushvalue (lua_State *L, void *row, long int len) {
	if (row == NULL)
		lua_pushnil (L);
	else
		lua_pushlstring (L, (const char*)row, len);
}


/*
** Get the internal database type of the given column.
*/
static char *getcolumntype (enum enum_field_types type) {

	switch (type) {
		case MYSQL_TYPE_VAR_STRING: case MYSQL_TYPE_STRING:
			return "string";
		case MYSQL_TYPE_DECIMAL: case MYSQL_TYPE_SHORT: case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_FLOAT: case MYSQL_TYPE_DOUBLE: case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24: case MYSQL_TYPE_YEAR: case MYSQL_TYPE_TINY: 
			return "number";
		case MYSQL_TYPE_TINY_BLOB: case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB: case MYSQL_TYPE_BLOB:
			return "binary";
		case MYSQL_TYPE_DATE: case MYSQL_TYPE_NEWDATE:
			return "date";
		case MYSQL_TYPE_DATETIME:
			return "datetime";
		case MYSQL_TYPE_TIME:
			return "time";
		case MYSQL_TYPE_TIMESTAMP:
			return "timestamp";
		case MYSQL_TYPE_ENUM: case MYSQL_TYPE_SET:
			return "set";
		case MYSQL_TYPE_NULL:
			return "null";
		default:
			return "undefined";
	}
}


/*
** Creates the lists of fields names and fields types.
*/
static void create_colinfo (lua_State *L, cur_data *cur) {
	MYSQL_FIELD *fields;
	char typename1[50];
	int i;
	fields = mysql_fetch_fields(cur->my_res);
	lua_newtable (L); /* names */
	lua_newtable (L); /* types */
	for (i = 1; i <= cur->numcols; i++) {
		lua_pushstring (L, fields[i-1].name);
		lua_rawseti (L, -3, i);
		sprintf (typename1, "%.20s(%ld)", getcolumntype (fields[i-1].type), fields[i-1].length);
		lua_pushstring(L, typename1);
		lua_rawseti (L, -2, i);
	}
	/* Stores the references in the cursor structure */
	cur->coltypes = luaL_ref (L, LUA_REGISTRYINDEX);
	cur->colnames = luaL_ref (L, LUA_REGISTRYINDEX);
}


/*
** Closes the cursos and nullify all structure fields.
*/
static void cur_nullify (lua_State *L, cur_data *cur) {
	/* Nullify structure fields. */
	cur->closed = 1;
	mysql_free_result(cur->my_res);
	while (mysql_next_result(cur->my_conn) == 0) {
		mysql_free_result(mysql_store_result(cur->my_conn));
	}
	luaL_unref (L, LUA_REGISTRYINDEX, cur->conn);
	luaL_unref (L, LUA_REGISTRYINDEX, cur->colnames);
	luaL_unref (L, LUA_REGISTRYINDEX, cur->coltypes);
}

static void fix_cursor(lua_State* L, cur_data* cur, MYSQL_RES* result, int cols) {
	/* fix in structure */
	luaL_unref(L, LUA_REGISTRYINDEX, cur->colnames);
	luaL_unref(L, LUA_REGISTRYINDEX, cur->coltypes);

	cur->numcols = cols;
	cur->colnames = LUA_NOREF;
	cur->coltypes = LUA_NOREF;
	cur->my_res = result;
}
	
/*
** Get another row of the given cursor.
*/
static int cur_fetch (lua_State *L) {
	cur_data *cur = getcursor (L);
	MYSQL_RES *res = cur->my_res;
	unsigned long *lengths;
	MYSQL_ROW row = mysql_fetch_row(res);
	if (row == NULL) {
		cur_nullify (L, cur);
		lua_pushnil(L);  /* no more results */
		return 1;
	}
	lengths = mysql_fetch_lengths(res);

	if (lua_istable (L, 2)) {
		const char *opts = luaL_optstring (L, 3, "n");
		if (strchr (opts, 'n') != NULL) {
			/* Copy values to numerical indices */
			int i;
			for (i = 0; i < cur->numcols; i++) {
				pushvalue (L, row[i], lengths[i]);
				lua_rawseti (L, 2, i+1);
			}
		}
		if (strchr (opts, 'a') != NULL) {
			int i;
			/* Check if colnames exists */
			if (cur->colnames == LUA_NOREF)
		        create_colinfo(L, cur);
			lua_rawgeti (L, LUA_REGISTRYINDEX, cur->colnames);/* Push colnames*/
	
			/* Copy values to alphanumerical indices */
			for (i = 0; i < cur->numcols; i++) {
				lua_rawgeti(L, -1, i+1); /* push the field name */

				/* Actually push the value */
				pushvalue (L, row[i], lengths[i]);
				lua_rawset (L, 2);
			}
			/* lua_pop(L, 1);  Pops colnames table. Not needed */
		}
		lua_pushvalue(L, 2);
		return 1; /* return table */
	}
	else {
		int i;
		luaL_checkstack (L, cur->numcols, LUASQL_PREFIX"too many columns");
		for (i = 0; i < cur->numcols; i++)
			pushvalue (L, row[i], lengths[i]);
		return cur->numcols; /* return #numcols values */
	}
}

/*
** Get the next result from multiple statements
*/
static int cur_next_result (lua_State *L) {
	cur_data *cur = getcursor (L);
	MYSQL* con = cur->my_conn;
	int status;
	if(mysql_more_results(con)){
		mysql_free_result(cur->my_res);
		fix_cursor(L, cur, NULL, 0);
		status = mysql_next_result(con);
		if(status == 0){
			fix_cursor(L, cur, mysql_store_result(con), mysql_field_count(con));
			if(cur->my_res != NULL){
				lua_pushboolean(L, 1);
				return 1;
			}else{
				lua_pushboolean(L, 0);
				lua_pushinteger(L, mysql_errno(con));
				lua_pushstring(L, mysql_error(con));
				return 3;
			}
		}else{
			lua_pushboolean(L, 0);
			lua_pushinteger(L, status);
			switch(status){
				case CR_COMMANDS_OUT_OF_SYNC:
					lua_pushliteral(L, "CR_COMMANDS_OUT_OF_SYNC");
				break;
				case CR_SERVER_GONE_ERROR:
					lua_pushliteral(L, "CR_SERVER_GONE_ERROR");
				break;
				case CR_SERVER_LOST:
					lua_pushliteral(L, "CR_SERVER_LOST");
				break;
				case CR_UNKNOWN_ERROR:
					lua_pushliteral(L, "CR_UNKNOWN_ERROR");
				break;
				default:
					lua_pushliteral(L, "Unknown");
			}
			return 3;
		}
	}else{
		lua_pushboolean(L, 0);
		lua_pushinteger(L, -1);
		return 2;
	}
}

/*
** Check if next result is available
*/
static int cur_has_next_result (lua_State *L) {
	cur_data *cur = getcursor (L);
	lua_pushboolean(L, mysql_more_results(cur->my_conn));
	return 1;
}


/*
** Cursor object collector function
*/
static int cur_gc (lua_State *L) {
	cur_data *cur = (cur_data *)luaL_checkudata (L, 1, LUASQL_CURSOR_MYSQL);
	if (cur != NULL && !(cur->closed))
		cur_nullify (L, cur);
	return 0;
}


/*
** Close the cursor on top of the stack.
** Return 1
*/
static int cur_close (lua_State *L) {
	cur_data *cur = (cur_data *)luaL_checkudata (L, 1, LUASQL_CURSOR_MYSQL);
	luaL_argcheck (L, cur != NULL, 1, LUASQL_PREFIX"cursor expected");
	if (cur->closed) {
		lua_pushboolean (L, 0);
		return 1;
	}
	cur_nullify (L, cur);
	lua_pushboolean (L, 1);
	return 1;
}


/*
** Pushes a column information table on top of the stack.
** If the table isn't built yet, call the creator function and stores
** a reference to it on the cursor structure.
*/
static void _pushtable (lua_State *L, cur_data *cur, size_t off) {
	int *ref = (int *)((char *)cur + off);

	/* If colnames or coltypes do not exist, create both. */
	if (*ref == LUA_NOREF)
		create_colinfo(L, cur);
	
	/* Pushes the right table (colnames or coltypes) */
	lua_rawgeti (L, LUA_REGISTRYINDEX, *ref);
}
#define pushtable(L,c,m) (_pushtable(L,c,offsetof(cur_data,m)))


/*
** Return the list of field names.
*/
static int cur_getcolnames (lua_State *L) {
	pushtable (L, getcursor(L), colnames);
	return 1;
}


/*
** Return the list of field types.
*/
static int cur_getcoltypes (lua_State *L) {
	pushtable (L, getcursor(L), coltypes);
	return 1;
}


/*
** Push the number of rows.
*/
static int cur_numrows (lua_State *L) {
	lua_pushinteger (L, (lua_Integer)mysql_num_rows (getcursor(L)->my_res));
	return 1;
}


/*
** Seeks to an arbitrary row in a query result set.
*/
static int cur_seek (lua_State *L) {
	cur_data *cur = getcursor (L);
	lua_Integer rownum = luaL_checkinteger (L, 2);
	mysql_data_seek (cur->my_res, rownum);
	return 0;
}

/*
** Create a new Cursor object and push it on top of the stack.
*/
static int create_cursor (lua_State *L, MYSQL *my_conn, int conn, MYSQL_RES *result, int cols) {
	cur_data *cur = (cur_data *)lua_newuserdata(L, sizeof(cur_data));
	luasql_setmeta (L, LUASQL_CURSOR_MYSQL);

	/* fill in structure */
	cur->closed = 0;
	cur->conn = LUA_NOREF;
	cur->numcols = cols;
	cur->colnames = LUA_NOREF;
	cur->coltypes = LUA_NOREF;
	cur->my_res = result;
	cur->my_conn = my_conn;
	lua_pushvalue (L, conn);
	cur->conn = luaL_ref (L, LUA_REGISTRYINDEX);

	return 1;
}

static int conn_gc (lua_State *L) {
	conn_data *conn=(conn_data *)luaL_checkudata(L, 1, LUASQL_CONNECTION_MYSQL);
	if (conn != NULL && !(conn->closed)) {
		/* Nullify structure fields. */
		conn->closed = 1;
		luaL_unref (L, LUA_REGISTRYINDEX, conn->env);
		mysql_close (conn->my_conn);
	}
	return 0;
}


/*
** Close a Connection object.
*/
static int conn_close (lua_State *L) {
	conn_data *conn=(conn_data *)luaL_checkudata(L, 1, LUASQL_CONNECTION_MYSQL);
	luaL_argcheck (L, conn != NULL, 1, LUASQL_PREFIX"connection expected");
	if (conn->closed) {
		lua_pushboolean (L, 0);
		return 1;
	}
	conn_gc (L);
	lua_pushboolean (L, 1);
	return 1;
}

/*
** Ping connection.
*/
static int conn_ping (lua_State *L) {
	conn_data *conn=(conn_data *)luaL_checkudata(L, 1, LUASQL_CONNECTION_MYSQL);
	luaL_argcheck (L, conn != NULL, 1, LUASQL_PREFIX"connection expected");
	if (conn->closed) {
		lua_pushboolean (L, 0);
		return 1;
	}
	if (mysql_ping (conn->my_conn) == 0) {
		lua_pushboolean (L, 1);
		return 1;
	} else if (mysql_errno (conn->my_conn) == CR_SERVER_GONE_ERROR) {
		lua_pushboolean (L, 0);
		return 1;
	}
	luaL_error(L, mysql_error(conn->my_conn));
	return 0;
}


static int escape_string (lua_State *L) {
  size_t size, new_size;
  conn_data *conn = getconnection (L);
  const char *from = luaL_checklstring(L, 2, &size);
  char *to;
  to = (char*)malloc(sizeof(char) * (2 * size + 1));
  if(to) {
    new_size = mysql_real_escape_string(conn->my_conn, to, from, (unsigned long)size);
    lua_pushlstring(L, to, new_size);
    free(to);
    return 1;
  }
  luaL_error(L, "could not allocate escaped string");
  return 0;
}

static int getallresult(lua_State* L, MYSQL* conn, MYSQL_RES* res, unsigned int cols)
{
	for (int j = 0; ; j++)
	{
		MYSQL_FIELD* fields = mysql_fetch_fields(res);
		unsigned long* lengths;

		lua_newtable(L);
		for (int k = 0; ; k++)
		{
			MYSQL_ROW row = mysql_fetch_row(res);
			if (row == NULL) {
				break;
			}
			lengths = mysql_fetch_lengths(res);

			lua_newtable(L);
			/* Copy values to alphanumerical indices */
			for (unsigned int i = 0; i < cols; i++) {
				lua_pushlstring(L, fields[i].name, fields[i].name_length);
				/* Actually push the value */
				pushvalue(L, row[i], lengths[i]);
				lua_rawset(L, -3);
			}
			lua_rawseti(L, -2, k + 1);
		}
		lua_rawseti(L, 3, j + 1);

		mysql_free_result(res);
		if (mysql_next_result(conn) == 0)
		{
			res = mysql_store_result(conn);
			cols = mysql_field_count(conn);
			if (res)
				continue;
		}
		break;
	}

	lua_pushvalue(L, 3);
	return 1;
}

/*
** Execute an SQL statement.
** Return a Cursor object if the statement is a query, otherwise
** return the number of tuples affected by the statement.
*/
static int conn_execute (lua_State *L) {
	conn_data *conn = getconnection (L);
	size_t st_len;
	const char *statement = luaL_checklstring (L, 2, &st_len);
	if (mysql_real_query(conn->my_conn, statement, (unsigned long)st_len)) 
		/* error executing query */
		return luasql_failmsg(L, "error executing query. MySQL: ", mysql_error(conn->my_conn));
	else
	{
		MYSQL_RES *res = mysql_store_result(conn->my_conn);
		unsigned int num_cols = mysql_field_count(conn->my_conn);

		if (res) { /* tuples returned */
			if (lua_istable(L, 3))
			{
				return getallresult(L, conn->my_conn, res, num_cols);
			}

			return create_cursor (L, conn->my_conn, 1, res, num_cols);
		}
		else { /* mysql_use_result() returned nothing; should it have? */
			if(num_cols == 0) { /* no tuples returned */
            	/* query does not return data (it was not a SELECT) */
				lua_pushinteger(L, mysql_affected_rows(conn->my_conn));
				return 1;
        	}
			else /* mysql_use_result() should have returned data */
				return luasql_failmsg(L, "error retrieving result. MySQL: ", mysql_error(conn->my_conn));
		}
	}
}


/*
** 创建一个新得stmt对象放在栈顶上.
*/
static int create_stmt(lua_State* L, MYSQL* my_conn, MYSQL_STMT* stmt, int conn) {
	stmt_data* _stmt = (stmt_data*)lua_newuserdata(L, sizeof(stmt_data));
	luasql_setmeta(L, LUASQL_STMT_MYSQL);

	/* fill in structure */
	_stmt->closed = 0;
	_stmt->conn = LUA_NOREF;
	_stmt->my_conn = my_conn;
	_stmt->my_stmt = stmt;
	lua_pushvalue(L, conn);
	_stmt->conn = luaL_ref(L, LUA_REGISTRYINDEX);
	_stmt->need_bind_count = mysql_stmt_param_count(stmt);
	_stmt->used_bind_index = 0;
	if (_stmt->need_bind_count)
	{
		_stmt->my_bind = (MYSQL_BIND*)calloc(_stmt->need_bind_count, sizeof(MYSQL_BIND));
		_stmt->bind_param = (stmt_param_data*)calloc(_stmt->need_bind_count, sizeof(stmt_param_data));
	}
	else
	{
		_stmt->my_bind = NULL;
		_stmt->bind_param = NULL;
	}
	return 1;
}

/*
** Closes the cursos and nullify all structure fields.
*/
static void stmt_nullify(lua_State* L, stmt_data* stmt) {
	/* Nullify structure fields. */
	stmt->closed = 1;

	if (stmt->my_bind)
	{
		free(stmt->my_bind);
		stmt->my_bind = NULL;
	}

	if (stmt->bind_param)
	{
		FreeParamData(stmt->bind_param, stmt->need_bind_count);
		stmt->bind_param = NULL;
	}

	mysql_stmt_close(stmt->my_stmt);
	stmt->my_stmt = NULL;
	luaL_unref(L, LUA_REGISTRYINDEX, stmt->conn);
}

/*
** Cursor object collector function
*/
static int stmt_gc(lua_State* L) {
	stmt_data* stmt = (stmt_data*)luaL_checkudata(L, 1, LUASQL_STMT_MYSQL);
	if (stmt != NULL && !(stmt->closed))
		stmt_nullify(L, stmt);
	return 0;
}


/*
** Close the cursor on top of the stack.
** Return 1
*/
static int stmt_close(lua_State* L) {
	stmt_data* stmt = (stmt_data*)luaL_checkudata(L, 1, LUASQL_STMT_MYSQL);
	luaL_argcheck(L, stmt != NULL, 1, LUASQL_PREFIX"stmt expected");
	if (stmt->closed) {
		lua_pushboolean(L, 0);
		return 1;
	}
	stmt_nullify(L, stmt);
	lua_pushboolean(L, 1);
	return 1;
}

/*
** Close the cursor on top of the stack.
** Return 1
*/
static int stmt_bind(lua_State* L) {
	stmt_data* stmt = (stmt_data*)luaL_checkudata(L, 1, LUASQL_STMT_MYSQL);
	luaL_argcheck(L, stmt != NULL, 1, LUASQL_PREFIX"stmt expected");
	if (stmt->closed) {
		lua_pushboolean(L, 0);
		return 1;
	}

	int count = lua_gettop(L);
	for (int i = 2; i <= count; i++)
	{
	if(stmt->used_bind_index >= stmt->need_bind_count)
		return luasql_failmsg(L, "error stmt bind param more need. MySQL", "");

	MYSQL_BIND* my_bind = &stmt->my_bind[stmt->used_bind_index];
	stmt_param_data* my_param_data = &stmt->bind_param[stmt->used_bind_index];

		int luatype = lua_type(L, i);
	my_bind->is_null = &my_param_data->is_null;
	switch (luatype)
	{
	case LUA_TNIL:
		my_param_data->is_null = 1;
		my_bind->is_null = &my_param_data->is_null;
		my_bind->buffer_type = MYSQL_TYPE_NULL;
		break;
	case LUA_TBOOLEAN:
		my_bind->buffer_type = MYSQL_TYPE_TINY;
		my_bind->buffer_length = sizeof(my_param_data->willnumber.l);
			my_param_data->willnumber.l = lua_tointeger(L, i);
		my_bind->buffer = &my_param_data->willnumber.l;
		break;
	case LUA_TNUMBER:
	{
			lua_Integer x = lua_tointeger(L, i);
			lua_Number n = lua_tonumber(L, i);

			if (n == x)
		{
			my_bind->buffer_type = MYSQL_TYPE_LONG;
				my_param_data->willnumber.l = lua_tointeger(L, i);
			my_bind->buffer = &my_param_data->willnumber.l;
		}
		else
		{
			my_bind->buffer_type = MYSQL_TYPE_DOUBLE;
				my_param_data->willnumber.d = lua_tonumber(L, i);
			my_bind->buffer = &my_param_data->willnumber.l;
		}
		break;
	}
	case LUA_TSTRING:
	{
		my_bind->buffer_type = MYSQL_TYPE_STRING;
		size_t sizelen = 0;
			const char* buffer = lua_tolstring(L, i, &sizelen);

		my_param_data->willbuffer = malloc(sizelen);
		memcpy(my_param_data->willbuffer, buffer, sizelen);
		my_bind->buffer = my_param_data->willbuffer;
		my_bind->buffer_length = (unsigned long)sizelen;
		break;
	}
	default:
		return luasql_failmsg(L, "error stmt bind. unkonw type", "");
	}
	stmt->used_bind_index++;
	}
	
	lua_pushboolean(L, 1);
	return 1;
}


#ifdef WIN32


inline struct tm* gmtime_r(
	time_t const* const _Time,
	struct tm* const _Tm
)
{
	if (!_Time || !_Tm) return NULL;
	if (gmtime_s(_Tm, _Time) == 0)
		return _Tm;

	return NULL;
}
#endif
/*
** Close the cursor on top of the stack.
** Return 1
*/
static int stmt_bind_time(lua_State* L) {
	stmt_data* stmt = (stmt_data*)luaL_checkudata(L, 1, LUASQL_STMT_MYSQL);
	luaL_argcheck(L, stmt != NULL, 1, LUASQL_PREFIX"stmt expected");
	if (stmt->closed) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if(stmt->used_bind_index >= stmt->need_bind_count)
		return luasql_failmsg(L, "error stmt bind param more need. MySQL", "");

	MYSQL_BIND* my_bind = &stmt->my_bind[stmt->used_bind_index];
	stmt_param_data* my_param_data = &stmt->bind_param[stmt->used_bind_index];

	time_t time = luaL_checkinteger(L, 2);

	MYSQL_TIME* mysql_time = malloc(sizeof(MYSQL_TIME));
	my_param_data->willbuffer = mysql_time;
	my_bind->buffer = my_param_data->willbuffer;
	my_bind->buffer_length = sizeof(MYSQL_TIME);
	my_bind->buffer_type = MYSQL_TYPE_DATETIME;
	
	struct tm _tm;
	gmtime_r(&time, &_tm);

	mysql_time->year = _tm.tm_year + 1900;
	mysql_time->month = _tm.tm_mon + 1;
	mysql_time->day = _tm.tm_mday;
	mysql_time->hour = _tm.tm_hour;
	mysql_time->minute = _tm.tm_min;
	mysql_time->second = _tm.tm_sec;
	mysql_time->second_part = 0;
	mysql_time->neg = 0;
	mysql_time->time_type = MYSQL_TIMESTAMP_DATETIME;

	stmt->used_bind_index++;
	lua_pushboolean(L, 1);
	return 1;
}

/*
** Create a new Cursor object and push it on top of the stack.
*/
static int create_stmt_cursor(lua_State* L, MYSQL* my_conn, MYSQL_STMT* my_stmt, int stmt, MYSQL_RES* result, int cols) {
	stmt_cur_data* stmt_cur = (stmt_cur_data*)lua_newuserdata(L, sizeof(stmt_cur_data));
	luasql_setmeta(L, LUASQL_STMT_CURSOR_MYSQL);

	/* fill in structure */
	stmt_cur->closed = 0;
	stmt_cur->stmt = LUA_NOREF;
	stmt_cur->numcols = cols;
	stmt_cur->colnames = LUA_NOREF;
	stmt_cur->coltypes = LUA_NOREF;
	stmt_cur->my_res = result;
	stmt_cur->my_conn = my_conn;
	stmt_cur->my_stmt = my_stmt;
	lua_pushvalue(L, stmt);
	stmt_cur->stmt = luaL_ref(L, LUA_REGISTRYINDEX);

	if (cols)
	{
		stmt_cur->my_bindwill = (MYSQL_BIND*)calloc(cols, sizeof(MYSQL_BIND));
		stmt_cur->my_paramwill = (stmt_param_data*)calloc(cols, sizeof(stmt_param_data));
		for (int i = 0; i < cols; i++)
		{
			//enum_field_types eft = result->fields[i].type;
			//unsigned int flags = result->fields[i].flags;
			MYSQL_BIND* bind = &stmt_cur->my_bindwill[i];
			stmt_param_data* param = &stmt_cur->my_paramwill[i];
			
			int malloclen = 512;
			

			param->willbuffer = malloc(malloclen);
			bind->buffer_type = MYSQL_TYPE_STRING;
			bind->buffer = param->willbuffer;
			bind->length = &param->length;
			bind->is_null = &param->is_null;
			bind->buffer_length = malloclen;
		}

		if (mysql_stmt_bind_result(stmt_cur->my_stmt, stmt_cur->my_bindwill) != 0)
		{
			return luasql_failmsg(L, "error cur result. MySQL: ", mysql_stmt_error(stmt_cur->my_stmt));
		}
	}
	else
	{
		stmt_cur->my_bindwill = NULL;
		stmt_cur->my_paramwill = NULL;
	}

	if (mysql_stmt_store_result(stmt_cur->my_stmt) != 0)
	{
		return luasql_failmsg(L, "error cur result. MySQL: ", mysql_stmt_error(stmt_cur->my_stmt));
	}

	return 1;
}

/*
** Push the value of #i field of #tuple row.
*/
static void stmt_pushvalue1(lua_State* L, MYSQL_STMT* stmt, stmt_param_data* param_data, MYSQL_BIND* bind, unsigned int column) {
	if (param_data->is_null)
	{
		lua_pushnil(L);
	}
	else
	{
		switch (bind->buffer_type)
		{
		case MYSQL_TYPE_LONGLONG:
			lua_pushinteger(L, param_data->willnumber.l);
			break;
		case MYSQL_TYPE_DOUBLE:
			lua_pushnumber(L, param_data->willnumber.d);
			break;
		default:
		{
			unsigned long offset = 0;
			int pushnum = 0;
			while (offset < param_data->length)
			{
				if (offset != 0)
				{
					int ret = mysql_stmt_fetch_column(stmt, bind, column, offset);
					if (ret != 0)
					{
						luaL_error(L, "");
					}
				}

				int readlen = param_data->length - offset;
				if (readlen > (int)bind->buffer_length)
					readlen = (int)bind->buffer_length;

				lua_pushlstring(L, param_data->willbuffer, readlen);

				offset += readlen;
				pushnum++;
			}

			if (pushnum > 1)
				lua_concat(L, pushnum);
			else if (pushnum == 0)
				lua_pushstring(L, ""); //长度是0得字符串
		}
			break;
		}
	}
}


static int stmt_getallresult(lua_State* L, stmt_data* stmt, MYSQL_RES* result, unsigned int cols)
{
	MYSQL_BIND* my_bindwill = NULL;
	stmt_param_data* my_paramwill = NULL;
	unsigned int ori_cols = 0;

	for(int k = 0; ; k++)
	{
		if (my_bindwill == NULL && my_paramwill == NULL)
		{

			my_bindwill = (MYSQL_BIND*)calloc(cols, sizeof(MYSQL_BIND));
			my_paramwill = (stmt_param_data*)calloc(cols, sizeof(stmt_param_data));
			ori_cols = cols;
			for (unsigned int i = 0; i < cols; i++)
			{
				MYSQL_BIND* bind = &my_bindwill[i];
				stmt_param_data* param = &my_paramwill[i];

				enum_field_types eft = result->fields[i].type;
				int malloclen = 4096;
				switch (eft)
				{
				case MYSQL_TYPE_DECIMAL:
				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
				case MYSQL_TYPE_NEWDECIMAL:
					bind->buffer_type = MYSQL_TYPE_DOUBLE;
					bind->buffer = &param->willnumber.d;
					bind->length = &param->length;
					bind->is_null = &param->is_null;
				case MYSQL_TYPE_BIT:
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_LONG:
				case MYSQL_TYPE_LONGLONG:
				case MYSQL_TYPE_INT24:
				case MYSQL_TYPE_TIMESTAMP:
				case MYSQL_TYPE_TIMESTAMP2:
				case MYSQL_TYPE_ENUM:
					bind->buffer_type = MYSQL_TYPE_LONGLONG;
					bind->buffer = &param->willnumber.l;
					bind->is_null = &param->is_null;
					bind->length = &param->length;
					break;
				default:
					param->willbuffer = malloc(malloclen);
					bind->buffer_type = MYSQL_TYPE_STRING;
					bind->buffer = param->willbuffer;
					bind->length = &param->length;
					bind->is_null = &param->is_null;
					bind->buffer_length = malloclen;
					break;
				}
			}
		}

		if (mysql_stmt_bind_result(stmt->my_stmt, my_bindwill) != 0)
		{
			FreeParamData(my_paramwill, ori_cols);
			free(my_bindwill);
			return luasql_failmsg(L, "error cur result. MySQL: ", mysql_stmt_error(stmt->my_stmt));
		}

		if (mysql_stmt_store_result(stmt->my_stmt) != 0)
		{
			return luasql_failmsg(L, "error cur result. MySQL: ", mysql_stmt_error(stmt->my_stmt));
		}
		lua_newtable(L);
		for(int j = 0 ; ; j++)
		{
			int r = mysql_stmt_fetch(stmt->my_stmt);
			if (r != 0 && r != MYSQL_DATA_TRUNCATED)
			{
				break;
			}

			lua_newtable(L);
			for (unsigned int i = 0; i < cols; i++) {
				lua_pushlstring(L, result->fields[i].name, result->fields[i].name_length);
				stmt_pushvalue1(L, stmt->my_stmt, &my_paramwill[i], &my_bindwill[i], i);
				lua_rawset(L, -3);
			}

			lua_rawseti(L, -2, j + 1);
		}
		lua_rawseti(L, 2, k + 1);

		
		mysql_stmt_free_result(stmt->my_stmt);
		mysql_free_result(result);
		int status = mysql_stmt_next_result(stmt->my_stmt);
		if (status == 0) {
			unsigned int cur_count = mysql_stmt_field_count(stmt->my_stmt);
			if (cur_count > ori_cols)
			{
				FreeParamData(my_paramwill, ori_cols);

				free(my_bindwill);
				
				my_bindwill = NULL;
				my_paramwill = NULL;

			}
			
			cols = cur_count;

			result = mysql_stmt_result_metadata(stmt->my_stmt);

			if(result)
				continue;
		}
		break;
	};
	if(my_paramwill)
		FreeParamData(my_paramwill, ori_cols);
	if(my_bindwill)
		free(my_bindwill);
	
	lua_pushvalue(L, 2);
	return 1;

}

static int stmt_execute(lua_State* L) {
	stmt_data* stmt = (stmt_data*)luaL_checkudata(L, 1, LUASQL_STMT_MYSQL);
	luaL_argcheck(L, stmt != NULL, 1, LUASQL_PREFIX"stmt expected");
	if (stmt->closed) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if (stmt->need_bind_count != stmt->used_bind_index)
	{
		luaL_error(L, "error stmt bind param not enought, need(%d), bind(%d)", stmt->need_bind_count, stmt->used_bind_index);
	}


	if (stmt->need_bind_count && mysql_stmt_bind_param(stmt->my_stmt, stmt->my_bind) != 0)
	{
		return luasql_failmsg(L, "error stmt bind. MySQL: ", mysql_stmt_error(stmt->my_stmt));
	}

	if (mysql_stmt_execute(stmt->my_stmt) != 0)
	{
		return luasql_failmsg(L, "error stmt bind. MySQL: ", mysql_stmt_error(stmt->my_stmt));
	}

	MYSQL_RES* res = mysql_stmt_result_metadata(stmt->my_stmt);
	unsigned int num_cols = mysql_stmt_field_count(stmt->my_stmt);

	if (res) { /* tuples returned */
		if (lua_istable(L, 2))
		{
			return stmt_getallresult(L, stmt, res, num_cols);
		}
		return create_stmt_cursor(L, stmt->my_conn, stmt->my_stmt, 1, res, num_cols);
	}
	else { /* mysql_use_result() returned nothing; should it have? */
		if (num_cols == 0) { /* no tuples returned */
			/* query does not return data (it was not a SELECT) */
			lua_pushinteger(L, mysql_stmt_affected_rows(stmt->my_stmt));
			return 1;
		}
		else /* mysql_use_result() should have returned data */
			return luasql_failmsg(L, "error retrieving result. MySQL: ", mysql_stmt_error(stmt->my_stmt));
	}
}

static int stmt_reset(lua_State* L) {
	stmt_data* stmt = (stmt_data*)luaL_checkudata(L, 1, LUASQL_STMT_MYSQL);
	luaL_argcheck(L, stmt != NULL, 1, LUASQL_PREFIX"stmt expected");

	stmt->used_bind_index = 0;
	mysql_stmt_reset(stmt->my_stmt);

	lua_pushboolean(L, 1);
	return 1;
}

/*
** Closes the stmt cursos and nullify all structure fields.
*/
static void stmt_cur_nullify(lua_State* L, stmt_cur_data* stmt_cur) {
	/* Nullify structure fields. */
	stmt_cur->closed = 1;
	mysql_free_result(stmt_cur->my_res);
	while (mysql_stmt_next_result(stmt_cur->my_stmt) == 0)
	{
		mysql_free_result(mysql_stmt_result_metadata(stmt_cur->my_stmt));
	}

	if (stmt_cur->my_bindwill)
	{
		free(stmt_cur->my_bindwill);
		stmt_cur->my_bindwill = NULL;
	}
	
	if (stmt_cur->my_paramwill)
	{
		FreeParamData(stmt_cur->my_paramwill, stmt_cur->numcols);
		stmt_cur->my_paramwill = NULL;
	}


	luaL_unref(L, LUA_REGISTRYINDEX, stmt_cur->stmt);
	luaL_unref(L, LUA_REGISTRYINDEX, stmt_cur->colnames);
	luaL_unref(L, LUA_REGISTRYINDEX, stmt_cur->coltypes);
}

/*
** Cursor object collector function
*/
static int stmt_cur_gc(lua_State* L) {
	stmt_cur_data* stmt_cur = (stmt_cur_data*)luaL_checkudata(L, 1, LUASQL_STMT_CURSOR_MYSQL);
	if (stmt_cur != NULL && !(stmt_cur->closed))
		stmt_cur_nullify(L, stmt_cur);
	return 0;
}

/*
** Close the cursor on top of the stack.
** Return 1
*/
static int stmt_cur_close(lua_State* L) {
	stmt_cur_data* stmt_cur = (stmt_cur_data*)luaL_checkudata(L, 1, LUASQL_STMT_CURSOR_MYSQL);
	luaL_argcheck(L, stmt_cur != NULL, 1, LUASQL_PREFIX"stmt cursor expected");
	if (stmt_cur->closed) {
		lua_pushboolean(L, 0);
		return 1;
	}
	stmt_cur_nullify(L, stmt_cur);
	lua_pushboolean(L, 1);
	return 1;
}

/*
** Return the list of field names.
*/
static int stmt_cur_getcolnames(lua_State* L) {
	//这边复用是基于 stmt_cur_data和 cur_data前半部分结构一致
	pushtable(L, (cur_data*)getstmtcursor(L), colnames);
	return 1;
}


/*
** Return the list of field types.
*/
static int stmt_cur_getcoltypes(lua_State* L) {
	//这边复用是基于 stmt_cur_data和 cur_data前半部分结构一致
	pushtable(L, (cur_data*)getstmtcursor(L), coltypes);
	return 1;
}

/*
** Push the value of #i field of #tuple row.
*/
static void stmt_pushvalue(lua_State* L, stmt_cur_data* stmt_cur, unsigned int column) {
	stmt_param_data* param_data = &stmt_cur->my_paramwill[column];
	MYSQL_BIND* bind = &stmt_cur->my_bindwill[column];
	if (param_data->is_null)
	{
		lua_pushnil(L);
	}
	else
	{
		//param_data->length 字段总长度
		unsigned long offset = 0;
		int pushnum = 0;
		while (offset < param_data->length)
		{
			if (offset != 0)
			{
				int ret = mysql_stmt_fetch_column(stmt_cur->my_stmt, bind, column, offset);
				if (ret != 0)
				{
					luaL_error(L, "");
				}
			}

			int readlen = param_data->length - offset;
			if (readlen > (int)bind->buffer_length)
				readlen = (int)bind->buffer_length;

			lua_pushlstring(L, param_data->willbuffer, readlen);

			offset += readlen;
			pushnum++;
		}

		if (pushnum > 1)
			lua_concat(L, pushnum);
		else if (pushnum == 0)
			lua_pushstring(L, ""); //长度是0得字符串
	}
}
/*
** Get another row of the given stmt cursor.
*/
static int stmt_cur_fetch(lua_State* L) {
	stmt_cur_data* stmt_cur = getstmtcursor(L);
	//MYSQL_RES* res = stmt_cur->my_res;

	int r = mysql_stmt_fetch(stmt_cur->my_stmt);
	if (r != 0 && r != MYSQL_DATA_TRUNCATED)
	{
		//const char* error = mysql_stmt_error(stmt_cur->my_stmt);
		stmt_cur_nullify(L, stmt_cur);
		lua_pushnil(L);  /* no more results */
		return 1;
	}

	if (lua_istable(L, 2)) {
		const char* opts = luaL_optstring(L, 3, "n");
		if (strchr(opts, 'n') != NULL) {
			/* Copy values to numerical indices */
			int i;
			for(i = 0; i < stmt_cur->numcols; i++){
				stmt_pushvalue(L, stmt_cur, i);
				lua_rawseti(L, 2, i + 1);
			}
		}
		if (strchr(opts, 'a') != NULL) {
			int i;
			/* Check if colnames exists */
			/* 前半部分结构一致所以复用create_colinfo函数 */
			if (stmt_cur->colnames == LUA_NOREF)
				create_colinfo(L, (cur_data*)stmt_cur);
			lua_rawgeti(L, LUA_REGISTRYINDEX, stmt_cur->colnames);/* Push colnames*/

			/* Copy values to alphanumerical indices */
			for (i = 0; i < stmt_cur->numcols; i++) {
				lua_rawgeti(L, -1, i + 1); /* push the field name */

				/* Actually push the value */
				stmt_pushvalue(L, stmt_cur, i);
				lua_rawset(L, 2);
			}
			/* lua_pop(L, 1);  Pops colnames table. Not needed */
		}
		lua_pushvalue(L, 2);
		return 1; /* return table */
	}
	else {
		int i;
		luaL_checkstack(L, stmt_cur->numcols, LUASQL_PREFIX"too many columns");
		for (i = 0; i < stmt_cur->numcols; i++)
			stmt_pushvalue(L, stmt_cur, i);
		return stmt_cur->numcols; /* return #numcols values */
	}

	return 0;
}

/*
** Push the number of rows.
*/
static int stmt_cur_numrows(lua_State* L) {
	lua_pushinteger(L, (lua_Integer)mysql_stmt_num_rows(getstmtcursor(L)->my_stmt));
	return 1;
}

/*
** Seeks to an arbitrary row in a query result set.
*/
static int stmt_cur_seek(lua_State* L) {
	stmt_cur_data* cur = getstmtcursor(L);
	lua_Integer rownum = luaL_checkinteger(L, 2);
	mysql_stmt_data_seek(cur->my_stmt, rownum);
	return 0;
}

static int fix_stmt_cursor(lua_State* L, stmt_cur_data* stmt_cur, MYSQL_RES* result, int cols) {
	/* fix in structure */
	luaL_unref(L, LUA_REGISTRYINDEX, stmt_cur->colnames);
	luaL_unref(L, LUA_REGISTRYINDEX, stmt_cur->coltypes);

	if (stmt_cur->my_bindwill)
	{
		free(stmt_cur->my_bindwill);
		stmt_cur->my_bindwill = NULL;
	}

	if (stmt_cur->my_paramwill)
	{
		FreeParamData(stmt_cur->my_paramwill, stmt_cur->numcols);
		stmt_cur->my_paramwill = NULL;
	}

	stmt_cur->numcols = cols;
	stmt_cur->colnames = LUA_NOREF;
	stmt_cur->coltypes = LUA_NOREF;
	stmt_cur->my_res = result;

	if (result)
	{
		if (cols)
		{
			stmt_cur->my_bindwill = (MYSQL_BIND*)calloc(cols, sizeof(MYSQL_BIND));
			stmt_cur->my_paramwill = (stmt_param_data*)calloc(cols, sizeof(stmt_param_data));
			for (int i = 0; i < cols; i++)
			{
				//enum_field_types eft = result->fields[i].type;
				//unsigned int flags = result->fields[i].flags;
				MYSQL_BIND* bind = &stmt_cur->my_bindwill[i];
				stmt_param_data* param = &stmt_cur->my_paramwill[i];

				int malloclen = 512;


				param->willbuffer = malloc(malloclen);
				bind->buffer_type = MYSQL_TYPE_STRING;
				bind->buffer = param->willbuffer;
				bind->length = &param->length;
				bind->is_null = &param->is_null;
				bind->buffer_length = malloclen;
			}

			if (mysql_stmt_bind_result(stmt_cur->my_stmt, stmt_cur->my_bindwill) != 0)
			{
				return luasql_failmsg(L, "error cur result. MySQL: ", mysql_stmt_error(stmt_cur->my_stmt));
			}
		}
		else
		{
			stmt_cur->my_bindwill = NULL;
			stmt_cur->my_paramwill = NULL;
		}

		if (mysql_stmt_store_result(stmt_cur->my_stmt) != 0)
		{
			return luasql_failmsg(L, "error cur result. MySQL: ", mysql_stmt_error(stmt_cur->my_stmt));
		}
	}
	
	return 0;
}

/*
** Get the next result from multiple statements
*/
static int stmt_cur_next_result(lua_State* L) {
	stmt_cur_data* stmt_cur = getstmtcursor(L);
	MYSQL_STMT* stmt = stmt_cur->my_stmt;
	int status;
	if (mysql_more_results(stmt_cur->my_conn)) {
		mysql_stmt_free_result(stmt);
		status = fix_stmt_cursor(L, stmt_cur, NULL, 0);
		if (status != 0)
		{
			return status;
		}
		status = mysql_stmt_next_result(stmt);
		if (status == 0) {
			fix_stmt_cursor(L, stmt_cur, mysql_stmt_result_metadata(stmt), mysql_stmt_field_count(stmt));
			if (stmt_cur->my_res != NULL) {
				lua_pushboolean(L, 1);
				return 1;
			}
			else {
				lua_pushboolean(L, 0);
				lua_pushstring(L, mysql_stmt_error(stmt));
				return 3;
			}
		}
		else {
			lua_pushboolean(L, 0);
			lua_pushinteger(L, status);
			switch (status) {
			case CR_COMMANDS_OUT_OF_SYNC:
				lua_pushliteral(L, "CR_COMMANDS_OUT_OF_SYNC");
				break;
			case CR_SERVER_GONE_ERROR:
				lua_pushliteral(L, "CR_SERVER_GONE_ERROR");
				break;
			case CR_SERVER_LOST:
				lua_pushliteral(L, "CR_SERVER_LOST");
				break;
			case CR_UNKNOWN_ERROR:
				lua_pushliteral(L, "CR_UNKNOWN_ERROR");
				break;
			default:
				lua_pushliteral(L, "Unknown");
			}
			return 3;
		}
	}
	else {
		lua_pushboolean(L, 0);
		lua_pushinteger(L, -1);
		return 2;
	}
}

/*
** Check if next result is available
*/
static int stmt_cur_has_next_result(lua_State* L) {
	stmt_cur_data* stmt_cur = getstmtcursor(L);
	lua_pushboolean(L, mysql_more_results(stmt_cur->my_conn));
	return 1;
}

/*
** Execute an SQL statement.
** Return a Cursor object if the statement is a query, otherwise
** return the number of tuples affected by the statement.
*/
static int conn_stmt_prepare(lua_State* L) {
	conn_data* conn = getconnection(L);
	MYSQL_STMT* stmt = mysql_stmt_init(conn->my_conn);
	if (!stmt)
	{
		return luasql_failmsg(L, "error stmt prepare. MySQL: ", mysql_error(conn->my_conn));
	}
	size_t st_len;
	const char* statement = luaL_checklstring(L, 2, &st_len);
	if (mysql_stmt_prepare(stmt, statement, (unsigned long)st_len) != 0)
	{
		char error_msg[512];
		strncpy(error_msg, mysql_stmt_error(stmt), 511);
		mysql_stmt_close(stmt);
		return luasql_failmsg(L, "error stmt prepare. MySQL: ", error_msg);
	}

	return create_stmt(L, conn->my_conn, stmt, 1);
}

/*
** Commit the current transaction.
*/
static int conn_commit (lua_State *L) {
	conn_data *conn = getconnection (L);
	lua_pushboolean(L, !mysql_commit(conn->my_conn));
	return 1;
}

/*
** Rollback the current transaction.
*/
static int conn_rollback (lua_State *L) {
	conn_data *conn = getconnection (L);
	lua_pushboolean(L, !mysql_rollback(conn->my_conn));
	return 1;
}


/*
** Set "auto commit" property of the connection. Modes ON/OFF
*/
static int conn_setautocommit (lua_State *L) {
	conn_data *conn = getconnection (L);
	if (lua_toboolean (L, 2)) {
		mysql_autocommit(conn->my_conn, 1); /* Set it ON */
	}
	else {
		mysql_autocommit(conn->my_conn, 0);
	}
	lua_pushboolean(L, 1);
	return 1;
}


/*
** Get Last auto-increment id generated
*/
static int conn_getlastautoid (lua_State *L) {
  conn_data *conn = getconnection(L);
  lua_pushinteger(L, mysql_insert_id(conn->my_conn));
  return 1;
}

/*
** Create a new Connection object and push it on top of the stack.
*/
static int create_connection (lua_State *L, int env, MYSQL *const my_conn) {
	conn_data *conn = (conn_data *)lua_newuserdata(L, sizeof(conn_data));
	luasql_setmeta (L, LUASQL_CONNECTION_MYSQL);

	/* fill in structure */
	conn->closed = 0;
	conn->env = LUA_NOREF;
	conn->my_conn = my_conn;
	lua_pushvalue (L, env);
	conn->env = luaL_ref (L, LUA_REGISTRYINDEX);
	return 1;
}


/*
** Connects to a data source.
**     param: one string for each connection parameter, said
**     datasource, username, password, host and port.
*/
static int env_connect (lua_State *L) {
	const char *sourcename = luaL_checkstring(L, 2);
	const char *username = luaL_optstring(L, 3, NULL);
	const char *password = luaL_optstring(L, 4, NULL);
	const char *host = luaL_optstring(L, 5, NULL);
	const int port = (int)luaL_optinteger(L, 6, 0);
	const char *unix_socket = luaL_optstring(L, 7, NULL);
	const long client_flag = (long)luaL_optinteger(L, 8, 0);
	MYSQL *conn;
	getenvironment(L); /* validade environment */

	/* Try to init the connection object. */
	conn = mysql_init(NULL);
	if (conn == NULL)
		return luasql_faildirect(L, "error connecting: Out of memory.");

	if (!mysql_real_connect(conn, host, username, password, 
		sourcename, port, unix_socket, client_flag))
	{
		char error_msg[100];
		strncpy (error_msg,  mysql_error(conn), 99);
		mysql_close (conn); /* Close conn if connect failed */
		return luasql_failmsg (L, "error connecting to database. MySQL: ", error_msg);
	}

	return create_connection(L, 1, conn);
}


/*
**
*/
static int env_gc (lua_State *L) {
	env_data *env= (env_data *)luaL_checkudata (L, 1, LUASQL_ENVIRONMENT_MYSQL);	if (env != NULL && !(env->closed))
		env->closed = 1;
	return 0;
}


/*
** Close environment object.
*/
static int env_close (lua_State *L) {
	env_data *env= (env_data *)luaL_checkudata (L, 1, LUASQL_ENVIRONMENT_MYSQL);
	luaL_argcheck (L, env != NULL, 1, LUASQL_PREFIX"environment expected");
	if (env->closed) {
		lua_pushboolean (L, 0);
		return 1;
	}
	mysql_library_end();
	env->closed = 1;
	lua_pushboolean (L, 1);
	return 1;
}


/*
** Create metatables for each class of object.
*/
static void create_metatables (lua_State *L) {
    struct luaL_Reg environment_methods[] = {
        {"__gc",    env_gc},
        {"__close", env_close},
        {"close",   env_close},
        {"connect", env_connect},
		{NULL, NULL},
	};
    struct luaL_Reg connection_methods[] = {
        {"__gc", conn_gc},
        {"__close", conn_close},
        {"close", conn_close},
        {"ping", conn_ping},
        {"escape", escape_string},
        {"execute", conn_execute},
        {"prepare", conn_stmt_prepare},
        {"commit", conn_commit},
        {"rollback", conn_rollback},
        {"setautocommit", conn_setautocommit},
		{"getlastautoid", conn_getlastautoid},
		{NULL, NULL},
    };
    struct luaL_Reg cursor_methods[] = {
        {"__gc", cur_gc},
        {"__close", cur_close},
        {"close", cur_close},
        {"getcolnames", cur_getcolnames},
        {"getcoltypes", cur_getcoltypes},
        {"fetch", cur_fetch},
        {"numrows", cur_numrows},
        {"seek", cur_seek},
		{"nextresult", cur_next_result},
		{"hasnextresult", cur_has_next_result},
		{NULL, NULL},
    };
	struct luaL_Reg stmt_methods[] = {
		{"__gc", stmt_gc},
		{"__close", stmt_close},
		{"close", stmt_close},
		{"bind", stmt_bind},
		{"bind_time", stmt_bind_time},
		{"execute", stmt_execute},
		{"reset", stmt_reset},
		{NULL, NULL},
	};
	struct luaL_Reg stmt_cur_methods[] = {
		{"__gc", stmt_cur_gc},
		{"__close", stmt_cur_close},
		{"close", stmt_cur_close},
		{"getcolnames", stmt_cur_getcolnames},
		{"getcoltypes", stmt_cur_getcoltypes},
		{"fetch", stmt_cur_fetch},
		{"numrows", stmt_cur_numrows},
		{"seek", stmt_cur_seek},
		{"nextresult", stmt_cur_next_result},
		{"hasnextresult", stmt_cur_has_next_result},
		{NULL, NULL},
	};
	luasql_createmeta (L, LUASQL_ENVIRONMENT_MYSQL, environment_methods);
	luasql_createmeta (L, LUASQL_CONNECTION_MYSQL, connection_methods);
	luasql_createmeta (L, LUASQL_CURSOR_MYSQL, cursor_methods);
	luasql_createmeta (L, LUASQL_STMT_MYSQL, stmt_methods);
	luasql_createmeta (L, LUASQL_STMT_CURSOR_MYSQL, stmt_cur_methods);
	lua_pop (L, 5);
}


/*
** Creates an Environment and returns it.
*/
static int create_environment (lua_State *L) {
	env_data *env = (env_data *)lua_newuserdata(L, sizeof(env_data));
	luasql_setmeta (L, LUASQL_ENVIRONMENT_MYSQL);

	/* fill in structure */
	env->closed = 0;
	return 1;
}


/*
** Creates the metatables for the objects and registers the
** driver open method.
*/
LUASQL_API int luaopen_luasql_mysql (lua_State *L) { 
	struct luaL_Reg driver[] = {
		{"mysql", create_environment},
		{NULL, NULL},
	};
	create_metatables (L);
	lua_newtable(L);
	luaL_setfuncs(L, driver, 0);
	luasql_set_info (L);
    lua_pushliteral (L, "_CLIENTVERSION");
#ifdef MARIADB_CLIENT_VERSION_STR
lua_pushliteral (L, MARIADB_CLIENT_VERSION_STR);
#else
lua_pushliteral (L, MYSQL_SERVER_VERSION);
#endif
    lua_settable (L, -3);
	return 1;
}
