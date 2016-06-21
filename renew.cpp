#include "goldchase.h"
#include "Map.h"
#include <string>
#include <cstring>
#include <mqueue.h>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fstream>
#include <iostream>
#include <fstream>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include<signal.h>
#include <sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h> //for memset
#include<stdio.h> //for fprintf, stderr, etc.
#include<stdlib.h> //for exit
#include <sys/wait.h>
#include<cassert>
#define DEBUG(msg, ...) do{sprintf(debugStr,msg, ##__VA_ARGS__); WRITE(debugFD,debugStr, strlen(debugStr));}while(0)
#define NAMEDPIPE "/home/kunalbhardwaj/school/611/project/pipe"


using namespace std;

struct GameBoard
{
  int rows; 
  int cols;
  pid_t pid[5];
  int daemonId;
  unsigned char map[0];
};
/////////////////////Global Variables////////////////////////////////////////////////////////
char debugStr[100];
int debugFD=-1;
unsigned char *localmap=NULL;
void server_daemon_setup();
void client_daemon_setup(const char* );
int a=-1 ,fd=-1;
int sockfd=-1; //file descriptor for the socket
mqd_t mq_fd;
sem_t *my_sem=NULL;
GameBoard *goldmap=NULL;
Map* gmp=NULL;
string name[5] = {"/Player1", "/Player2","/Player3","/Player4","/Player5"};
void read_message(int);
unsigned char current_player=0;
int Cur_loc=-1;
void clean_up(int);
void SIGHUP_HANDLER(int a);
void SIGUSR1_Handler(int x);
void handle_interrupt(int);
void the_while();

//////////////////////////////////////////Fancy READ & WRITE///////////////////////////////////////

  template<typename T>
int READ(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  //loop. Read repeatedly until count bytes are read in
  int bytesRead = 0;
  int result;
  while (bytesRead < count)
  {
    result = read(fd,addr + bytesRead, count - bytesRead);
    if (result <= 0 )
    {
      if(errno == EINTR)
      {
        continue;
      }
      else
        return -1;
      break;
    }

    bytesRead += result;
  }
  return bytesRead;
}



  template<typename T>
int WRITE(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  //loop. Write repeatedly until count bytes are written out
  int bytesRead = 0;
  int result;
  while (bytesRead < count)
  {
    result = write(fd,addr + bytesRead, count - bytesRead);
    if (result <= 0 )
    {
      if(errno == EINTR)
      {
        continue;
      }
      else
        return -1;
      break;
    }

    bytesRead += result;
  }
  return bytesRead;

}

/////////////////////////////////////int main()//////////////////////////////

