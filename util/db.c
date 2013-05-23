
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "basic_types.h"
#include "list.h"

#define STRING_CONCAT(dest, src)  {                                                                 \
                                    dest = realloc (dest, ((strlen (dest)) + (strlen (src)) + 1));  \
                                    dest = strcat (dest, src);                                      \
                                  }

enum
{
  DB_COLUMN_TYPE_TEXT  = 1,
  DB_COLUMN_TYPE_INT   = 2,
  DB_COLUMN_TYPE_FLOAT = 3,
  DB_COLUMN_TYPE_BLOB  = 4 
};

enum
{
  DB_COLUMN_FLAG_PRIMARY_KEY       = 0x00000001,
  DB_COLUMN_FLAG_NOT_NULL          = 0x00000002,
  DB_COLUMN_FLAG_DEFAULT_TIMESTAMP = 0x00000004,
  DB_COLUMN_FLAG_DEFAULT_NA        = 0x00000008
};

struct db_column_list_entry
{
  struct db_column_list_entry *next;
  int8                        *title;
  uint32                       index;
  uint8                        type;
  uint32                       flags;
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
                       (int)strlen(name), NULL );

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return SQLITE_OK;
}



static void db_add_column (char *title, uint32 index, uint8 type, uint8 flags,
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
  int status = 1;
  db_table_list_entry_t *table_list_entry = (db_table_list_entry_t *)malloc (sizeof (*table_list_entry));

  if ((asprintf (&(table_list_entry->title), "%s", title)) > 0)
  {
    char *sql = NULL;
    char *error_message = NULL;

    table_list_entry->column_list = column_list;
    table_list_entry->index       = index;
  
    sql = strdup ("CREATE TABLE ");
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, title);
    STRING_CONCAT (sql, "] ( ");

    while (column_list != NULL)
    {
      /* Column title */
      STRING_CONCAT (sql, "[");
      STRING_CONCAT (sql, column_list->title);
      STRING_CONCAT (sql, "]");
 
      /* Space, then column data type */
      if (column_list->type != 0)
      {
        if (column_list->type == DB_COLUMN_TYPE_TEXT)
        {
          STRING_CONCAT (sql, " TEXT");
        }
        else if (column_list->type == DB_COLUMN_TYPE_INT)
        {
          STRING_CONCAT (sql, " INTEGER");
        }
        else if (column_list->type == DB_COLUMN_TYPE_FLOAT)
        {
          STRING_CONCAT (sql, " REAL");
        }
        else if (column_list->type == DB_COLUMN_TYPE_BLOB)
        {
          STRING_CONCAT (sql, " BLOB");
        }
      }

      /* Space, then column flags/constraints */
      if (column_list->flags != 0)
      {
        if (column_list->flags & DB_COLUMN_FLAG_PRIMARY_KEY)
        {
          STRING_CONCAT (sql, " PRIMARY KEY");
        }
        if (column_list->flags & DB_COLUMN_FLAG_NOT_NULL)
        {
          STRING_CONCAT (sql, " NOT NULL");
        }
        if (column_list->flags & DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)
        {
          STRING_CONCAT (sql, " DEFAULT CURRENT_TIMESTAMP");
        }
        if (column_list->flags & DB_COLUMN_FLAG_DEFAULT_NA)
        {
          STRING_CONCAT (sql, " DEFAULT 'NA'");
        }
      }

      /* Comma, if there is column to add, else closing bracket */
      if (column_list->next != NULL)
      {
        STRING_CONCAT (sql, ", ");
      }
      else
      {
        STRING_CONCAT (sql, " )");
      }

      column_list = column_list->next;
    }

    printf ("Execute sql %s\n", sql);
    status = sqlite3_exec ((sqlite3 *)(db_info->handle), sql, NULL, NULL, &error_message);
    if (status == SQLITE_OK)
    {
      list_add ((list_entry_t **)(&(db_info->table_list)), (list_entry_t *)table_list_entry);
      status = 1;
    }
    else
    {
      printf ("Error %s\n", error_message);
      sqlite3_free (error_message);
      free (table_list_entry);
      status = -1;
    }

    free (sql);
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

      db_add_column ("No.", column_index, DB_COLUMN_TYPE_INT,
                     DB_COLUMN_FLAG_PRIMARY_KEY, &column_list);
      column_index++;
      db_add_column ("Time", column_index, DB_COLUMN_TYPE_TEXT,
                     (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP), &column_list);
      column_index++;
      db_add_column ("Temperature (F)", column_index, DB_COLUMN_TYPE_FLOAT,
                     (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA), &column_list);
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

