
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "types.h"
#include "list.h"
#include "util.h"

#define STRING_CONCAT(dest, src)                                    \
  {                                                                 \
    dest = realloc (dest, ((strlen (dest)) + (strlen (src)) + 1));  \
    dest = strcat (dest, src);                                      \
  }

#define STRING_REPLACE_CHAR(dest, search, replace)  \
  {                                                 \
    int32 index = 0;                                \
    while (dest[index] != '\0')                     \
    {                                               \
      if (dest[index] == search)                    \
      {                                             \
        dest[index] = replace;                      \
      }                                             \
      index++;                                      \
    }                                               \
  }


static void db_add_column (char *title, uint32 index, uint8 type, uint8 flags,
                           db_column_list_entry_t **column_list)
{
  db_column_list_entry_t *column_list_entry = (db_column_list_entry_t *)malloc (sizeof (*column_list_entry));

  column_list_entry->title = strdup (title);
  column_list_entry->index = index;
  column_list_entry->type  = type;
  column_list_entry->flags = flags;
  column_list_entry->tag   = strdup (":");
  STRING_CONCAT (column_list_entry->tag, title);
  STRING_REPLACE_CHAR (column_list_entry->tag, ' ', '_');
  STRING_REPLACE_CHAR (column_list_entry->tag, '.', '_');
  list_add ((list_entry_t **)column_list, (list_entry_t *)column_list_entry);
}

static void db_free_column_list (db_column_list_entry_t *column_list)
{
  while (column_list != NULL)
  {
    db_column_list_entry_t *column_list_entry = column_list;
    free (column_list_entry->title);
    free (column_list_entry->tag);
    list_remove ((list_entry_t **)(&column_list), (list_entry_t *)column_list_entry);
    free (column_list_entry);
  }
}

static db_table_list_entry_t * db_find_table (db_table_list_entry_t *table_list_entry, int8 *title)
{
  while (table_list_entry != NULL)
  {
    if ((strcmp (table_list_entry->title, title)) == 0)
    {
      break;
    }

    table_list_entry = table_list_entry->next;
  }

  return table_list_entry;
}

static db_column_list_entry_t * db_find_column (db_column_list_entry_t *column_list_entry,
                                                int8 *title)
{
  while (column_list_entry != NULL)
  {
    if ((strcmp (column_list_entry->title, title)) == 0)
    {
      break;
    }

    column_list_entry = column_list_entry->next;
  }

  return column_list_entry;
}

int32 db_read_column (db_info_t *db_info, int8 *table_title, int8 *column_title,
                      db_column_value_t *column_value)
{
  int status;
  db_table_list_entry_t *table_list_entry = db_find_table (db_info->table_list, table_title);
  
  if (table_list_entry != NULL)
  {
    db_column_list_entry_t *column_list_entry
      = db_find_column (table_list_entry->column_list, column_title);
    
    if (column_list_entry != NULL)
    {
      sqlite3_stmt *select = (sqlite3_stmt *)(table_list_entry->select);

      if (column_list_entry->type == DB_COLUMN_TYPE_TEXT)
      {
        column_value->text = (int8 *)sqlite3_column_text (select,
                                                          column_list_entry->index);
      }
      else if (column_list_entry->type == DB_COLUMN_TYPE_INT)
      {
        column_value->integer = sqlite3_column_int (select,
                                                    column_list_entry->index);
      }
      else if (column_list_entry->type == DB_COLUMN_TYPE_FLOAT)
      {
        column_value->decimal = sqlite3_column_double (select,
                                                       column_list_entry->index);

      }
      else
      {
        column_value->blob.data = (uint8 *)sqlite3_column_blob (select,
                                                                column_list_entry->index);
        column_value->blob.length = sqlite3_column_bytes (select,
                                                          column_list_entry->index);
      }

      status = 1;
    }
    else
    {
      printf ("Can't find database table '%s', column '%s'\n", table_title, column_title);
      status = -1;
    }
  }
  else
  {
    printf ("Can't find database table '%s'\n", table_title);
    status = -1;
  }

  return status;
}