int main(int argc, char* argv[])
{
  //srand((unsigned)time(NULL));
  srand(43434343);
  debugFD=open(NAMEDPIPE,O_WRONLY);
  //assert(debugFD!=-1);
  if(argc == 2)
  {
    client_daemon_setup(argv[1]);
  }
  //mq_close(mq_fd); mq_unlink(name[0].c_str());mq_unlink(name[1].c_str());mq_unlink(name[2].c_str());mq_unlink(name[3].c_str());mq_unlink(name[4].c_str()); sem_unlink("/KBgoldchase"); shm_unlink("/shm_name"); return 0;
  int index;

  struct sigaction exit_handler;
  exit_handler.sa_handler=clean_up;
  sigemptyset(&exit_handler.sa_mask);
  exit_handler.sa_flags=0;
  sigaction(SIGINT, &exit_handler, NULL);

  struct sigaction sig_action;
  sig_action.sa_handler = handle_interrupt;
  sigemptyset(&sig_action.sa_mask);
  sig_action.sa_flags = 0;
  sig_action.sa_restorer = NULL;
  sigaction(SIGUSR1, &sig_action,NULL);

  struct sigaction action_to_take;
  action_to_take.sa_handler=read_message; 
  sigemptyset(&action_to_take.sa_mask); 
  action_to_take.sa_flags=0;
  sigaction(SIGUSR2, &action_to_take, NULL); 

  struct mq_attr mq_attributes;
  mq_attributes.mq_flags=0;
  mq_attributes.mq_maxmsg=10;
  mq_attributes.mq_msgsize=120;

  int row = 0, col = 0, fool_gold, real_gold;
  int gold_location , fool_loc;
  my_sem = sem_open("/KBgoldchase", O_CREAT|O_EXCL,S_IROTH| S_IWOTH| S_IRGRP| S_IWGRP| S_IRUSR| S_IWUSR,1);
  if(my_sem==SEM_FAILED)
  {
    if(errno!=EEXIST)
    {
      perror("ERROR! Semaphore Not Created");
      exit(1);
    }
  }
  if(my_sem!=SEM_FAILED && argc == 1)//First Player
  {
    DEBUG("Entering 'first player' branch\n");
    fd = shm_open("/shm_name", O_RDWR|O_CREAT,S_IRWXU|S_IRUSR|S_IWUSR|S_IXUSR|S_IRWXG|S_IRGRP|S_IWGRP|S_IXGRP|  			S_IRWXO|S_IROTH|S_IWOTH|S_IXOTH);
    if(fd == -1)
    {
      perror("ERROR! Shared Memory Not Created");
      exit(1);
    }

    string line;
    fstream file;
    file.open("mymap.txt");
    getline(file, line);
    int total_gold = atoi(line.c_str());
    fool_gold = total_gold - 1;
    real_gold = total_gold - fool_gold;	
    while (getline(file, line))
    {
      row++;	
      col = line.size();	
    };
    file.close();
    int er = ftruncate(fd, (row*col+sizeof(GameBoard)));
    if(er == -1)
      perror("ERROR! Ftruncate");
    goldmap = (GameBoard*) mmap(NULL, ((row*col)+sizeof(GameBoard)), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    goldmap->pid[1] = 0;
    goldmap->pid[2] = 0;
    goldmap->pid[3] = 0;
    goldmap->pid[4] = 0;
    goldmap->daemonId = 0;
    if(goldmap == MAP_FAILED)
      perror("ERROR! MMAP");
    if(sem_wait(my_sem) == -1)
      perror("ERROR! SEM_WAIT1");			
    goldmap->rows = row;
    goldmap->cols = col;
    if (sem_post(my_sem) == -1)
      perror("ERROR! SEM_POST1");
    int area = row*col;
    goldmap->map[area+1];
    file.open("mymap.txt");
    char map_ar[area+1];
    char *ptr = map_ar;
    unsigned char *mp = goldmap->map;
    getline(file, line);
    getline(file, line);
    strcpy(map_ar, line.c_str());
    while (getline(file, line))
    {
      strcat(map_ar, line.c_str());	
    };
    while(*ptr!='\0')
    {
      if(*ptr==' ')      *mp=0;
      else if(*ptr=='*') *mp=G_WALL; //A wall
      ++ptr;
      ++mp;
    }
    int r,i, fg = 0, rg = 0;
    for(i = 1; i<= total_gold;)
    {
      r = rand() % (col * row);
      if(goldmap->map[r] == 0)
      {
        if(fg == fool_gold)
        {
          if(sem_wait(my_sem) == -1)
            perror("ERROR! SEM_WAIT2");
          goldmap->map[r] |= G_GOLD;
          gold_location = r;
          i++;
          rg++;
          if (sem_post(my_sem) == -1)
            perror("ERROR! SEM_POST2");
          break;
        }
        else
        {
          if(sem_wait(my_sem) == -1)
            perror("ERROR! SEM_WAIT3");	
          fg++;
          goldmap->map[r] |= G_FOOL; 
          fool_loc = r;
          i++;
          if (sem_post(my_sem) == -1)
            perror("ERROR! SEM_POST3");
        }
      }
    }
    //cout<<"befooree"<<endl;	

    int j = 0;
    while(j < 1)
    {
      r = rand() % (col * row);
      if(goldmap->map[r] == 0)
      {
        //cerr << "about to sem_wait\n";
        if(sem_wait(my_sem) == -1)
          perror("ERROR! SEM_WAIT4");
        index = 0;
        a = index;
        goldmap->pid[0] = getpid();
        current_player = G_PLR0;
        goldmap->map[r] |= current_player;
        //cerr << "about to send SIGHUP\n";
        //kill(goldmap->daemonId, SIGHUP);
        //system("ls /dev/mqueue");
        if((mq_fd=mq_open(name[0].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
                S_IRUSR|S_IWUSR, &mq_attributes))==-1)
        {
          perror("mq_open");
          //exit(1);
        }
        struct sigevent mq_notification_event;
        mq_notification_event.sigev_notify=SIGEV_SIGNAL;
        mq_notification_event.sigev_signo=SIGUSR2;
        mq_notify(mq_fd, &mq_notification_event);
        if (sem_post(my_sem) == -1)
          perror("ERROR! SEM_POST4");
        Cur_loc = r;				
        j++;
      }
    }
  }
  ////////////////////IF ENDS///////////////////////////////////////

  else
  {
    unsigned char gplrbit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4}; 
    DEBUG("Some game process. Starting else for this: if(my_sem!=SEM_FAILED && argc == 1)\n");
    my_sem= sem_open("/KBgoldchase", O_RDWR);
    if(my_sem == SEM_FAILED)
      perror("ERROR! ELSE SEM_OPEN");
    int fd = shm_open("/shm_name", O_RDWR,S_IRWXU|S_IRUSR|S_IWUSR|S_IXUSR|S_IRWXG|S_IRGRP|S_IWGRP|S_IXGRP|  			S_IRWXO|S_IROTH|S_IWOTH|S_IXOTH);
    if(fd == -1)
    {
      perror("ERROR!ELSE Shared Memory Not Created");
      exit(1);
    }
    int s_row, s_col;
    read(fd, &s_row, sizeof(int));
    read(fd, &s_col, sizeof(int)); 
    goldmap = (GameBoard*)mmap(NULL, ((s_row*s_col)+sizeof(GameBoard)), PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    goldmap->rows = s_row;
    goldmap->cols = s_col;
    if(goldmap == MAP_FAILED)
      perror("ERROR! ELSE MMAP");
    else;
    if(sem_wait(my_sem) == -1)
      perror("ERROR! SEM_WAIT5");
    else;
    int iter=0;
    for(int iter=0; iter<5; ++iter)
    {
      if(goldmap->pid[iter] == 0)
      {
        index = iter;	
        current_player=gplrbit[iter];
        goldmap->pid[iter]=getpid();
        kill(goldmap->daemonId, SIGHUP);
        break;
      }
    }
    if(iter==5)
    {
      //cout<<"The Game is full! Try later!"<<endl;
      sem_post(my_sem);
      return 0;
    }

    if (sem_post(my_sem) == -1)
      perror("ERROR! SEM_POST5");
    a = index;
    int j = 0,r;
    while(j < 1)
    {
      r = rand() % (goldmap->cols * goldmap->rows);
      if(goldmap->map[r] == 0)
      {
        sem_wait(my_sem);
        goldmap->pid[index] = getpid();
        goldmap->map[r] |= current_player;
        /*if((mq_fd=mq_open(name[index].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
          S_IRUSR|S_IWUSR, &mq_attributes))==-1)
          {
          perror("mq_open");
          exit(1);
          }
          struct sigevent mq_notification_event;
          mq_notification_event.sigev_notify=SIGEV_SIGNAL;
          mq_notification_event.sigev_signo=SIGUSR2;
          mq_notify(mq_fd, &mq_notification_event);*/
        sem_post(my_sem);	
        Cur_loc = r;				
        j++;
      }
    }
    for(int i=0; i<5; i++)
    {
      if(i == index || goldmap->pid[i] == 0);
      else
      {
        kill(goldmap->pid[i], SIGUSR1);
      }
    }
  }

  if(goldmap->daemonId == 0)
  {
    DEBUG("in if daemon\n");
    if(sem_wait(my_sem)==-1)
      DEBUG("sem_waitE %s\n",strerror(errno));
    server_daemon_setup();
  }

  ////////////////////ELSE ENDS///////////////////////////////////////

  //cerr << "About to create Map\n";
  gmp=new Map(goldmap->map,goldmap->rows, goldmap->cols);
  //cerr << "Created map\n";
  //goldMine=*gmp;
  int a=0;
  int win_flag = 0;	
  int New_loc;
  char input;
  while(1)		
  {
    DEBUG("Main loop for game process. Awaiting keystroke\n");
    int a=gmp->getKey();
    if(a == 'h')//left
    {
      New_loc = Cur_loc -1;
      sem_wait(my_sem);
      if(Cur_loc - goldmap->cols < 0)
      {
        if(win_flag == 1)
        {
          goldmap->map[Cur_loc] &=~ current_player;
          goldmap->pid[index] = 0;
          for(int i = 0 ; i < 5 ;i++)
          {
            if(goldmap->pid[i] != 0 && index != i)
            {
              mqd_t wq_fd;
              if((wq_fd = mq_open(name[i].c_str(),O_WRONLY|O_NONBLOCK,0)) == -1)	
              {
                perror("mq_open");
                exit(1);
              }	
              char msg_text[121];
              string plr = name[index];
              string msg_str = " Won the Gold!";
              msg_str = plr+msg_str;
              memset(msg_text, 0, 121);
              strncpy(msg_text, msg_str.c_str(), 120);
              if(mq_send(wq_fd, msg_text, strlen(msg_text), 0) == -1)
              {
                perror("mq_send");
                exit(1);
              }
              mq_close(wq_fd);
              mq_unlink(name[index].c_str());		
            }	
          }
          if (goldmap->pid[0] == 0 && goldmap->pid[1] == 0 && goldmap->pid[2] == 0 && goldmap->pid[3] == 0 && goldmap->pid[4] == 0)
          {
            if (sem_unlink("/KBgoldchase") == -1)		
              perror("ERROR! SEM_UNLINK");
            if(shm_unlink("/shm_name") == -1)
              perror("ERROR! SHM_UNLINK");
          }
          for(int i=0; i<5; i++)
          {
            if(goldmap->pid[i] == 0);
            else
            {
              kill(goldmap->pid[i], SIGUSR1);
            }
          }
          if (sem_post(my_sem) == -1)
            perror("ERROR! SEM_POST17");
          break;
        }
        else
        {
          goldmap->map[Cur_loc] |= current_player;
        }
      }
      else if(goldmap->map[New_loc] == G_WALL)
      {			
        goldmap->map[Cur_loc] |= current_player;
      }
      else if(goldmap->map[New_loc] & G_GOLD )
      {
        goldmap->map[New_loc] |= current_player;		
        goldmap->map[New_loc] &=~ G_GOLD;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
        gmp->postNotice("You won the gold ! Get Out!");
        win_flag = 1;
      }
      else if(goldmap->map[New_loc] & G_FOOL)
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[New_loc] &=~ G_FOOL;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
        gmp->postNotice("Ouch!Fool! FOOL's GOLD");
      }
      else
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
      }
      for(int i = 0; i < 5 ; i++)
      {
        if(goldmap->pid[i] == 0);
        else
          kill(goldmap->pid[i] , SIGUSR1);
      }
      sem_post(my_sem);
    }
    if(a == 'l')//right
    {
      New_loc = Cur_loc + 1;
      sem_wait(my_sem);
      if(Cur_loc % goldmap->cols < 0)
      {
        DEBUG("I should never enter this code. What am I doing here?\n");
        
        if(win_flag == 1)
        {
          goldmap->map[Cur_loc] &=~ current_player;
          goldmap->pid[index] = 0;
          for(int i = 0 ; i < 5 ;i++)
          {
            if(goldmap->pid[i] != 0 && index != i)
            {
              mqd_t wq_fd;
              if((wq_fd = mq_open(name[i].c_str(),O_WRONLY|O_NONBLOCK,0)) == -1)	
              {
                perror("mq_open");
                exit(1);
              }	
              char msg_text[121];
              string plr = name[index];
              string msg_str = " Won the Gold!";
              msg_str = plr+msg_str;
              memset(msg_text, 0, 121);
              strncpy(msg_text, msg_str.c_str(), 120);
              if(mq_send(wq_fd, msg_text, strlen(msg_text), 0) == -1)
              {
                perror("mq_send");
                exit(1);
              }
              mq_close(wq_fd);
              mq_unlink(name[index].c_str());		
            }	
          }
          if (goldmap->pid[0] == 0 && goldmap->pid[1] == 0 && goldmap->pid[2] == 0 && goldmap->pid[3] == 0 && goldmap->pid[4] == 0)
          {
            if (sem_unlink("/KBgoldchase") == -1)		
              perror("ERROR! SEM_UNLINK");
            if(shm_unlink("/shm_name") == -1)
              perror("ERROR! SHM_UNLINK");
          }
          for(int i=0; i<5; i++)
          {
            if(goldmap->pid[i] == 0);
            else
            {
              kill(goldmap->pid[i], SIGUSR1);
            }
          }	
          if (sem_post(my_sem) == -1)
            perror("ERROR! SEM_POST17");
          break;
        }
        else
        {
          goldmap->map[Cur_loc] |= current_player;
        }
      }
      if(goldmap->map[New_loc] == G_WALL)
      {			
        goldmap->map[Cur_loc] |= current_player;
      }
      else if(goldmap->map[New_loc] & G_GOLD )
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[New_loc] &=~ G_GOLD;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
        gmp->postNotice("You won the gold ! Get Out!");
        win_flag = 1;
      }
      else if(goldmap->map[New_loc] & G_FOOL)
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[New_loc] &=~ G_FOOL;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
        gmp->postNotice("OOPS!! FOOL's GOLD");
      }
      else
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
      }
      for(int i = 0; i < 5 ; i++)
      {
        DEBUG("ckecking kill-SIGUSR1 to i=%d\n",i);
        DEBUG("if(goldmap->pid[i](%d) != 0 && goldmap->pid[i](%d)!=getpid()(%d))\n", goldmap->pid[i], goldmap->pid[i], getpid());
        //if(goldmap->pid[i] != 0 && goldmap->pid[i]!=getpid())
        if(goldmap->pid[i] != 0)
        {
          DEBUG("  KILLING-SIGUSR1 to i=%d\n",i);
          kill(goldmap->pid[i] , SIGUSR1);
        }
      }
      sem_post(my_sem);
    }
    if(a == 'k')//up
    {
      New_loc = Cur_loc - goldmap->cols;
      sem_wait(my_sem);
      if(Cur_loc - goldmap->cols < 0)
      {
        if(win_flag == 1)
        {
          goldmap->map[Cur_loc] &=~ current_player;
          goldmap->pid[index] = 0;
          for(int i = 0 ; i < 5 ;i++)
          {
            if(goldmap->pid[i] != 0 && index != i)
            {
              mqd_t wq_fd;
              if((wq_fd = mq_open(name[i].c_str(),O_WRONLY|O_NONBLOCK,0)) == -1)	
              {
                perror("mq_open");
                exit(1);
              }	
              char msg_text[121];
              string plr = name[index];
              string msg_str = " Won the Gold!";
              msg_str = plr+msg_str;
              memset(msg_text, 0, 121);
              strncpy(msg_text, msg_str.c_str(), 120);
              if(mq_send(wq_fd, msg_text, strlen(msg_text), 0) == -1)
              {
                perror("mq_send");
                exit(1);
              }
              mq_close(wq_fd);
              mq_unlink(name[index].c_str());		
            }	
          }
          if (goldmap->pid[0] == 0 && goldmap->pid[1] == 0 && goldmap->pid[2] == 0 && goldmap->pid[3] == 0 && goldmap->pid[4] == 0)
          {
            if (sem_unlink("/KBgoldchase") == -1)		
              perror("ERROR! SEM_UNLINK");
            if(shm_unlink("/shm_name") == -1)
              perror("ERROR! SHM_UNLINK");
          }
          for(int i=0; i<5; i++)
          {
            if(goldmap->pid[i] == 0);
            else
            {
              kill(goldmap->pid[i], SIGUSR1);
            }
          }	
          if (sem_post(my_sem) == -1)
            perror("ERROR! SEM_POST17");
          break;
        }
        else
        {
          goldmap->map[Cur_loc] |= current_player;
        }
      }
      else if(goldmap->map[New_loc] == G_WALL)
      {			
        goldmap->map[Cur_loc] |= current_player;
      }
      else if(goldmap->map[New_loc] & G_GOLD )
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[New_loc] &=~ G_GOLD;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
        gmp->postNotice("You won the gold ! Get Out!");
        win_flag = 1;
      }
      else if(goldmap->map[New_loc] & G_FOOL)
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[New_loc] &=~ G_FOOL;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
        gmp->postNotice("OOPS!! FOOL's GOLD");
      }
      else
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
      }
      for(int i = 0; i < 5 ; i++)
      {
        if(goldmap->pid[i] == 0);
        else
          kill(goldmap->pid[i] , SIGUSR1);
      }
      sem_post(my_sem);
    }
    if(a == 'j')//down
    {
      New_loc = Cur_loc + goldmap->cols;
      sem_wait(my_sem);
      if((Cur_loc + goldmap->cols) > (goldmap->rows*goldmap->cols))
      {
        if(win_flag == 1)
        {
          goldmap->map[Cur_loc] &=~ current_player;
          goldmap->pid[index] = 0;
          for(int i = 0 ; i < 5 ;i++)
          {
            if(goldmap->pid[i] != 0 && index != i)
            {
              mqd_t wq_fd;
              if((wq_fd = mq_open(name[i].c_str(),O_WRONLY|O_NONBLOCK,0)) == -1)	
              {
                perror("mq_open");
                exit(1);
              }	
              char msg_text[121];
              string plr = name[index];
              string msg_str = " Won the Gold!";
              msg_str = plr+msg_str;
              memset(msg_text, 0, 121);
              strncpy(msg_text, msg_str.c_str(), 120);
              if(mq_send(wq_fd, msg_text, strlen(msg_text), 0) == -1)
              {
                perror("mq_send");
                exit(1);
              }
              mq_close(wq_fd);
              mq_unlink(name[index].c_str());		
            }	
          }
          if (goldmap->pid[0] == 0 && goldmap->pid[1] == 0 && goldmap->pid[2] == 0 && goldmap->pid[3] == 0 && goldmap->pid[4] == 0)
          {
            if (sem_unlink("/KBgoldchase") == -1)		
              perror("ERROR! SEM_UNLINK");
            if(shm_unlink("/shm_name") == -1)
              perror("ERROR! SHM_UNLINK");
          }
          for(int i=0; i<5; i++)
          {
            if(goldmap->pid[i] == 0);
            else
            {
              kill(goldmap->pid[i], SIGUSR1);
            }
          }	
          if (sem_post(my_sem) == -1)
            perror("ERROR! SEM_POST17");
          break;
        }
        else
        {
          goldmap->map[Cur_loc] |= current_player;
        }
      }
      else if(goldmap->map[New_loc] == G_WALL)
      {			
        goldmap->map[Cur_loc] |= current_player;
      }
      else if(goldmap->map[New_loc] & G_GOLD )
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[New_loc] &=~ G_GOLD;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;	
        gmp->postNotice("You won the gold ! Get Out!");
        win_flag = 1;
      }
      else if(goldmap->map[New_loc] & G_FOOL)
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[New_loc] &=~ G_FOOL;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
        gmp->postNotice("OOPS!! FOOL's GOLD");
      }
      else
      {
        goldmap->map[New_loc] |= current_player;
        goldmap->map[Cur_loc] &=~ current_player;
        Cur_loc = New_loc;
      }
      for(int i = 0; i < 5 ; i++)
      {
        if(goldmap->pid[i] == 0);
        else
          kill(goldmap->pid[i] , SIGUSR1);
      }
      sem_post(my_sem);
    }
    else if(a == 'm')
    {
      unsigned char mask = 0;
      for(int i = 0 ; i < 5 ;i++)
      {
        if(goldmap->pid[i] != 0 && index != i)
        {
          switch(i)
          {
            case 0:
              mask |= G_PLR0;
              break;
            case 1:
              mask |= G_PLR1;
              break;
            case 2:
              mask |= G_PLR2;
              break;
            case 3:
              mask |= G_PLR3;
              break;
            case 4:
              mask |= G_PLR4;
              break;
          }
        }
      }
      string p_name;	
      unsigned char player_bit = gmp->getPlayer(mask);//knows all active players.	
      if(player_bit == 0);
      else
      {
        if(player_bit == G_PLR0)
        {p_name = "/Player1";}
        else if(player_bit == G_PLR1)
        {p_name = "/Player2";}
        else if(player_bit == G_PLR2)
        {p_name = "/Player3";}
        else if(player_bit == G_PLR3)
        {p_name = "/Player4";}
        else if(player_bit == G_PLR4)
        {p_name = "/Player5";}
        mqd_t wq_fd;
        if((wq_fd = mq_open(p_name.c_str(),O_WRONLY|O_NONBLOCK,0)) == -1)	
        {
          perror("mq_open");
          exit(1);
        }
        char msg_text[121];
        string plr = name[index]+" Says: ";
        string msg_str;
        msg_str = plr+gmp->getMessage();
        memset(msg_text, 0, 121);
        strncpy(msg_text, msg_str.c_str(), 120);
        if(mq_send(wq_fd, msg_text, strlen(msg_text), 0) == -1)
        {
          perror("mq_send");
          exit(1);
        }
        mq_close(wq_fd);		
      }
    }
    else if(a == 'b')
    {
      string msg_str;
      string plr = name[index]+" Says: ";
      msg_str = plr+gmp->getMessage();
      for(int i = 0 ; i < 5 ;i++)
      {
        if(goldmap->pid[i] != 0 && index != i)
        {
          mqd_t wq_fd;
          if((wq_fd = mq_open(name[i].c_str(),O_WRONLY|O_NONBLOCK,0)) == -1)	
          {
            perror("mq_open");
            exit(1);
          }	
          char msg_text[121];
          memset(msg_text, 0, 121);
          strncpy(msg_text, msg_str.c_str(), 120);
          if(mq_send(wq_fd, msg_text, strlen(msg_text), 0) == -1)
          {
            perror("mq_send");
            exit(1);
          }
          mq_close(wq_fd);	
        }
      }	
    }
    else if(a == 'Q')//This is doing fine no need to change.
    {
	
      if(sem_wait(my_sem) == -1)
        perror("ERROR! SEM_WAIT19");
      DEBUG("daemon ID %d\n", goldmap->daemonId);
      goldmap->pid[index] = 0;
      goldmap->map[Cur_loc] &=~ current_player;
      for(int i = 0; i < 5 ; i++)
      {
        if(goldmap->pid[i] == 0);
        else
        {
          kill(goldmap->pid[i] , SIGUSR1);
        }
      }
      kill(goldmap->daemonId, SIGHUP);
      DEBUG("got back from sighup");
      //mq_close(mq_fd); 
      //mq_unlink(name[index].c_str());
      /* if (goldmap->pid[0] == 0 && goldmap->pid[1] == 0 && goldmap->pid[2] == 0 && goldmap->pid[3] == 0 && goldmap->pid[4] == 0)
         {
         if (sem_unlink("/KBgoldchase") == -1)		
         perror("ERROR! SEM_UNLINK");
         if(shm_unlink("/shm_name") == -1)
         perror("ERROR! SHM_UNLINK");
         }*/
      if (sem_post(my_sem) == -1)
        perror("ERROR! SEM_POST19");
      break;
    }

  }
  delete gmp;
  DEBUG("before return 0");
  return 0;
}

