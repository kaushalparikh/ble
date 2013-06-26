
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


static void db_prepare_table_columns (db_table_list_entry_t *table_list_entry)
{
  int column = 0;
  int table = 0;

  do
  {
    db_column_list_entry_t *column_list_entry = table_list_entry[table].column_list;
    table_list_entry[table].column_list       = NULL;
    
    column = 0;
    do
    {
      column_list_entry[column].index = column;
      column_list_entry[column].tag   = strdup (":");
      STRING_CONCAT (column_list_entry[column].tag, column_list_entry[column].title);
      STRING_REPLACE_CHAR (column_list_entry[column].tag, ' ', '_');
      STRING_REPLACE_CHAR (column_list_entry[column].tag, '.', '_');
      list_add ((list_entry_t **)&(table_list_entry->column_list), (list_entry_t *)&(column_list_entry[column]));
      column++;
    } while (column_list_entry[column].index != -1);

    table_list_entry[table].index = table;
    table++;
  } while (table_list_entry[table].index != -1);
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

int32 db_create_table (db_info_t *db_info, db_table_list_entry_t *table_list_entry)
{
  int status;
  char *sql = NULL;
  db_column_list_entry_t *column_list_entry = table_list_entry->column_list;

  /* Prepare create statement */
  sql = strdup ("CREATE TABLE IF NOT EXISTS");
  STRING_CONCAT (sql, "[");
  STRING_CONCAT (sql, table_list_entry->title);
  STRING_CONCAT (sql, "] ( ");

  while (column_list_entry != NULL)
  {
    /* Column title */
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, column_list_entry->title);
    STRING_CONCAT (sql, "]");
 
    /* Space, then column data type */
    if (column_list_entry->type != 0)
    {
      if (column_list_entry->type == DB_COLUMN_TYPE_TEXT)
      {
        STRING_CONCAT (sql, " TEXT");
      }
      else if (column_list_entry->type == DB_COLUMN_TYPE_INT)
      {
        STRING_CONCAT (sql, " INTEGER");
      }
      else if (column_list_entry->type == DB_COLUMN_TYPE_FLOAT)
      {
        STRING_CONCAT (sql, " REAL");
      }
      else if (column_list_entry->type == DB_COLUMN_TYPE_BLOB)
      {
        STRING_CONCAT (sql, " BLOB");
      }
    }

    /* Space, then column flags/constraints */
    if (column_list_entry->flags != 0)
    {
      if (column_list_entry->flags & DB_COLUMN_FLAG_PRIMARY_KEY)
      {
        STRING_CONCAT (sql, " PRIMARY KEY");
      }
      if (column_list_entry->flags & DB_COLUMN_FLAG_NOT_NULL)
      {
        STRING_CONCAT (sql, " NOT NULL");
      }
      if (column_list_entry->flags & DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)
      {
        STRING_CONCAT (sql, " DEFAULT (DATETIME('NOW','LOCALTIME'))");
      }
      if (column_list_entry->flags & DB_COLUMN_FLAG_DEFAULT_NA)
      {
        STRING_CONCAT (sql, " DEFAULT NA");
      }
    }

    /* Comma, if there is column to add, else closing bracket */
    if (column_list_entry->next != NULL)
    {
      STRING_CONCAT (sql, ", ");
    }
    else
    {
      STRING_CONCAT (sql, " )");
    }

    column_list_entry = column_list_entry->next;
  }

  status = sqlite3_exec ((sqlite3 *)(db_info->handle), sql, NULL, NULL, NULL);
  free (sql);

  if (status == SQLITE_OK)
  {
    /* Prepare insert statement */
    sql = strdup ("INSERT INTO ");
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, table_list_entry->title);
    STRING_CONCAT (sql, "] ( ");

    /* Column title */
    column_list_entry = table_list_entry->column_list;
    while (column_list_entry != NULL)
    {
      if (!(column_list_entry->flags & (DB_COLUMN_FLAG_PRIMARY_KEY | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)))
      {
        /* Column tag */
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, column_list_entry->title);
        STRING_CONCAT (sql, "]");

        /* Comma, if there is column to add, else closing bracket */
        if (column_list_entry->next != NULL)
        {
          STRING_CONCAT (sql, ", ");
        }
        else
        {
          STRING_CONCAT (sql, " ) VALUES ( ");
        }
      }
      
      column_list_entry = column_list_entry->next;
    }

    column_list_entry = table_list_entry->column_list;
    while (column_list_entry != NULL)
    {
      if (!(column_list_entry->flags & (DB_COLUMN_FLAG_PRIMARY_KEY | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)))
      {
        /* Column tag */
        STRING_CONCAT (sql, column_list_entry->tag);

        /* Comma, if there is column to add, else closing bracket */
        if (column_list_entry->next != NULL)
        {
          STRING_CONCAT (sql, ", ");
        }
        else
        {
          STRING_CONCAT (sql, " )");
        }
      }
      
      column_list_entry = column_list_entry->next;
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
    printf ("Can't create database table '%s'\n", table_list_entry->title);    
  }

  if (status == SQLITE_OK)
  {
    /* Prepare select statement */
    sql = strdup ("SELECT * FROM ");
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, table_list_entry->title);
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

#define NUM_TABLES                (1)
#define DEVICE_TABLE_NUM_COLUMNS  (5)

static db_column_list_entry_t device_table_column_entries[DEVICE_TABLE_NUM_COLUMNS+1] = 
{
  {NULL, "No.",          0,  DB_COLUMN_TYPE_INT,  DB_COLUMN_FLAG_PRIMARY_KEY,                            NULL},
  {NULL, "Address",      0,  DB_COLUMN_TYPE_TEXT, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA), NULL},
  {NULL, "Name",         0,  DB_COLUMN_TYPE_TEXT, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA), NULL},
  {NULL, "Service List", 0,  DB_COLUMN_TYPE_TEXT, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA), NULL},
  {NULL, "Status",       0,  DB_COLUMN_TYPE_TEXT, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA), NULL},
  {NULL, NULL,           -1, DB_COLUMN_TYPE_NULL, 0,                                                     NULL},
};

static db_table_list_entry_t table_entries[NUM_TABLES+1] =
{
  {NULL, device_table_column_entries, "Device List", 0,  NULL, NULL},
  {NULL, NULL,                        NULL,          -1, NULL, NULL},
};


int main (int argc, char *argv[])
{
  db_info_t *db_info = NULL;

  if (argc > 1)
  {
    if ((db_open (argv[1], &db_info)) > 0)
    {
      db_prepare_table_columns (table_entries);

      if ((db_create_table (db_info, &(table_entries[0]))) > 0)
      {
        db_column_value_t column_value;

        /*
        db_clear_table (db_info, "Device List");
        
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
        */

        while ((db_read_table (db_info, table_entries[0].title)) > 0)
        {
          db_column_list_entry_t *column_list_entry = table_entries[0].column_list;

          db_read_column (db_info, table_entries[0].title, column_list_entry[0].title, &column_value);
          printf ("%3d", column_value.integer);
          db_read_column (db_info, table_entries[0].title, column_list_entry[1].title, &column_value);
          printf ("%20s", column_value.text);          
          db_read_column (db_info, table_entries[0].title, column_list_entry[2].title, &column_value);
          printf ("%20s", column_value.text);
          db_read_column (db_info, table_entries[0].title, column_list_entry[3].title, &column_value);
          printf ("%15s", column_value.text);          
          db_read_column (db_info, table_entries[0].title, column_list_entry[4].title, &column_value);
          printf ("%10s\n", column_value.text);
        }
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