int32 db_write_column (db_info_t *db_info, int8 *table_title, int8 *column_title,
                       db_column_value_t *column_value)
{
  int status;
  db_table_list_entry_t *table_list_entry = db_find_table (db_info->table_list, table_title);
  
  if (table_list_entry != NULL)
  {
    db_column_list_entry_t *column_list_entry
      = db_find_column (table_list_entry->column_list, column_title);
    
    if (column_list_entry != NULL)
    {
      sqlite3_stmt *insert = (sqlite3_stmt *)(table_list_entry->insert);
      
      if (column_list_entry->type == DB_COLUMN_TYPE_TEXT)
      {
        status = sqlite3_bind_text (insert,
                                    (sqlite3_bind_parameter_index (insert, column_list_entry->tag)),
                                    column_value->text, -1, SQLITE_TRANSIENT);
      }
      else if (column_list_entry->type == DB_COLUMN_TYPE_INT)
      {
        status = sqlite3_bind_int (insert,
                                   (sqlite3_bind_parameter_index (insert, column_list_entry->tag)),
                                   column_value->integer);
      }
      else if (column_list_entry->type == DB_COLUMN_TYPE_FLOAT)
      {
        status = sqlite3_bind_double (insert,
                                      (sqlite3_bind_parameter_index (insert, column_list_entry->tag)),
                                      (double)(column_value->decimal));
      }
      else if (column_list_entry->type == DB_COLUMN_TYPE_BLOB)
      {
        status = sqlite3_bind_blob (insert,
                                    (sqlite3_bind_parameter_index (insert, column_list_entry->tag)),
                                    column_value->blob.data, column_value->blob.length, SQLITE_TRANSIENT);
      }
      else
      {
        status = sqlite3_bind_null (insert,
                                    (sqlite3_bind_parameter_index (insert, column_list_entry->tag)));
      }

      if (status == SQLITE_OK)
      {
        status = 1;
      }
      else
      {
        printf ("Can't write database table '%s', column '%s'\n", table_title, column_title);
        status = -1;
      }
    }
    else
    {
      printf ("Can't find database table '%s', column '%s'\n", table_title, column_title);
      status = -1;
    }
  }
  else
  {
    printf ("Can't find database table '%s'\n", table_title);
    status = -1;
  }

  return status;
}

int32 db_read_table (db_info_t *db_info, int8 *title)
{
  int status;
  db_table_list_entry_t *table_list_entry = db_find_table (db_info->table_list, title);

  if (table_list_entry != NULL)
  {
    sqlite3_stmt *select = (sqlite3_stmt *)(table_list_entry->select);
    
    status = sqlite3_step (select);
    
    if (status == SQLITE_ROW)
    {
      status = 1;
    }
    else if (status == SQLITE_DONE)
    {
      status = 0;
      sqlite3_reset (select);
    }
    else
    {
      printf ("Can't read database table '%s'\n", title);
      status = -1;
    }
  }  
  else
  {
    printf ("Can't find database table '%s'\n", title);
    status = -1;
  }

  return status;
}

int32 db_write_table (db_info_t *db_info, int8 *title)
{
  int status;
  db_table_list_entry_t *table_list_entry = db_find_table (db_info->table_list, title);
 
  if (table_list_entry != NULL)
  {
    sqlite3_stmt *insert = (sqlite3_stmt *)(table_list_entry->insert);
    
    status = sqlite3_step (insert);
    
    if (status == SQLITE_DONE)
    {
      status = 1;
    }
    else
    {
      printf ("Can't write database table '%s'\n", title);
      status = -1;
    }

    sqlite3_reset (insert);
  }
  else
  {
    printf ("Can't find database table '%s'\n", title);
    status = -1;
  }

  return status;
}

int32 db_clear_table (db_info_t *db_info, int8 *title)
{
  int status;
  db_table_list_entry_t *table_list_entry = db_find_table (db_info->table_list, title);

  if (table_list_entry != NULL)
  {
    char *sql = NULL;
  
    sql = strdup ("DELETE FROM ");
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, title);
    STRING_CONCAT (sql, "]");
  
    status = sqlite3_exec ((sqlite3 *)(db_info->handle), sql, NULL, NULL, NULL);
    free (sql);

    if (status == SQLITE_OK)
    {
      status = 1;
    }
    else
    {
      printf ("Can't clear database table '%s'\n", title);
      status = -1;
    }
  }
  else
  {
    printf ("Can't find database table '%s'\n", title);
    status = -1;
  }

  return status;
}