///////////////////////////////////////Signal Handlers////////////////////////////////////////////

void handle_interrupt(int)
{
  gmp->drawMap();
}

void read_message(int)
{
  //set up message queue to receive signal whenever
  //message comes in. This is being done inside of
  //the handler function because it has to be "reset"
  //each time it is used.
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(mq_fd, &mq_notification_event);

  //read a message
  int err;
  char msg[121];
  memset(msg, 0, 121);//set all characters to '\0'
  while((err=mq_receive(mq_fd, msg, 120, NULL))!=-1)
  {
    gmp->postNotice(msg);
    memset(msg, 0, 121);//set all characters to '\0'
  }
  //we exit while-loop when mq_receive returns -1
  //if errno==EAGAIN that is normal: there is no message waiting
  if(errno!=EAGAIN)
  {
    perror("mq_receive");
    exit(1);
  }
}

void clean_up(int)
{
  if(sem_wait(my_sem) == -1)
    DEBUG("sem_waitA=%s\n",strerror(errno));
  goldmap->pid[a] = 0;
  goldmap->map[Cur_loc] &=~ current_player;
  kill(goldmap->daemonId,SIGHUP);
  mq_close(mq_fd); 
  mq_unlink(name[a].c_str());
  for(int i = 0; i < 5 ; i++)
  {
    if(goldmap->pid[i] == 0);
    else
    {
      kill(goldmap->pid[i] , SIGUSR1);
    }
  }
  if (goldmap->pid[0] == 0 && goldmap->pid[1] == 0 && goldmap->pid[2] == 0 && goldmap->pid[3] == 0 && goldmap->pid[4] == 0)
  {
    if (sem_unlink("/KBgoldchase") == -1)		
      perror("ERROR! SEM_UNLINK");
    if(shm_unlink("/shm_name") == -1)
      perror("ERROR! SHM_UNLINK");
  }
  if (sem_post(my_sem) == -1)
    DEBUG("sem_postA=%s\n",strerror(errno));
  delete gmp;
  perror("EXIT 981");	
  exit(1);
}

