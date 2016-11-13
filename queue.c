/*  Author: Dave Van
 *  CMSC 421
 *  Section 01
 *  Professor Park
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>

//struct that creates and holds queues
typedef struct Queue{
  long id;    //queue ID
  int size;   //size counter
  spinlock_t lock;
  struct list_head list; //list that holds all queues
  struct list_head qList; //list that holds nodes
}Queue;

//struct that creates a node of a queue
typedef struct QueueItem{
  long len;    //len of the data
  void *data;  //the data being copied
  struct list_head nodeList;   //list that holds all the nodes
}QueueItem;

static LIST_HEAD(queue);
DEFINE_SEMAPHORE(lock);

// helper function to find a queue
static Queue *__sys_queue_find421(unsigned long id)
{
  Queue *i;

  list_for_each_entry(i, &queue, list)
    if(i->id == id)
      return i;

  return NULL;
}

/*Allocate internal data structures for a new queue with the given ID.
 If a queue with ID id already exists, return -EEXIST. 
 If there is insufficient memory to create the queue, return -ENOMEM. 
 Return 0 on success
*/
asmlinkage long sys_createQueue421(unsigned long id)
{
  Queue *q;
  Queue *i;

  printk("**************CREATING QUEUE****************\n");

  //check if queue is already exist
  list_for_each_entry(i, &queue, list)
    if(i->id == id){
      printk("ID %lu already exist\n", i->id);
      return -EEXIST;
    }

  //allocate memory for the queue
  q = (Queue*) kmalloc(sizeof(Queue), GFP_KERNEL);
  
  //error
  if(!q){
    printk("Not enough memory to create queue with ID: %lu\n", id);
    return -ENOMEM;
  }

  //initialize list head of the queue
  INIT_LIST_HEAD(&(q->qList));

  //get the id and initialize the size
  q->id = id;
  q->size = 0;

  printk("Creating Queue ID: %lu\n", q->id);

  //add queue into the list
  list_add(&(q->list), &queue);

  //print out every queue in the list
  list_for_each_entry(i, &queue, list)
    printk("Queues inside the list: %lu\n", i->id);

  //success!
  return 0;
}

/*
Adds a new item to the tail of the queue with the given ID. 
If a queue with ID id does not exist, return -ENOENT. 
If there is insufficient memory to add the item to the queue or to store the data, return -ENOMEM. 
If there is an error accessing the user-space pointer to copy data, return -EFAULT.
If the len parameter is negative, return -EINVAL. 
Return 0 on success.
On any error, ensure that you do not modify any data in the queue. 
The parameter len is the number of bytes you should copy starting at the data pointer.
*/
asmlinkage long sys_enqueue421(unsigned long id, const void __user *data, long len)
{
  QueueItem *qi;
  QueueItem *i;
  Queue *q;
  void *copiedData;
  unsigned long bytesNotCopied;

  printk("***************ENQUEUING**************\n");

  //check if queue exist  
  if((q = __sys_queue_find421(id)) == NULL){
    printk("Queue %lu doesn't exist\n", id);
    return -ENOENT;
  }

  //allocate memory for the queue item
  if((qi = (QueueItem *) kmalloc(sizeof(QueueItem), GFP_KERNEL)) == NULL){
    printk("Not enough memory to create node in queue with ID: %lu\n", q->id);
    return -ENOMEM;
  }

  //allocate memory for data to be copied
  if((copiedData = kmalloc(len, GFP_KERNEL)) == NULL){
    printk("Not enough memory to store the data\n");
    return -ENOMEM;
  }
  
  //check if access is ok
  if(access_ok(VERIFY_WRITE, data, len)){
    printk("Access ok, copying data\n");
    bytesNotCopied = copy_from_user(copiedData, data, len);
    printk("Bytes not copied: %lu\n", bytesNotCopied);
  }
  else{
    printk("Access_ok failed\n");
    return -EFAULT;
  }

  //check if len is negative
  if(len < 0){
    printk("Len is a negative");
    return -EINVAL;
  }

  //set the data and len
  qi->data = copiedData;
  qi->len = len;
  
  //increment size of queue
  q->size += 1;

  //initialize the list head of nodeList
  INIT_LIST_HEAD(&qi->nodeList);

  //locking
  down_interruptible(&lock);
  
  //add the item to the back of the list
  list_add_tail(&(qi->nodeList), &(q->qList));

  //unlock
  up(&lock);

  //print out information about the queue node
  printk("Queue ID: %lu\n", q->id);
  printk("Data being copied: %s\n", qi->data);
  printk("Len of the Data: %lu\n", qi->len);  
  
  /*
  list_for_each_entry(i, &qi->nodeList, nodeList){
    printk("**Things inside the queue**\n");
    printk("Nodes in queue: %p\n", i->data );
    printk("Node Len: %lu\n", i->len);
    }*/

  return 0;
}


