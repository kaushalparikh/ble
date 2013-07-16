
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "types.h"
#include "list.h"
#include "util.h"


int32 db_read_column (db_table_list_entry_t *table_list_entry,
                      uint32 index, db_column_value_t *column_value)
{
  int status = 1;
  sqlite3_stmt *statement = (sqlite3_stmt *)(table_list_entry->select);

  if (table_list_entry->column[index].type == DB_COLUMN_TYPE_TEXT)
  {
    column_value->text = (int8 *)sqlite3_column_text (statement, index);
  }
  else if (table_list_entry->column[index].type == DB_COLUMN_TYPE_INT)
  {
    column_value->integer = sqlite3_column_int (statement, index);
  }
  else if (table_list_entry->column[index].type == DB_COLUMN_TYPE_FLOAT)
  {
    column_value->decimal = sqlite3_column_double (statement, index);

  }
  else
  {
    column_value->blob.data   = (uint8 *)sqlite3_column_blob (statement, index);
    column_value->blob.length = sqlite3_column_bytes (statement, index);
  }

  return status;
}

int32 db_write_column (db_table_list_entry_t *table_list_entry, uint8 type,
                       uint32 index, db_column_value_t *column_value)
{
  int status;
  sqlite3_stmt *statement;

  if (type == DB_WRITE_INSERT)
  {
    statement = (sqlite3_stmt *)(table_list_entry->insert);
  }
  else if (type == DB_WRITE_UPDATE)
  {
    statement = (sqlite3_stmt *)(table_list_entry->update);
  }
  else
  {
    statement = (sqlite3_stmt *)(table_list_entry->delete);
  }
  
  if (column_value != NULL)
  {
    if (table_list_entry->column[index].type == DB_COLUMN_TYPE_TEXT)
    {
      status = sqlite3_bind_text (statement,
                                  (sqlite3_bind_parameter_index (statement, table_list_entry->column[index].tag)),
                                  column_value->text, -1, SQLITE_TRANSIENT);
    }
    else if (table_list_entry->column[index].type == DB_COLUMN_TYPE_INT)
    {
      status = sqlite3_bind_int (statement,
                                 (sqlite3_bind_parameter_index (statement, table_list_entry->column[index].tag)),
                                 column_value->integer);
    }
    else if (table_list_entry->column[index].type == DB_COLUMN_TYPE_FLOAT)
    {
      status = sqlite3_bind_double (statement,
                                    (sqlite3_bind_parameter_index (statement, table_list_entry->column[index].tag)),
                                    (double)(column_value->decimal));
    }
    else
    {
      status = sqlite3_bind_blob (statement,
                                  (sqlite3_bind_parameter_index (statement, table_list_entry->column[index].tag)),
                                  column_value->blob.data, column_value->blob.length, SQLITE_TRANSIENT);
    }
  }
  else
  {   
    status = sqlite3_bind_text (statement,
                                (sqlite3_bind_parameter_index (statement, table_list_entry->column[index].tag)),
                                "NA", -1, SQLITE_TRANSIENT);
  }

  if (status == SQLITE_OK)
  {
    status = 1;
  }
  else
  {
    printf ("Can't write database table '%s', column '%s'\n", table_list_entry->title, table_list_entry->column[index].title);
    status = -1;
  }

  return status;
}

int32 db_read_table (db_table_list_entry_t *table_list_entry)
{
  int status;
  sqlite3_stmt *statement = (sqlite3_stmt *)(table_list_entry->select);
    
  status = sqlite3_step (statement);
  if (status == SQLITE_ROW)
  {
    status = 1;
  }
  else if (status == SQLITE_DONE)
  {
    status = 0;
    sqlite3_reset (statement);
  }
  else
  {
    printf ("Can't read database table '%s'\n", table_list_entry->title);
    status = -1;
  }

  return status;
}