void SIGHUP_HANDLER(int a)
{
  DEBUG("inside sighup handler\n");
  unsigned char players=0;
  if(goldmap->pid[0]!=0)
    players|=G_PLR0; 
  if(goldmap->pid[1]!=0)
    players|=G_PLR1;
  if(goldmap->pid[2]!=0)
    players|=G_PLR2;
  if(goldmap->pid[3]!=0)
    players|=G_PLR3;
  if(goldmap->pid[4]!=0)
    players|=G_PLR4;

  unsigned char new_player = G_SOCKPLR | players;
  DEBUG("daemon: before if-statement in SIGHUP handler\n");
  if(WRITE(sockfd, &new_player, 1)==-1)
    perror("WRITE in SIGHUP_HANDLER");
  //cout<<"I am in SIGHUP"<<endl;
  if (new_player == G_SOCKPLR)
  {
    DEBUG("daemon: in if-statement in SIGHUP handler\n");
    if (sem_unlink("/KBgoldchase") == -1)		
      perror("ERROR! SEM_UNLINK");
    if(shm_unlink("/shm_name") == -1)
      perror("ERROR! SHM_UNLINK");
    delete gmp;
  }
  DEBUG("daemon: end of SIGHUP handler\n");
}

void SIGUSR1_Handler(int x)
{
  vector<pair<short int,unsigned char>> mapChanges;
  DEBUG("inside SIGUSR1 handler\n");
  if(sem_wait(my_sem)==-1)
    DEBUG("sem_waitC %s\n",strerror(errno));
  assert(goldmap!=NULL);
  assert(localmap!=NULL);
  for(short int i = 0; i < goldmap->rows * goldmap->cols; i++)
  {
    DEBUG(".");
    if(localmap[i] != goldmap->map[i])
    {
      DEBUG("#");
      pair<short int, unsigned char> entry(i,goldmap->map[i]);
      mapChanges.push_back(entry);		
    }	
  }
  DEBUG("\n");
  for(int i = 0; i<mapChanges.size();i++)
  {
    DEBUG("SIGUSR1_Hander, location-c\n");
    localmap[mapChanges[i].first] = mapChanges[i].second;	
  }

  if(mapChanges.size()>0)
  {DEBUG("SIGUSR1_Hander, location-d\n");
    short int size = mapChanges.size();
    int n=0;
    //(1) 1 byte: 0
    unsigned char ch=0;
    WRITE(sockfd, &ch, 1);
    //(2) 1 short: n, # of changed map squares
    WRITE(sockfd, &size, sizeof(size));
    //(3) for 1 to n:
    //   1 short: offset into map memory
    //   1 byte: new byte value
    for(int i = 0; i < size; i++)
    {DEBUG("SIGUSR1_Hander, location-e\n");
      if((n = WRITE(sockfd, &mapChanges[i].first, sizeof(short int))) == -1)
      {
        perror("MapChange1");
        exit(1);
      }DEBUG("SIGUSR1_Hander, location-f\n");
      if((n = WRITE(sockfd, &mapChanges[i].second, sizeof(unsigned char))) == -1)
      {
        perror("MapChange2");
        exit(1);
      }
    }


  }
 sem_post(my_sem);		
  DEBUG("end SIGUSR1 handler\n");
}