/*Removes an item from the head of the queue with the given ID, storing the data back to user-space. 
If a queue with ID id does not exist, return -ENOENT. 
If the queue is empty, return -ENODATA. 
If there is an error accessing the user-space pointer to copy data, return -EFAULT. 
If the len parameter is negative, return -EINVAL. 
If the len parameter indicates that the space allocated in user-space for data is not large enough to store the entire message, return -E2BIG. 
Return 0 on success. 
On any error, ensure that you do not modify any data in the queue. 
The parameter len indicates how many bytes of space should be allocated starting at the pointer data. 
If len bytes is greater than the size of the element at the head of the queue, simply copy the number of bytes that the head element contains, and ignore any extra space provided.
*/
asmlinkage long sys_dequeue421(unsigned long id, void __user *data, long len)
{
  QueueItem *qi;
  Queue *q;
  unsigned long bytesNotCopied;

  printk("***********DEQUEUING********\n");

  //check if queue exist  
  if((q = __sys_queue_find421(id)) == NULL){
    printk("Queue %lu doesn't exist\n", id);
    return -ENOENT;
  }
  
  //check if queue is empty
  if(list_empty(&(q->qList))){
    printk("Queue is empty\n");
    return -ENODATA;
  }

  down_interruptible(&lock);
  
  //grab the first element in the queue
  qi = list_first_entry(&(q->qList), QueueItem, nodeList);

  printk("Queue ID: %lu\n", q->id);
  printk("Node Data: %s\n", qi->data);
  printk("Node Len: %lu\n", qi->len);
  
  //check if access is ok
  if(access_ok(VERIFY_WRITE, data, len)){
    printk("Access ok, copying data\n");
    bytesNotCopied = copy_to_user(data, qi->data, len);
    printk("Bytes not copied: %lu\n", bytesNotCopied);
  }
  else{
    printk("Access_ok failed\n");
    return -EFAULT;
  }

  //check if len is negative
  if(len < 0){
    printk("Len is a negative");
    return -EINVAL;
  }
  
  //delete and free the node
  list_del(&qi->nodeList);
  kfree(qi);

  //decrement size
  q->size -= 1;

  //unlocking
  up(&lock);

  return 0;
}

/*Retrieves the length of the element at the head of the queue with the given ID, in bytes. 
If a queue with ID id does not exist, return -ENOENT. 
If the queue is empty, return -ENODATA. 
On success, return the size of the data in the element at the head of the queue, in bytes.
*/
asmlinkage long sys_peekLen421(unsigned long id)
{ 
  Queue *q;  
  QueueItem *qi;

  printk("***************PEEKLEN*****************\n");

   //check if queue exist
  if((q = __sys_queue_find421(id)) == NULL){
    printk("Queue %lu doesn't exist\n", id);
    return -ENOENT;
  }

  //check if queue is empty
  if(list_empty(&(q->qList))){
    printk("Queue is empty\n");
    return -ENODATA;
  }
  
  //grab the first element in the queue
  qi = list_first_entry(&(q->qList), QueueItem, nodeList);

  //print out the first element in the queue
  printk("Queue ID: %lu \n", q->id);
  printk("First Element in queue with len: %lu\n", qi->len);

  //return the len
  return qi->len;
}

/* Retrieves the number of elements in the queue with the given ID. 
If a queue with ID id does not exist, return -ENOENT. 
On success, return the number of elements in the queue.
*/
asmlinkage long sys_queueLen421(unsigned long id)
{
  Queue *q;  

  printk("***************QUEUELEN*****************\n");

   //check if queue exist
  if((q = __sys_queue_find421(id)) == NULL){
    printk("Queue %lu doesn't exist\n", id);
    return -ENOENT;
  }

  //print the number of nodes in the queue
  printk("Queue Len ID: %lu \n", q->id);
  printk("Number of queue nodes: %d\n", q->size);

  //success!
  return 0;
}

/* Deletes the queue with the specified ID, freeing all allocated memory and releasing all elements stored within. 
If a queue with ID id does not exist, return -ENOENT. 
On success, return 0.
*/
asmlinkage long sys_removeQueue421(unsigned long id)
{
  QueueItem *qi, *next;
  Queue *q, *i;

  printk("*******************REMOVING QUEUE****************\n");

  //check if queue exist
  if((q = __sys_queue_find421(id)) == NULL){
    printk("Queue ID %lu does not exist\n", id);
    return -ENOENT;
  }

  //locking
  down_interruptible(&lock);

  //free each queue node
  list_for_each_entry_safe(qi, next, &qi->nodeList, nodeList){
    list_del(&qi->nodeList);
    kfree(qi);
  }

  //delete the queue and free
  list_del(&q->list);
  kfree(q);

  //unlocking
  up(&lock);

  printk("Remove Queue ID: %lu \n", id);

  //print every queue in list
  //print out every queue in the list
  list_for_each_entry(i, &queue, list)
    printk("Queues inside the list: %lu\n", i->id);
 
  //success!
  return 0;
}