int32 db_write_table (db_table_list_entry_t *table_list_entry, uint8 type)
{
  int status;
  sqlite3_stmt *statement;

  if (type == DB_WRITE_INSERT)
  {
    statement = (sqlite3_stmt *)(table_list_entry->insert);
  }
  else if (type == DB_WRITE_UPDATE)
  {
    statement = (sqlite3_stmt *)(table_list_entry->update);
  }
  else
  {
    statement = (sqlite3_stmt *)(table_list_entry->delete);
  }
    
  status = sqlite3_step (statement);
  if (status == SQLITE_DONE)
  {
    status = 1;
  }
  else
  {
    printf ("Can't write database table '%s'\n", table_list_entry->title);
    status = -1;
  }

  sqlite3_reset (statement);

  return status;
}

int32 db_delete_table (db_info_t *db_info, db_table_list_entry_t *table_list_entry)
{
  int status;
  char *sql = NULL;
  
  sql = strdup ("DROP TABLE IF EXISTS ");
  STRING_CONCAT (sql, "[");
  STRING_CONCAT (sql, table_list_entry->title);
  STRING_CONCAT (sql, "]");

  status = sqlite3_exec ((sqlite3 *)(db_info->handle), sql, NULL, NULL, NULL);
  free (sql);

  if (status == SQLITE_OK)
  {
    if (table_list_entry->insert != NULL)
    {
      sqlite3_finalize (table_list_entry->insert);
      table_list_entry->insert = NULL;
    }

    if (table_list_entry->update != NULL)
    {
      sqlite3_finalize (table_list_entry->update);
      table_list_entry->update = NULL;
    }

    if (table_list_entry->delete != NULL)
    {
      sqlite3_finalize (table_list_entry->delete);
      table_list_entry->delete = NULL;
    }

    if (table_list_entry->select != NULL)
    {
      sqlite3_finalize (table_list_entry->select);
      table_list_entry->select = NULL;
    }

    list_remove ((list_entry_t **)(&(db_info->table_list)), (list_entry_t *)table_list_entry);
    status = 1;
  }
  else
  {
    printf ("Can't delete database table '%s'\n", table_list_entry->title);
    status = -1;
  }

  return status;
}