void the_while()
{
  //while(1){
  DEBUG("in while 1\n");
  unsigned char first_byte=0;
  unsigned char map_change=0;
  int n;
  assert(sockfd!=-1);
  if((n = READ(sockfd, &first_byte, sizeof(unsigned char))) == -1)
  {
    DEBUG("sockfd=%d, error message=%s\n",sockfd, strerror(errno));
    perror("READ ERROR WHILE()");
  }
  DEBUG("READ was fine. sockfd=%d\n",sockfd);
  if(first_byte & G_SOCKPLR)
  {
    DEBUG("1\n");
    if(first_byte&G_PLR0 && goldmap->pid[0]==0)
    {
      DEBUG("2\n");
      goldmap->pid[0] = goldmap->daemonId;
    }
    if(first_byte&G_PLR1 && goldmap->pid[1]==0)
    {
      DEBUG("player2\n");
      goldmap->pid[1] = goldmap->daemonId;
    }
    if(first_byte&G_PLR2 && goldmap->pid[2]==0)
    {DEBUG("player3\n");
      goldmap->pid[2] = goldmap->daemonId;
    }
    if(first_byte&G_PLR3 && goldmap->pid[3]==0)
    {DEBUG("player4\n");
      goldmap->pid[3] = goldmap->daemonId;
    }
    if(first_byte&G_PLR4 && goldmap->pid[4]==0)
    {DEBUG("player5\n");
      goldmap->pid[4] = goldmap->daemonId;
    }

    if(!(first_byte&G_PLR0) && goldmap->pid[0]!=0)
    {
      DEBUG("player1 off\n");
      goldmap->pid[0] = 0; 
    }
    if(!(first_byte&G_PLR1) && goldmap->pid[1]!=0)
    {
      DEBUG("player2 off\n");
      goldmap->pid[1] = 0; 
    }
    if(!(first_byte&G_PLR2) && goldmap->pid[2]!=0)
    {
	DEBUG("player3 off\n");
      goldmap->pid[2] = 0; 
    }
    if(!(first_byte&G_PLR3) && goldmap->pid[3]!=0)
    {
	DEBUG("player4 off\n");
      goldmap->pid[3] = 0; 
    }
    if(!(first_byte&G_PLR4) && goldmap->pid[4]!=0)
    {
	DEBUG("player5 off\n");
      goldmap->pid[4] = 0; 
    }
    if(first_byte == G_SOCKPLR)
    {
      DEBUG("No players remaining\n");
      if (sem_unlink("/KBgoldchase") == -1)		
        perror("ERROR! SEM_UNLINK in while");
      if(shm_unlink("/shm_name") == -1)
        perror("ERROR! SHM_UNLINK in while");
      delete gmp;
      exit(0);
    }
  }
  else if(first_byte == 0) //(1) 1 byte: 0
  {
    DEBUG("first byte is map change\n");
    if(sem_wait(my_sem)==-1)
      DEBUG("sem_waitB%s\n",strerror(errno));
    short int num_map_change=0, index=0;

    //(2) 1 short: n, # of changed map squares
    if((n = READ(sockfd, &num_map_change, sizeof(short int))) == -1)
    {DEBUG("8\n");
      perror("read num_map_change");
    }
    //(3) for 1 to n:
    //   1 short: offset into map memory
    //   1 byte: new byte value
    for(int i = 0; i < num_map_change; i++)
    {DEBUG("9\n");
      if((n = READ(sockfd, &index, sizeof(short int))) == -1)
      {DEBUG("10\n");
        perror("while index failed");
      }
      if((n = READ(sockfd, &map_change, sizeof(unsigned char))) == -1)
      {DEBUG("11\n");
        perror("while index failed");
      }
      localmap[index]=goldmap->map[index] = map_change;
    }
    DEBUG("location-A\n");
    for(int i = 0; i < 5; i++)
    {
      if(goldmap->pid[i] != 0 && goldmap->pid[i]!=getpid())
      {
        DEBUG("12\n");
        kill(goldmap->pid[i], SIGUSR1);
      }
    }

    sem_post(my_sem);
  }
  //}   //end while(1)
  DEBUG("location-B\n");
}
////////////////////////////////////Daemon Setup CODE///////////////////////////////////////////////


