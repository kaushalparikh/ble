
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

typedef union
{
  int32  integer;
  float  decimal;
  int8  *text;
  struct
  {
    uint32  length;
    uint8  *data;
  } blob;
} db_column_value_t;

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
  void                       *insert;
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

static void db_free_column_list (db_column_list_entry_t *column_list)
{
  while (column_list != NULL)
  {
    db_column_list_entry_t *column_list_entry = column_list;
    free (column_list_entry->title);
    list_remove ((list_entry_t **)(&column_list), (list_entry_t *)column_list_entry);
    free (column_list_entry);
  }
}

int32 db_fill_column (db_info_t *db_info, int8 *table_title, int8 *column_title,
                      db_column_value_t *column_value)
{
  int status;
  sqlite3_stmt *insert = NULL;
  db_column_list_entry_t *column_list = NULL;
  db_table_list_entry_t *table_list = db_info->table_list;
  
  while (table_list != NULL)
  {
    column_list = table_list->column_list;

    while (column_list != NULL)
    {
      if (((strcmp (table_list->title, table_title)) == 0) &&
          ((strcmp (column_list->title, column_title)) == 0))
      {
        insert = (sqlite3_stmt *)(table_list->insert);
        break;
      }

      column_list = column_list->next;
    }

    table_list = table_list->next;
  }

  if (insert != NULL)
  {
    int8 *column_tag = NULL;
    
    column_tag = strdup ("\":");
    STRING_CONCAT (column_tag, column_title);
    STRING_CONCAT (column_tag, "\"");

    printf ("Statement %s\n", (char *)insert);
    printf ("Column tag %s\n", column_tag);

    if (column_list->type == DB_COLUMN_TYPE_TEXT)
    {
      status = sqlite3_bind_text (insert,
                                  (sqlite3_bind_parameter_index (insert, column_tag)),
                                  column_value->text, -1, SQLITE_TRANSIENT);
    }
    else if (column_list->type == DB_COLUMN_TYPE_INT)
    {
      status = sqlite3_bind_int (insert,
                                 (sqlite3_bind_parameter_index (insert, column_tag)),
                                 column_value->integer);
    }
    else if (column_list->type == DB_COLUMN_TYPE_FLOAT)
    {
      status = sqlite3_bind_double (insert,
                                    (sqlite3_bind_parameter_index (insert, column_tag)),
                                    column_value->decimal);
    }
    else
    {
      status = sqlite3_bind_blob (insert,
                                  (sqlite3_bind_parameter_index (insert, column_tag)),
                                  column_value->blob.data, column_value->blob.length, SQLITE_TRANSIENT);
    }

    free (column_tag);

    if (status == SQLITE_OK)
    {
      sqlite3_step (insert);
      sqlite3_reset (insert);
      status = 1;
    }
    else
    {
      printf ("Can't fill table %s, column %s\n", table_title, column_title);
      status = -1;
    }
  }
  else
  {
    printf ("Can't find table %s, column %s\n", table_title, column_title);
    status = -1;
  }

  return status;
}

int32 db_create_table (db_info_t *db_info, char *title, uint32 index,
                       db_column_list_entry_t *column_list)
{
  int status;
  char *sql = NULL;
  db_table_list_entry_t *table_list_entry = (db_table_list_entry_t *)malloc (sizeof (*table_list_entry));

  table_list_entry->title       = strdup (title);
  table_list_entry->column_list = column_list;
  table_list_entry->index       = index;
  
  /* Prepare create statement */
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
        STRING_CONCAT (sql, " DEFAULT (DATETIME('NOW','LOCALTIME'))");
      }
      if (column_list->flags & DB_COLUMN_FLAG_DEFAULT_NA)
      {
        STRING_CONCAT (sql, " DEFAULT NA");
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
  status = sqlite3_exec ((sqlite3 *)(db_info->handle), sql, NULL, NULL, NULL);
  free (sql);
  sql = NULL;

  if (status == SQLITE_OK)
  {
    /* Prepare insert statement */
    sql = strdup ("INSERT INTO ");
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, title);
    STRING_CONCAT (sql, "] VALUES ( ");

    column_list = table_list_entry->column_list;
    while (column_list != NULL)
    {
      /* Column title */
      STRING_CONCAT (sql, ":\"");
      STRING_CONCAT (sql, column_list->title);
      STRING_CONCAT (sql, "\"");

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
    
    printf ("Prepare sql %s\n", sql);
    status = sqlite3_prepare_v2 ((sqlite3 *)(db_info->handle), sql, -1,
                                 (sqlite3_stmt **)(&(table_list_entry->insert)), NULL);
    free (sql);
    sql = NULL;

    if (status == SQLITE_OK)
    {
      list_add ((list_entry_t **)(&(db_info->table_list)), (list_entry_t *)table_list_entry);
      status = 1;
    }
    else
    {
      printf ("Can't prepare insert statement\n");
      status = -1;
    }
  }
  else
  {
    status = -1;
    printf ("Can't create table\n");
  }

  if (status < 0)
  {
    free (table_list_entry->title);
    free (table_list_entry);
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

      if ((db_create_table (db_info, "Group Title", table_index, column_list)) > 0)
      {
        db_column_value_t column_value;
        
        table_index++;
        
        column_value.decimal = 98.4;
        db_fill_column (db_info, "Group Title", "Temperature (F)", &column_value);
      }
      else
      {
        db_free_column_list (column_list);
      }

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