int32 db_create_table (db_info_t *db_info, db_table_list_entry_t *table_list_entry)
{
  int status;
  int index;
  char *sql;

  /* Prepare create statement */
  sql = strdup ("CREATE TABLE IF NOT EXISTS");
  STRING_CONCAT (sql, "[");
  STRING_CONCAT (sql, table_list_entry->title);
  STRING_CONCAT (sql, "] ( ");

  for (index = 0; index < table_list_entry->num_columns; index++)
  {
    if (table_list_entry->column[index].tag == NULL)
    {
      table_list_entry->column[index].tag   = strdup (":");
      STRING_CONCAT (table_list_entry->column[index].tag, table_list_entry->column[index].title);
      string_replace_char (table_list_entry->column[index].tag, ' ', '_');
      string_replace_char (table_list_entry->column[index].tag, '.', '_');
    }

    /* Column title */
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, table_list_entry->column[index].title);
    STRING_CONCAT (sql, "]");

    /* Space, then column data type */
    if (table_list_entry->column[index].type == DB_COLUMN_TYPE_TEXT)
    {
      STRING_CONCAT (sql, " TEXT COLLATE NOCASE");
    }
    else if (table_list_entry->column[index].type == DB_COLUMN_TYPE_INT)
    {
      STRING_CONCAT (sql, " INTEGER");
    }
    else if (table_list_entry->column[index].type == DB_COLUMN_TYPE_FLOAT)
    {
      STRING_CONCAT (sql, " REAL");
    }
    else
    {
      STRING_CONCAT (sql, " BLOB");
    }

    /* Space, then column flags/constraints */
    if (table_list_entry->column[index].flags != 0)
    {
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_PRIMARY_KEY)
      {
        STRING_CONCAT (sql, " PRIMARY KEY");
      }
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_NOT_NULL)
      {
        STRING_CONCAT (sql, " NOT NULL");
      }
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)
      {
        STRING_CONCAT (sql, " DEFAULT (DATETIME('NOW','LOCALTIME'))");
      }
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_DEFAULT_NA)
      {
        STRING_CONCAT (sql, " DEFAULT NA");
      }
    }

    /* Comma, if there is column to add, else closing bracket */
    if ((index + 1) < table_list_entry->num_columns)
    {
      STRING_CONCAT (sql, ", ");
    }
    else
    {
      STRING_CONCAT (sql, " )");
    }
  }

  status = sqlite3_exec ((sqlite3 *)(db_info->handle), sql, NULL, NULL, NULL);
  free (sql);
  sql = NULL;

  if (status == SQLITE_OK)
  {
    /* Prepare insert statement */
    sql = strdup ("INSERT INTO ");
    STRING_CONCAT (sql, "[");
    STRING_CONCAT (sql, table_list_entry->title);
    STRING_CONCAT (sql, "]");

    /* Column title */
    for (index = 0; index < table_list_entry->num_columns; index++)
    {
      if (!(table_list_entry->column[index].flags & (DB_COLUMN_FLAG_PRIMARY_KEY | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)))
      {
        STRING_CONCAT (sql, " ( ");
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->column[index].title);
        STRING_CONCAT (sql, "]");
        break;
      }
    }

    for (; index < table_list_entry->num_columns; index++)
    {
      if (!(table_list_entry->column[index].flags & (DB_COLUMN_FLAG_PRIMARY_KEY | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)))
      {
        STRING_CONCAT (sql, ", ");
        
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->column[index].title);
        STRING_CONCAT (sql, "]");
      }
    }

    STRING_CONCAT (sql, " ) VALUES ( ");

    /* Column tag */
    for (index = 0; index < table_list_entry->num_columns; index++)
    {
      if (!(table_list_entry->column[index].flags & (DB_COLUMN_FLAG_PRIMARY_KEY | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)))
      {
        STRING_CONCAT (sql, table_list_entry->column[index].tag);
        break;
      }
    }

    for (; index < table_list_entry->num_columns; index++)
    {
      if (!(table_list_entry->column[index].flags & (DB_COLUMN_FLAG_PRIMARY_KEY | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP)))
      {
        STRING_CONCAT (sql, ", ");
        STRING_CONCAT (sql, table_list_entry->column[index].tag);
      }
    }

    STRING_CONCAT (sql, " )");

    status = sqlite3_prepare_v2 ((sqlite3 *)(db_info->handle), sql, -1,
                                 (sqlite3_stmt **)(&(table_list_entry->insert)), NULL);
    
    if (status != SQLITE_OK)
    {
      printf ("Can't prepare database insert statement '%s'\n", sql);
    }
    
    free (sql);
    sql = NULL;
  }
  else
  {
    printf ("Can't create database table '%s'\n", table_list_entry->title);    
  }

  if (status == SQLITE_OK)
  {
    /* Prepare update statement   */
    /* Column value(s) to be updated */
    for (index = 0; index < table_list_entry->num_columns; index++)
    {
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_UPDATE_VALUE)
      {
        sql = strdup ("UPDATE ");
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->title);
        STRING_CONCAT (sql, "] SET ");
  
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->column[index].title);
        STRING_CONCAT (sql, "] = ");
        STRING_CONCAT (sql, table_list_entry->column[index].tag);
        break;
      }
    }

    for (; index < table_list_entry->num_columns; index++)
    {
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_UPDATE_VALUE)
      {
        STRING_CONCAT (sql, ", ");
  
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->column[index].title);
        STRING_CONCAT (sql, "] = ");
        STRING_CONCAT (sql, table_list_entry->column[index].tag);
      }
    }

    /* Column update key */
    for (index = 0; index < table_list_entry->num_columns; index++)
    {
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_UPDATE_KEY)
      {
        STRING_CONCAT (sql, " WHERE ");
        
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->column[index].title);
        STRING_CONCAT (sql, "] = ");
        STRING_CONCAT (sql, table_list_entry->column[index].tag);
        break;
      }
    }

    for (; index < table_list_entry->num_columns; index++)
    {
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_UPDATE_KEY)
      {
        STRING_CONCAT (sql, " AND ");
        
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->column[index].title);
        STRING_CONCAT (sql, "] = ");
        STRING_CONCAT (sql, table_list_entry->column[index].tag);
      }
    }

    if (sql != NULL)
    {
      status = sqlite3_prepare_v2 ((sqlite3 *)(db_info->handle), sql, -1,
                                   (sqlite3_stmt **)(&(table_list_entry->update)), NULL);
    
      if (status != SQLITE_OK)
      {
        printf ("Can't prepare database update statement '%s'\n", sql);
      }
    
      free (sql);
    }
  }

  if (status == SQLITE_OK)
  {
    /* Prepare delete statement   */
    /* Column value(s) to be deleted */
    for (index = 0; index < table_list_entry->num_columns; index++)
    {
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_UPDATE_KEY)
      {
        sql = strdup ("DELETE FROM ");
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->title);
        STRING_CONCAT (sql, "] WHERE ");
        
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->column[index].title);
        STRING_CONCAT (sql, "] = ");
        STRING_CONCAT (sql, table_list_entry->column[index].tag);
        break;
      }
    }

    for (; index < table_list_entry->num_columns; index++)
    {
      if (table_list_entry->column[index].flags & DB_COLUMN_FLAG_UPDATE_KEY)
      {
        STRING_CONCAT (sql, " AND ");
        
        STRING_CONCAT (sql, "[");
        STRING_CONCAT (sql, table_list_entry->column[index].title);
        STRING_CONCAT (sql, "] = ");
        STRING_CONCAT (sql, table_list_entry->column[index].tag);
      }
    }

    if (sql != NULL)
    {
      status = sqlite3_prepare_v2 ((sqlite3 *)(db_info->handle), sql, -1,
                                   (sqlite3_stmt **)(&(table_list_entry->delete)), NULL);
    
      if (status != SQLITE_OK)
      {
        printf ("Can't prepare database delete statement '%s'\n", sql);
      }
    
      free (sql);
    }
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
    sql = NULL;
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

    if (table_list_entry->update != NULL)
    {
      sqlite3_finalize (table_list_entry->update);
      table_list_entry->update = NULL;
    }

    if (table_list_entry->delete != NULL)
    {
      sqlite3_finalize (table_list_entry->delete);
      table_list_entry->delete = NULL;
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

  *db_info = (db_info_t *)malloc (sizeof (**db_info));
  status   = sqlite3_open_v2 (file_name, &db,
                              (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE), NULL);
  
  if (status == SQLITE_OK)
  {
    (*db_info)->handle     = db;
    (*db_info)->table_list = NULL;
    status = 1;
  }
  else
  {
    printf ("Can't open database %s\n", file_name);
    
    free (*db_info);
    *db_info = NULL;
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

#define NUM_STATIC_TABLES              (1)
#define DEVICE_TABLE_NUM_COLUMNS       (6)
#define TEMPERATURE_TABLE_NUM_COLUMNS  (4)

static db_column_entry_t device_table_columns[DEVICE_TABLE_NUM_COLUMNS] = 
{
  {"No.",      0,  DB_COLUMN_TYPE_INT,  DB_COLUMN_FLAG_NOT_NULL,                                 NULL},
  {"Address",  1,  DB_COLUMN_TYPE_BLOB, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_UPDATE_KEY),   NULL},
  {"Name",     2,  DB_COLUMN_TYPE_TEXT, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),   NULL},
  {"Service",  3,  DB_COLUMN_TYPE_BLOB, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_UPDATE_KEY),   NULL},
  {"Interval", 4,  DB_COLUMN_TYPE_INT,  (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),   NULL},
  {"Status",   5,  DB_COLUMN_TYPE_TEXT, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA 
                                                                 | DB_COLUMN_FLAG_UPDATE_VALUE), NULL}
};

