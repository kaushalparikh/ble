#ifndef __LIST_H__
#define __LIST_H__

/* List element */
struct list_entry
{
  struct list_entry *next;
};

typedef struct list_entry list_entry_t;


#define LIST_HEAD_INIT(list_struct, name)  \
  static list_struct *name = NULL


static inline list_entry_t * list_tail (list_entry_t **head)
{
  list_entry_t *entry;

  for (entry = *head; ((entry != NULL) && (entry->next != NULL)); entry = entry->next);

  return entry;  
}

static inline int list_length (list_entry_t **head)
{
  list_entry_t *entry = *head;
  int length = 0;

  for (entry = *head; entry != NULL; entry = entry->next)
  {
    length++;
  }

  return length;
}

static inline void list_add (list_entry_t **head, list_entry_t *new_entry)
{
  list_entry_t *tail;

  new_entry->next = NULL;
  tail = list_tail (head);
  if (tail != NULL)
  {
    tail->next = new_entry;
  }
  else
  {
    *head = new_entry;
  }
}

static inline void list_concat (list_entry_t **head, list_entry_t *new_list)
{
  list_entry_t *tail;

  tail = list_tail (head);
  if (tail != NULL)
  {
    tail->next = new_list;
  }
  else
  {
    *head = new_list;
  }
}

static inline void list_remove (list_entry_t **head, list_entry_t *del_entry)
{
  list_entry_t *entry, *prev_entry;

  for (entry = *head, prev_entry = *head; entry != NULL; entry = entry->next)
  {
    if (entry == del_entry)
    {
      if (entry == *head)
      {
        *head = entry->next;
      }
      else
      {
        prev_entry->next = entry->next;
      }
      entry->next = NULL;
      break;
    }
    
    prev_entry = entry;
  }
}

#endif
