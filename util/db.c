
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "basic_types.h"
#include "list.h"

enum
{
  DB_COLUMN_TYPE_TEXT  = 0,
  DB_COLUMN_TYPE_INT   = 1,
  DB_COLUMN_TYPE_INDEX = 2,
  DB_COLUMN_TYPE_FLOAT = 3,
  DB_COLUMN_TYPE_BLOB  = 4
};

struct db_column_list_entry
{
  struct db_column_list_entry *next;
  int8                        *title;
  uint32                       index;
  uint8                        type;
  uint8                        flags;
};

typedef struct db_column_list_entry db_column_list_entry_t;

struct db_table_list_entry
{
  struct db_table_list_entry *next;
  db_column_list_entry_t     *column_list;
  int8                       *title;
  uint32                      index;
};

typedef struct db_table_list_entry db_table_list_entry_t;

typedef struct
{
  int32                  handle;
  db_table_list_entry_t *table_list;
} db_info_t;


void cleanup_fn(void* data)
{
    fprintf(stderr, "Cleanup function called for: %p\n", data);
}

int test_positional_params(sqlite3 *db)
{
    sqlite3_stmt *stmt;
    const char* tail;

    const char* sql = "insert into episodes (id, cid, name) "
                      " values (?,?,'Mackinaw Peaches')";
    int rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, &tail);

    if(rc != SQLITE_OK) {
        fprintf(stderr, "Error: %s\n", tail);
    }

    sqlite3_bind_int(stmt, 1, 10);
    sqlite3_bind_int(stmt, 2, 10);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return SQLITE_OK;
}

int test_numbered_params(sqlite3 *db)
{
    sqlite3_stmt *stmt;
    const char* tail;
    char *name, *sql;
    int rc;

    name = "Mackinaw Peaches";
    sql = "insert into episodes (id, cid, name) "
          "values (?100,?100,?101)";

    rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, &tail);

    if(rc != SQLITE_OK) {
        fprintf(stderr, "sqlite3_prepare() : Error: %s\n", tail);
        return rc;
    }

    sqlite3_bind_int(stmt, 100, 10);
    sqlite3_bind_text(stmt, 101, name, (int)strlen(name), SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return SQLITE_OK;
}

int test_named_params(sqlite3 *db)
{
    sqlite3_stmt *stmt;
    char *name, *sql;
    int rc;
    const char* tail;

    name = "Mackinaw Peaches";
    sql = "insert into episodes (id, cid, name) values (:cosmo,:cosmo,:newman)";

    rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, &tail);

    if(rc != SQLITE_OK) {
        fprintf(stderr, "Error: %s\n", tail);
        return rc;
    }

    sqlite3_bind_int( stmt, 
                      sqlite3_bind_parameter_index(stmt, ":cosmo"), 10);
    
    sqlite3_bind_text( stmt, 
                       sqlite3_bind_parameter_index(stmt, ":newman"), 
                       name, 
                       (int)strlen(name), cleanup_fn );

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return SQLITE_OK;
}

static void db_build_column_list (char *title, uint32 index, uint8 type, uint8 flags,
                                  db_column_list_entry_t **column_list)
{
  db_column_list_entry_t *column_list_entry = (db_column_list_entry_t *)malloc (sizeof (*column_list_entry));

  if ((asprintf (&(column_list_entry->title), "%s", title)) > 0)
  {
    column_list_entry->index = index;
    column_list_entry->type  = type;
    column_list_entry->flags = flags;
    list_add ((list_entry_t **)column_list, (list_entry_t *)column_list_entry);
  }
  else
  {
    free (column_list_entry);
    printf ("Can't allocate column title %s\n", title);
  }
}

int32 db_create_table (db_info_t *db_info, char *title, uint32 index,
                       db_column_list_entry_t *column_list)
{
  int status = SQLITE_OK;
  db_table_list_entry_t *table_list_entry = (db_table_list_entry_t *)malloc (sizeof (*table_list_entry));

  if ((asprintf (&(table_list_entry->title), "%s", title)) > 0)
  {
    table_list_entry->column_list = column_list;
    table_list_entry->index       = index;

    if (status == SQLITE_OK)
    {
      list_add ((list_entry_t **)(&(db_info->table_list)), (list_entry_t *)table_list_entry);
      status = 1;
    }
    else
    {
      free (table_list_entry);
      status = -1;
    }
  }
  else
  {
    free (table_list_entry);
    printf ("Can't allocate table title %s\n", title);
  }

  return status;
}

int32 db_open (int8 *filename, db_info_t **db_info)
{
  int status;
  sqlite3 *db;

  status = sqlite3_open_v2 (filename, &db,
                            (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE), NULL);
  if (status == SQLITE_OK)
  {
    *db_info = (db_info_t *)malloc (sizeof (**db_info));
    (*db_info)->handle = (int32)db;
    status = 1;
  }
  else
  {
    printf ("Can't open %s\n", sqlite3_errmsg (db));
    sqlite3_close (db);
    status = -1;
  }
  
  return status;
}

int32 db_close (db_info_t *db_info)
{
  sqlite3 *db = (sqlite3 *)(db_info->handle);
  free (db_info);
  return sqlite3_close (db);
}

#ifdef UTIL_DB_TEST

int main (int argc, char *argv[])
{
  db_info_t *db_info = NULL;

  if (argc > 1)
  {
    if ((db_open (argv[1], &db_info)) > 0)
    {
      uint32 column_index = 1;
      uint32 table_index  = 1;
      db_column_list_entry_t *column_list = NULL;

      db_build_column_list ("No.", column_index, DB_COLUMN_TYPE_INDEX, 0, &column_list);
      column_index++;
      db_build_column_list ("Time", column_index, DB_COLUMN_TYPE_TEXT, 0, &column_list);
      column_index++;
      db_build_column_list ("Temperature (F)", column_index, DB_COLUMN_TYPE_FLOAT, 0, &column_list);
      column_index++;

      db_create_table (db_info, "Group Title", table_index, column_list);
      table_index++;

      db_close (db_info);
    }
  }
  else
  {
    printf ("Usage: db <filename>\n");
  }

  return 0;    
}

#endif