void server_daemon_setup()
{
  //cerr<<"in daemon"<<endl;
  int s_row=goldmap->rows, s_col=goldmap->cols;
  write(debugFD, "-1" , sizeof("-1"));
  if(fork() > 0)
    return;
  if(fork() > 0)
    exit(0);

  if(setsid()==-1)
  {exit(1);}
  for(int i=0; i< sysconf(_SC_OPEN_MAX); ++i)
  {
    if(i != debugFD)
    {	
      close(i);
    }
  }
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  open("/dev/null", O_RDWR); //fd 2

  umask(0);
  chdir("/");
  DEBUG("I am here-01\n");
  write(debugFD, "0" , sizeof("0"));
  //exit(0);
  struct sigaction sighup;
  sighup.sa_handler=SIGHUP_HANDLER;
  sigemptyset(&sighup.sa_mask);
  sigaddset(&sighup.sa_mask, SIGUSR1);
  sighup.sa_flags=0;
  sighup.sa_restorer = NULL;
  sigaction(SIGHUP, &sighup, NULL);

  struct sigaction sigusr1;
  sigusr1.sa_handler=SIGUSR1_Handler;
  sigemptyset(&sigusr1.sa_mask);
  sigaddset(&sighup.sa_mask, SIGHUP);
  sigusr1.sa_restorer = NULL;
  sigusr1.sa_flags=0;
  sigaction(SIGUSR1, &sigusr1, NULL);

  goldmap->daemonId = getpid();	
  sem_post(my_sem);
  int status; //for error checking

  DEBUG("I am here-02\n");
  //change this # between 2000-65k before using
  const char* portno="42424";
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
  hints.ai_flags=AI_PASSIVE; //file in the IP of the server for me
  write(debugFD, "1" , sizeof("1"));
  struct addrinfo *servinfo;
  if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1)
  {
    write(debugFD, "msg getaddr" , sizeof("msg getaddr"));
    exit(1);
  }
  write(debugFD, "2" , sizeof("2"));
  int pre_connect_sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  /*avoid "Address already in use" error*/
  int yes=1;
  if(setsockopt(pre_connect_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
  {
    write(debugFD, "msg setsockopt" , sizeof("msg setsockopt"));
    exit(1);
  }
  write(debugFD, "3" , sizeof("3"));
  //We need to "bind" the socket to the port number so that the kernel
  //can match an incoming packet on a port to the proper process
  if((status=bind(pre_connect_sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    write(debugFD, "msg bind" , sizeof("msg bind"));
    exit(1);
  }
  //when done, release dynamically allocated memory
  freeaddrinfo(servinfo);
  write(debugFD, "4" , sizeof("4"));
  if(listen(pre_connect_sockfd,1)==-1)
  {
    write(debugFD, "msg listen" , sizeof("msg listen"));
    exit(1);
  }
  struct sockaddr_in client_addr;
  socklen_t clientSize=sizeof(client_addr);
  int write_result=-99;
  //if((write_result=accept(pre_connect_sockfd, (struct sockaddr*) &client_addr, &clientSize)) == -1)//
  do{
    DEBUG("msg blocking on accept\n");
    sockfd=accept(pre_connect_sockfd, (struct sockaddr*) &client_addr, &clientSize);
    DEBUG("msg done blocking on accept\n");
  } while(write_result==-1 && errno==EINTR);
  localmap=new unsigned char[goldmap->rows*goldmap->cols];
  for(int i = 0; i < s_row*s_col; i++)
  {
    localmap[i] = goldmap->map[i];
  }	
  if(write_result==-1)
  {
    DEBUG("in new sockfd == -1\n");
    exit(1);
  }
  if(sem_wait(my_sem)==-1)
    DEBUG("sem_waitD%s\n",strerror(errno));
  assert(sockfd!=-1);
  if((write_result=WRITE(sockfd, &s_row, sizeof(int)))==-1)
  {
    DEBUG(" in s_row\n");
    exit(1);
  }
  if((write_result=WRITE(sockfd, &s_col, sizeof(int)))==-1)
  {
    DEBUG(" in s_col\n");
    exit(1);
  }
  if((write_result=WRITE(sockfd, goldmap->map, s_row*s_col))==-1)
  {
    DEBUG(" in map not written\n");
    exit(1);
  }
  DEBUG("Just finished writing rows, cols, & map\n");
  unsigned char new_player = G_SOCKPLR | current_player;
  if((write_result=WRITE(sockfd, &new_player,sizeof(unsigned char)))==-1)
  {
    DEBUG(" player not written\n");
  }
  sem_post(my_sem);
  DEBUG("end server \n");
  //close(write_result);
  while(1)
  {
    the_while();
  }
}


void client_daemon_setup(const char* ip_address)
{
  int s_row=-1, s_col=-1;

  int op = shm_open("/shm_name",O_RDWR , S_IRUSR|S_IWUSR|S_IROTH|S_IWOTH);
  DEBUG("op = %d\n", op);
  if(op == -1) //if no daemon is running on client
  {DEBUG("in client\n");
    if(fork() > 0)
    {
      DEBUG("parent/game about to sleep\n");
      sleep(2);
      DEBUG("parent/game done sleeping. Returning\n");
      return;
    }
    if(fork() > 0)
    {	
      exit(0);
    }    	
    wait(NULL); //clean up zombie
    if(setsid()==-1)
    {exit(1);}
    for(int i=0; i< sysconf(_SC_OPEN_MAX); ++i)
    {
      if(i != debugFD)
      {	
        close(i);
      }
    }	
    open("/dev/null", O_RDWR); //fd 0
    open("/dev/null", O_RDWR); //fd 1
    open("/dev/null", O_RDWR); //fd 2
    umask(0);
    chdir("/");
    DEBUG("daemon created\n");
    struct sigaction sighup;
    sighup.sa_handler=SIGHUP_HANDLER;
    sigemptyset(&sighup.sa_mask);
    sigaddset(&sighup.sa_mask, SIGUSR1);
    sighup.sa_flags=0;
    sighup.sa_restorer = NULL;
    sigaction(SIGHUP, &sighup, NULL);

    struct sigaction sigusr1;
    sigusr1.sa_handler=SIGUSR1_Handler;
    sigemptyset(&sigusr1.sa_mask);
    sigaddset(&sighup.sa_mask, SIGHUP);
    sigusr1.sa_restorer = NULL;
    sigusr1.sa_flags=0;
    sigaction(SIGUSR1, &sigusr1, NULL);
    int status; //for error checking

    const char* portno="42424";
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints)); //zero out everything in structure
    hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
    hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
    DEBUG("before status\n");
    struct addrinfo *servinfo;
    if((status=getaddrinfo(ip_address, portno, &hints, &servinfo))==-1)
    {
      write(debugFD,"status error\n",sizeof("status error"));
      exit(1);
    }
    sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    //the code goes till here it writes"Connect ER" to the pipe. 
    DEBUG("before connect\n");
    if((status=connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
    {		
      DEBUG("Connect ER\n");
      exit(1);
    }
    freeaddrinfo(servinfo);
    DEBUG("before int n\n");
    int n=-1;
    if((n = READ(sockfd, &s_row, sizeof(s_row))) == -1)
    {
      DEBUG("R NOT R");
      exit(1);
    }
    if((n = READ(sockfd, &s_col, sizeof(s_col))) == -1)
    {
      DEBUG("COL NOT R");
      exit(1);
    }
    DEBUG("rows=%d cols=%d\n", s_row, s_col);
    localmap = new unsigned char[s_row*s_col];
    if((n = READ(sockfd, localmap, s_row*s_col)) == -1)
    {
      DEBUG("bad read of map!\n");
      exit(1);
    }
    my_sem = sem_open("/KBgoldchase", O_CREAT|O_EXCL,S_IROTH| S_IWOTH| S_IRGRP| S_IWGRP| S_IRUSR| S_IWUSR,1);
    if(my_sem == SEM_FAILED)
    {
      DEBUG("CLIENT sem_open ERROR\n");
      DEBUG("%s\n",strerror(errno));
    }
    int fd = shm_open("/shm_name", O_RDWR|O_CREAT,S_IRWXU|S_IRUSR|S_IWUSR|S_IXUSR|S_IRWXG|S_IRGRP|S_IWGRP|S_IXGRP|  			S_IRWXO|S_IROTH|S_IWOTH|S_IXOTH);
    if(fd == -1)
    {
      DEBUG("CLIENT FD ERROR\n");
    }
    int er = ftruncate(fd, (s_row*s_col+sizeof(GameBoard)));
    if(er == -1)
      DEBUG("%s\n",strerror(errno));
    goldmap = (GameBoard*)mmap(NULL, ((s_row*s_col)+sizeof(GameBoard)), PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(goldmap==MAP_FAILED)
      DEBUG("Map failed-client: %s\n",strerror(errno));
    goldmap->rows = s_row;
    goldmap->cols = s_col;
    goldmap->daemonId = getpid();	
    int map_size = s_row*s_col;	
    for(int i = 0; i < map_size; i++)
    {
      goldmap->map[i] = localmap[i]; 
    }
    DEBUG("client end\n");
    while(1)
    {
      the_while();
      DEBUG("over here\n");
    }
    DEBUG("out of while\n");
  }

}