static db_column_entry_t temperature_table_columns[TEMPERATURE_TABLE_NUM_COLUMNS] =
{
  {"No.",               0, DB_COLUMN_TYPE_INT,   DB_COLUMN_FLAG_PRIMARY_KEY,                                   NULL},
  {"Time",              1, DB_COLUMN_TYPE_TEXT,  (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_TIMESTAMP), NULL},
  {"Temperature (C)",   2, DB_COLUMN_TYPE_FLOAT, (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),        NULL},
  {"Battery Level (%)", 3, DB_COLUMN_TYPE_INT,   (DB_COLUMN_FLAG_NOT_NULL | DB_COLUMN_FLAG_DEFAULT_NA),        NULL},
};

static db_table_list_entry_t static_tables[NUM_STATIC_TABLES] =
{
  {NULL, "Device List", DEVICE_TABLE_NUM_COLUMNS, device_table_columns, NULL, NULL, NULL, NULL}
};


int main (int argc, char *argv[])
{
  db_info_t *db_info = NULL;

  if (argc > 1)
  {
    if ((db_open (argv[1], &db_info)) > 0)
    {
      int repeat = 2;

      while (repeat > 0)
      {
        db_table_list_entry_t *table_list_entry;
        db_column_value_t column_value;

        if ((db_create_table (db_info, &(static_tables[0]))) > 0)
        {
          uint8 address[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
          uint8 temperature_service[] = {0x18, 0x09};
          uint8 battery_service[] = {0x18, 0x0f};
  
          column_value.integer = 1;
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 0, &column_value);
          column_value.blob.data   = address;
          column_value.blob.length = 6;
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 1, &column_value);
          column_value.text = "Temperature Sensor";
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 2, &column_value);
          column_value.blob.data   = temperature_service;
          column_value.blob.length = 2;
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 3, &column_value);
          column_value.integer = 15;
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 4, &column_value);
          column_value.text = "NA";
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 5, &column_value);
          db_write_table (&(static_tables[0]), DB_WRITE_INSERT);
          
          column_value.integer = 1;
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 0, &column_value);
          column_value.blob.data   = address;
          column_value.blob.length = 6;
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 1, &column_value);
          column_value.text = "Temperature Sensor";
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 2, &column_value);
          column_value.blob.data   = battery_service;
          column_value.blob.length = 2;
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 3, &column_value);
          column_value.integer = 60;
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 4, &column_value);
          column_value.text = "NA";
          db_write_column (&(static_tables[0]), DB_WRITE_INSERT, 5, &column_value);
          db_write_table (&(static_tables[0]), DB_WRITE_INSERT);
          
          printf ("Write table\n");
          while ((db_read_table (&(static_tables[0]))) > 0)
          {
            int byte;

            db_read_column (&(static_tables[0]), 0, &column_value);
            printf ("%3d", column_value.integer);
            db_read_column (&(static_tables[0]), 1, &column_value);
            printf ("\t0x");
            for (byte = 0; byte < column_value.blob.length; byte++)
            {
              printf ("%02x", column_value.blob.data[byte]);
            }
            printf ("\t");
            db_read_column (&(static_tables[0]), 2, &column_value);
            printf ("%20s", column_value.text);
            db_read_column (&(static_tables[0]), 3, &column_value);
            printf ("\t0x");
            for (byte = 0; byte < column_value.blob.length; byte++)
            {
              printf ("%02x", column_value.blob.data[byte]);
            }
            printf ("\t");
            db_read_column (&(static_tables[0]), 4, &column_value);
            printf ("%4d", column_value.integer);
            db_read_column (&(static_tables[0]), 5, &column_value);
            printf ("%10s\n", column_value.text);
          }
  
          column_value.blob.data   = address;
          column_value.blob.length = 6;
          db_write_column (&(static_tables[0]), DB_WRITE_UPDATE, 1, &column_value);
          column_value.blob.data   = temperature_service;
          column_value.blob.length = 2;
          db_write_column (&(static_tables[0]), DB_WRITE_UPDATE, 3, &column_value);
          column_value.text = "Active";
          db_write_column (&(static_tables[0]), DB_WRITE_UPDATE, 5, &column_value);
          db_write_table (&(static_tables[0]), DB_WRITE_UPDATE);
          
          column_value.blob.data   = address;
          column_value.blob.length = 6;
          db_write_column (&(static_tables[0]), DB_WRITE_UPDATE, 1, &column_value);
          column_value.blob.data   = battery_service;
          column_value.blob.length = 2;
          db_write_column (&(static_tables[0]), DB_WRITE_UPDATE, 3, &column_value);
          column_value.text = "Inactive";
          db_write_column (&(static_tables[0]), DB_WRITE_UPDATE, 5, &column_value);
          db_write_table (&(static_tables[0]), DB_WRITE_UPDATE);
          
          printf ("Update table\n");
          while ((db_read_table (&(static_tables[0]))) > 0)
          {
            int byte;

            db_read_column (&(static_tables[0]), 0, &column_value);
            printf ("%3d", column_value.integer);
            db_read_column (&(static_tables[0]), 1, &column_value);
            printf ("\t0x");
            for (byte = 0; byte < column_value.blob.length; byte++)
            {
              printf ("%02x", column_value.blob.data[byte]);
            }
            printf ("\t");
            db_read_column (&(static_tables[0]), 2, &column_value);
            printf ("%20s", column_value.text);
            db_read_column (&(static_tables[0]), 3, &column_value);
            printf ("\t0x");
            for (byte = 0; byte < column_value.blob.length; byte++)
            {
              printf ("%02x", column_value.blob.data[byte]);
            }
            printf ("\t");
            db_read_column (&(static_tables[0]), 4, &column_value);
            printf ("%4d", column_value.integer);
            db_read_column (&(static_tables[0]), 5, &column_value);
            printf ("%10s\n", column_value.text);
          }
  
          db_delete_table (db_info, &(static_tables[0]));
        }

        table_list_entry = (db_table_list_entry_t *)malloc (sizeof (*table_list_entry));
        
        table_list_entry->title       = strdup ("Temperature Sensor");
        table_list_entry->num_columns = TEMPERATURE_TABLE_NUM_COLUMNS;
        table_list_entry->column      = temperature_table_columns;
        table_list_entry->insert      = NULL;
        table_list_entry->update      = NULL;
        table_list_entry->delete      = NULL;
        table_list_entry->select      = NULL;

        if ((db_create_table (db_info, table_list_entry)) > 0)
        {
          db_write_column (table_list_entry, DB_WRITE_INSERT, 2, NULL);
          db_write_column (table_list_entry, DB_WRITE_INSERT, 3, NULL);
          column_value.decimal = 98.4;
          db_write_column (table_list_entry, DB_WRITE_INSERT, 2, &column_value);
          db_write_table (table_list_entry, DB_WRITE_INSERT);

          printf ("Write table\n");
          while ((db_read_table (table_list_entry)) > 0)
          {
            db_read_column (table_list_entry, 0, &column_value);
            printf ("%3d", column_value.integer);
            db_read_column (table_list_entry, 1, &column_value);
            printf ("%22s", column_value.text);
            db_read_column (table_list_entry, 2, &column_value);
            printf ("%8.1f", column_value.decimal);
            db_read_column (table_list_entry, 3, &column_value);
            printf ("%7d\n", column_value.integer);
          }

          db_delete_table (db_info, table_list_entry);
        }

        free (table_list_entry);

        repeat--;
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