int32 db_create_table (db_info_t *db_info, int8 *title, uint32 index,
                       db_column_list_entry_t *column_list)
{
  int status;
  char *sql = NULL;
  db_table_list_entry_t *table_list_entry = (db_table_list_entry_t *)malloc (sizeof (*table_list_entry));

  table_list_entry->title       = strdup (title);
  table_list_entry->column_list = column_list;
  table_list_entry->index       = index;
  table_list_entry->insert      = NULL;
  table_list_entry->select      = NULL;
  
  /* Prepare create statement */
  sql = strdup ("CREATE TABLE IF NOT EXISTS");
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

  status = sqlite3_exec ((sqlite3 *)(db_info->handle), sql, NULL, NULL, NULL);
  free (sql);

  if (status == SQLITE_OK)
  {
    /* Prepare insert statement */
    sql = strdup ("INSERT INTO ");
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, title);
    STRING_CONCAT (sql, "] ( ");

    /* Column title */
    column_list = table_list_entry->column_list;
    while (column_list != NULL)
    {
      if (!(column_list->flags & (DB_COLUMN_FLAG_PRIMARY_KEY | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)))
      {
        /* Column tag */
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, column_list->title);
        STRING_CONCAT (sql, "]");

        /* Comma, if there is column to add, else closing bracket */
        if (column_list->next != NULL)
        {
          STRING_CONCAT (sql, ", ");
        }
        else
        {
          STRING_CONCAT (sql, " ) VALUES ( ");
        }
      }
      
      column_list = column_list->next;
    }

    column_list = table_list_entry->column_list;
    while (column_list != NULL)
    {
      if (!(column_list->flags & (DB_COLUMN_FLAG_PRIMARY_KEY | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)))
      {
        /* Column tag */
        STRING_CONCAT (sql, column_list->tag);

        /* Comma, if there is column to add, else closing bracket */
        if (column_list->next != NULL)
        {
          STRING_CONCAT (sql, ", ");
        }
        else
        {
          STRING_CONCAT (sql, " )");
        }
      }
      
      column_list = column_list->next;
    }
    
    status = sqlite3_prepare_v2 ((sqlite3 *)(db_info->handle), sql, -1,
                                 (sqlite3_stmt **)(&(table_list_entry->insert)), NULL);
    
    if (status != SQLITE_OK)
    {
      printf ("Can't prepare database write statement '%s'\n", sql);
    }
    
    free (sql);
  }
  else
  {
    printf ("Can't create database table '%s'\n", title);    
  }

  if (status == SQLITE_OK)
  {
    /* Prepare select statement */
    sql = strdup ("SELECT * FROM ");
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, title);
    STRING_CONCAT (sql, "]");

    status = sqlite3_prepare_v2 ((sqlite3 *)(db_info->handle), sql, -1,
                                 (sqlite3_stmt **)(&(table_list_entry->select)), NULL);
    
    if (status == SQLITE_OK)
    {
      list_add ((list_entry_t **)(&(db_info->table_list)), (list_entry_t *)table_list_entry);
    }
    else
    {
      printf ("Can't prepare database read statement '%s'\n", sql);
    }
    
    free (sql); 
  }

  if (status == SQLITE_OK)
  {
    status = 1;
  }
  else
  {
    if (table_list_entry->insert != NULL)
    {
      sqlite3_finalize (table_list_entry->insert);
      table_list_entry->insert = NULL;
    }

    if (table_list_entry->select != NULL)
    {
      sqlite3_finalize (table_list_entry->select);
      table_list_entry->select = NULL;
    }
    
    free (table_list_entry->title);
    free (table_list_entry);

    status = -1;
  }

  return status;
}

int32 db_open (int8 *file_name, db_info_t **db_info)
{
  int status;
  sqlite3 *db;

  status = sqlite3_open_v2 (file_name, &db,
                            (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE), NULL);
  if (status == SQLITE_OK)
  {
    *db_info = (db_info_t *)malloc (sizeof (**db_info));
    (*db_info)->handle = (int32)db;
    status = 1;
  }
  else
  {
    printf ("Can't open database %s\n", file_name);
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
      uint32 column_index = 0;
      uint32 table_index  = 0;
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
        db_clear_table (db_info, "Group Title");
        
        column_value.decimal = 98.4;
        if ((db_write_column (db_info, "Group Title", "Temperature (F)", &column_value)) > 0)
        {
          db_write_table (db_info, "Group Title");
        }
        column_value.decimal = 100.4;
        if ((db_write_column (db_info, "Group Title", "Temperature (F)", &column_value)) > 0)
        {
          db_write_table (db_info, "Group Title");
        }

        while ((db_read_table (db_info, "Group Title")) > 0)
        {
          db_read_column (db_info, "Group Title", "No.", &column_value);
          printf ("%3d", column_value.integer);
          db_read_column (db_info, "Group Title", "Time", &column_value);
          printf ("%22s", column_value.text);
          db_read_column (db_info, "Group Title", "Temperature (F)", &column_value);
          printf ("%9.2f\n", column_value.decimal);
        }
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

