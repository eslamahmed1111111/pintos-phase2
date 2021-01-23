#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

void haltWrapper(void);
void ExitWrapper(void);
tid_t ExecWrapper(void);
int WaitWrapper(void);
int ReadWrapper(void);
int WriteWrapper(void);
void SeekWrapper(void);
bool CreateWrapper(void);
bool RemoveWrapper(void);
int OpenWrapper(void);
unsigned TellWrapper(void);
int FilesizeWrapper(void);
void CloseWrapper(void);
void ValidateVoidPtr(void * add);
void * getVoidPTR(void ** * sPTR);
char * getCharPTR(char ** * sPTR);
int getInt(int ** sPTR);
bool Remove(char* f);
int Open(char *file_name);
tid_t Exec(char *command);
int Filesize(int fID);
unsigned Tell(int fd);
void Close(int fd);
void exit(int status);
void Seek(int fd, unsigned position);
bool Create(char *f, unsigned initial_size);
int Wait(int pid);
int Read(int fd, void* buffer);
int Write(int fd,void *buffer);

void * sPTR;
struct lock file_system_lock;

static void syscall_handler(struct intr_frame * );

void
syscall_init(void) {
  lock_init( & file_system_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame * f UNUSED) {
  sPTR = f -> esp;
  ValidateVoidPtr(f -> esp);
  int sys_call = getInt((int ** ) & sPTR);
  switch (sys_call) {
  case SYS_HALT:
    haltWrapper();
    break;
  case SYS_EXIT:
    ExitWrapper();
    break;
  case SYS_EXEC:
    f -> eax = ExecWrapper();
    break;
  case SYS_WAIT:
    f -> eax = WaitWrapper();
    break;
  case SYS_CREATE:
    f -> eax = CreateWrapper();
    break;
  case SYS_REMOVE:
    f -> eax = RemoveWrapper();
    break;
  case SYS_OPEN:
    f -> eax = OpenWrapper();
    break;
  case SYS_FILESIZE:
    f -> eax = FilesizeWrapper();
    break;
  case SYS_READ:
    f -> eax = ReadWrapper();
    break;
  case SYS_WRITE:
    f -> eax = WriteWrapper();
    break;
  case SYS_SEEK:
    SeekWrapper();
    break;
  case SYS_TELL:
    f -> eax = TellWrapper();
    break;
  case SYS_CLOSE:
    CloseWrapper();
    break;
  default:
    break;
  }
}

void
haltWrapper(void) {
  shutdown_power_off();
}

void
exit(int status) {
  struct thread * child = thread_current();
  struct thread * parent = get_thread(child->parent_id);
  if (parent != NULL) {
    struct child * childInfo = getParentChild(parent, child->tid);
    if (childInfo != NULL) {
      childInfo -> exit_status = status;
    }
  }
  child->exit_status = status;
  thread_exit();
}

tid_t
ExecWrapper(void) {
  char *command = getCharPTR((char***)&sPTR);
  return Exec(command);
}

tid_t
Exec(char *command){
  int pid = process_execute(command);
  if (pid == -1) return -1;
  struct child * child_elem = getParentChild(thread_current(), pid);
  if (child_elem != NULL && child_elem->is_loaded == false) return -1;
  return pid;
}

int
WaitWrapper(void) {
  int pid = getInt((int**)&sPTR);
  return Wait(pid);
}

int 
Wait(int pid){
return process_wait(pid);
}

bool
CreateWrapper(void) {
  char *f = getCharPTR((char***)&sPTR);
  unsigned initial_size = getInt((int**)&sPTR);
  return Create(f,initial_size);
}

bool
Create(char *f, unsigned initial_size){
  lock_acquire(&file_system_lock);
  bool create_status = filesys_create(f, initial_size);
  lock_release(&file_system_lock);
  return create_status;
}


bool
RemoveWrapper(void) {
  char *f = getVoidPTR((char***)&sPTR);
  return Remove(f);
}

bool
Remove(char* f){
  lock_acquire(&file_system_lock);
  bool remove_status = filesys_remove(f);
  lock_release(&file_system_lock);
  return remove_status;
}

int
OpenWrapper(void) {
  char * file_name = getCharPTR((char ** * ) & sPTR);
  return Open(file_name);
}

int
Open(char *file_name){
  struct thread * cur = thread_current();
  lock_acquire(&file_system_lock);
  struct file *f = filesys_open(file_name);
  int ret_value = -1;
  if (f != NULL) {
    struct file_des * file_desc = malloc(sizeof(struct file_des));
    file_desc->file = f;
    file_desc->fid = cur->fd_last++;
    ret_value = file_desc->fid;
    list_push_back(&cur->files,&file_desc->thread_elem);
  }
  lock_release(&file_system_lock);
  return ret_value;
}

int
FilesizeWrapper(void) {
  int fID = getInt((int**)&sPTR);
  return Filesize(fID);
}

int Filesize(int fID){
 struct thread * cur = thread_current();
 struct file_des * file_desc = get_file_des(cur, fID);
  int length = -1;
  if (file_desc != NULL) {
    struct file * f = file_desc -> file;
     lock_acquire(&file_system_lock);
    length = file_length(f);
     lock_release(&file_system_lock);
  }
  return length;
}

int
ReadWrapper(void) {
  int fd = getInt((int**)&sPTR);
  void *buffer = getVoidPTR((void***)&sPTR);
   ValidateVoidPtr(buffer);
   return Read(fd,buffer);
}

int
Read(int fd, void* buffer){
  unsigned length = getInt((int**)&sPTR);
  int ret_value = length;
  lock_acquire(&file_system_lock);
  if (fd == 0)
  {
    for (int i = 0; i < length; i++) {
       uint8_t c = input_getc (); 
      *((uint8_t*) buffer) = c;
      buffer += sizeof(uint8_t);
    }
  } else if (fd == 1){
  //negative area
  } else {
    struct file_des * file_desc = get_file_des(thread_current(), fd);
    if (file_desc == NULL) {
      ret_value = -1;
    }else {
      struct file * f = file_desc -> file;
      ret_value = file_read(f, buffer, length);
    }
  }
  lock_release( & file_system_lock);
  return ret_value;
}

void
SeekWrapper(void) {
  int fd = getInt((int**)&sPTR);
  unsigned position = getInt((int**)&sPTR);
  Seek(fd,position);
}

void 
Seek(int fd, unsigned position){
  struct file_des * file_desc = get_file_des(thread_current(), fd);
  if (file_desc != NULL) {
    struct file * f = file_desc -> file;
    lock_acquire(&file_system_lock);
    file_seek(f, position);
    lock_release(&file_system_lock);
  }
}

int
WriteWrapper(void) {
  int fd = getInt((int**)&sPTR);
  void *buffer = getVoidPTR((void***)&sPTR);
  ValidateVoidPtr(buffer);
  return Write(fd,buffer);
  
}

int 
Write(int fd,void *buffer){
  unsigned length = getInt((int**)&sPTR);
  int ret_value = length;

  if (fd == 1){
    lock_acquire( & file_system_lock);
    putbuf(buffer, length);
    lock_release( & file_system_lock);
  }else {
    struct file_des * file_desc = get_file_des(thread_current(), fd);
    if (file_desc == NULL)
      ret_value = -1;
    else {
      struct file * f = file_desc -> file;
      lock_acquire( & file_system_lock);
      ret_value = file_write(f, buffer, length);
      lock_release( & file_system_lock);
    }
  }
  return ret_value;
}


void
ExitWrapper(void) {
  int status = getInt((int**)&sPTR);
  exit(status);
}

unsigned
TellWrapper(void) {
  int fd = getInt((int ** ) & sPTR);
  return Tell(fd);
}

unsigned 
Tell(int fd){
  int ret_value = 0;
  struct file_des * file_desc = get_file_des(thread_current(), fd);
  if (file_desc == NULL)
    ret_value = 0;
  else {
    struct file * f = file_desc -> file;
    lock_acquire( & file_system_lock);
    ret_value = file_tell(f);
    lock_release( & file_system_lock);
  }
  return ret_value;
}

char *
getCharPTR(char*** esp) {
    ValidateVoidPtr(sPTR);
    char * ret = **esp;
    (*esp) ++;
    ValidateVoidPtr(ret);
    return ret;
  }
void *
getVoidPTR(void*** esp) {
    ValidateVoidPtr(sPTR);
    void *ret = **esp;
    (*esp)++;
    ValidateVoidPtr(ret);
    return ret;
  }

void
CloseWrapper(void) {
  int fd = getInt((int**)&sPTR);
  Close(fd);
}

void 
Close(int fd){
 struct file_des * file_desc = get_file_des(thread_current(), fd);
  if (file_desc != NULL) {
    list_remove(&file_desc->thread_elem);
    file_close(file_desc->file);
    free(file_desc);
  }
}
void
ValidateVoidPtr(void *add) {
  if (add == NULL || is_kernel_vaddr(add) || pagedir_get_page(thread_current()->pagedir, add) == NULL)
    exit(-1);
}
int
getInt(int ** esp) {
  ValidateVoidPtr(sPTR);
  int ret = **esp;
  (*esp) ++;
  return ret;
}
