/*************************************************************
 * CLIENTE liga ao servidor (definido em argv[1]) no porto especificado  
 * (em argv[2]), escrevendo a palavra predefinida (em argv[3]).
 * USO: >cliente <enderecoServidor>  <porto>  <Palavra>
 ./client 127.0.0.1 9000
 *************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <errno.h>

#define BUF_SIZE 2048

//declaracao de funcoes
void erro(char *msg);
void miniclient(char *argmc,int porto, int num);

//estrutura da memoria partilhada
typedef struct shm{
  pid_t child_pid[8];
  int elimina;
  int fd2[7];
}shm;
int shm_id;

shm* shared_mem; 

//funcao que elimina os processos das subscricoes e fecha os sockets
void quit_sub()
{
  pid_t self = getpid();
  if(shared_mem->child_pid[7]==self)
  {
    close(shared_mem->fd2[shared_mem->elimina]);
    kill(shared_mem->child_pid[shared_mem->elimina],SIGKILL);
  }

}
int main(int argc, char *argv[]) {
  char endServer[100];
  char buffer[BUF_SIZE];
  int nread = 0;
  char temp[100]; //variavel usada mais a frente para guardar informacao lida do buffer
  int x = 1; //flag do primeiro while
  int y = 1; //flag do segundo while
  int subscricao[7]={0,0,0,0,0,0,0}; //array de flags para verificar a existencia de subscricoes
  int fd;
  int novo_porto=(short) atoi(argv[2])+1;
  int flag=0;   //flag para exibicao do menu
  struct sockaddr_in addr;
  struct hostent *hostPtr;

  //Criacao da shared memory
  shm_id=shmget(IPC_PRIVATE,sizeof(shm),IPC_CREAT|0766);
  if(shm_id==-1)
  {
      printf("A memória partilhada não foi criada");
      exit(0);
  }
  shared_mem=(shm*)shmat(shm_id,NULL,0);
  
  if (argc != 3) {
    printf("client <host> <port>\n");
    exit(-1);
  }

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    erro("Nao consegui obter endereço");

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short) atoi(argv[2]));

  if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
    erro("socket");
  if( connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
    erro("Connect");

  shared_mem->child_pid[7]=getpid(); //guardar o pid do processo pai para mais tarde conseguir destruir os processos filhos provenientes das subscricoes
  nread = read(fd,buffer,BUF_SIZE-1);
  buffer[nread] = '\0';
  printf("%s\n", buffer);

  //escrita do ID
  scanf("%s", buffer);
  write(fd, buffer, strlen(buffer)+1);

  while(x>0)
  {
    //Read do menu ou do id nao pertencer à base de dados
    nread = read(fd,buffer,BUF_SIZE-1);
    buffer[nread] = '\0';
    printf("%s\n", buffer);
    if(strcmp(buffer,"O teu ID não pertence á base de dados, tenta de novo!\n")==0)
    {
      //pede novamente um id
      printf("Introduz o teu client ID:");
      scanf("%s", buffer);
      write(fd, buffer, strlen(buffer)+1);
    }
    else
      x=-1; //sai do while
  }

  while(y>0)
  {
    if(flag==1) //caso ja tenha lido o menu do while anterior para nao imprimir duas vezes quando entra neste while pela primeira vez
    {
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);
    }
    flag=1;
    printf("Seleciona uma opção:");
    scanf("%s", buffer);
    write(fd, buffer, strlen(buffer)+1);
    //------------------------------------//
    //OPCAO DE VER AS INFORMACOES PESSOAIS//
    //------------------------------------//
    if(strcmp(buffer,"1")==0)
    {
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);
    }
    //-------------------------------------//
    //OPCOES DE VER AS INFORMACOES DO GRUPO//
    //-------------------------------------//
    else if(strcmp(buffer,"2")==0)
    {
      //todas informacoes do grupo
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);
    }
    else if(strcmp(buffer,"3")==0)
    {
      //duracao de chamadas
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);
    }
    else if(strcmp(buffer,"4")==0)
    {
      //chamadas recebidas
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);

    }
    else if(strcmp(buffer,"5")==0)
    {
      //chamadas perdidas
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);

    }
    else if(strcmp(buffer,"6")==0)
    {
      //chamadas feitas
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);

    }
    else if(strcmp(buffer,"7")==0)
    {
      //sms's recebidos
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);

    }
    else if(strcmp(buffer,"8")==0)
    {
      //sms's mandados
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);

    }
    //----------------------//
    //OPCOES DAS SUBSCRICOES//
    //----------------------//
    else if(strcmp(buffer,"9")==0 || strcmp(buffer,"10")==0 || strcmp(buffer,"11")==0 || strcmp(buffer,"12")==0 || strcmp(buffer,"13")==0 || strcmp(buffer,"14")==0 || strcmp(buffer,"15")==0)
    {
      if(subscricao[0]==0 && strcmp(buffer,"9")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Subscricao geral
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        novo_porto=atoi(buffer); //le a mensagem do server que diz o porto ao qual tem que se ligar para a subscricao
        sleep(1); //esperar que a cricao do processo miniserver seja feita primeiro para nao dar erro
        if((fork())==0)
        {
          miniclient(argv[1],novo_porto, 0);
          exit(0);
        }
        subscricao[0]=1;//registra que tal subscricao foi realizada
      }
      else if(subscricao[0]==1 && strcmp(buffer,"9")==0) //caso tal subscricao ja exista, vai ler a mensagem de aviso do servidor
      {
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
      }
      else if(subscricao[1]==0 && strcmp(buffer,"10")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Subscricao duracao das chamadas
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
        nread = read(fd,buffer,BUF_SIZE-1);
        novo_porto=atoi(buffer); //le a mensagem do server que diz o porto ao qual tem que se ligar para a subscricao
        sleep(1); //esperar que a cricao do processo miniserver seja feita primeiro para nao dar erro
        if(fork()==0)
        {
          miniclient(argv[1],novo_porto, 1);
          exit(0);
        }
        subscricao[1]=1; //registra que tal subscricao foi realizada
      }
      else if(subscricao[1]==1 && strcmp(buffer,"10")==0) //caso tal subscricao ja exista, vai ler a mensagem de aviso do servidor
      {
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
      }
      else if(subscricao[2]==0 && strcmp(buffer,"11")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Subscricao chamadas recebidas
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        novo_porto=atoi(buffer); //le a mensagem do server que diz o porto ao qual tem que se ligar para a subscricao
        sleep(1); //esperar que a cricao do processo miniserver seja feita primeiro para nao dar erro
        if(fork()==0)
        {
          miniclient(argv[1],novo_porto, 2);
          exit(0);
        }
        subscricao[2]=1; //registra que tal subscricao foi realizada
      }
      else if(subscricao[2]==1 && strcmp(buffer,"11")==0) //caso tal subscricao ja exista, vai ler a mensagem de aviso do servidor
      {
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
      }
      else if(subscricao[3]==0 && strcmp(buffer,"12")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Subscricao chamadas perdidas
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        novo_porto=atoi(buffer); //le a mensagem do server que diz o porto ao qual tem que se ligar para a subscricao
        sleep(1); //esperar que a cricao do processo miniserver seja feita primeiro para nao dar erro
        if(fork()==0)
        {
          miniclient(argv[1],novo_porto, 3);
          exit(0);
        }
        subscricao[3]=1; //registra que tal subscricao foi realizada
      }
      else if(subscricao[3]==1 && strcmp(buffer,"12")==0) //caso tal subscricao ja exista, vai ler a mensagem de aviso do servidor
      {
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
      }
      else if(subscricao[4]==0 && strcmp(buffer,"13")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Subscricao chamadas feitas
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        novo_porto=atoi(buffer); //le a mensagem do server que diz o porto ao qual tem que se ligar para a subscricao
        sleep(1); //esperar que a cricao do processo miniserver seja feita primeiro para nao dar erro
        if(fork()==0)
        {
          miniclient(argv[1],novo_porto, 4);
          exit(0);
        }
        subscricao[4]=1; //registra que tal subscricao foi realizada
      }
      else if(subscricao[4]==1 && strcmp(buffer,"13")==0) //caso tal subscricao ja exista, vai ler a mensagem de aviso do servidor
      {
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
      }
      else if(subscricao[5]==0 && strcmp(buffer,"14")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Subscricao sms's recebidos
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        novo_porto=atoi(buffer); //le a mensagem do server que diz o porto ao qual tem que se ligar para a subscricao
        sleep(1); //esperar que a cricao do processo miniserver seja feita primeiro para nao dar erro
        if(fork()==0)
        {
          miniclient(argv[1],novo_porto, 5);
          exit(0);
        }
        subscricao[5]=1; //registra que tal subscricao foi realizada
      }
      else if(subscricao[5]==1 && strcmp(buffer,"14")==0) //caso tal subscricao ja exista, vai ler a mensagem de aviso do servidor
      {
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
      }
      else if(subscricao[6]==0 && strcmp(buffer,"15")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Subscricao sms's mandados
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        novo_porto=atoi(buffer); //le a mensagem do server que diz o porto ao qual tem que se ligar para a subscricao
        sleep(1); //esperar que a cricao do processo miniserver seja feita primeiro para nao dar erro
        if(fork()==0)
        {
          miniclient(argv[1],novo_porto, 6);
          exit(0);
        }
        subscricao[6]=1; //registra que tal subscricao foi realizada
      }
      else if(subscricao[6]==1 && strcmp(buffer,"15")==0) //caso tal subscricao ja exista, vai ler a mensagem de aviso do servidor
      {
        nread = read(fd,buffer,BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s\n", buffer);
      }
    }
    //--------------------//
    //CANCELAR SUBSCRICOES//
    //--------------------//
    else if(strcmp(buffer,"16")==0 || strcmp(buffer,"17")==0 || strcmp(buffer,"18")==0 || strcmp(buffer,"19")==0 || strcmp(buffer,"20")==0 || strcmp(buffer,"21")==0 || strcmp(buffer,"22")==0)
    {
      strcpy(temp,buffer); //guardar na variavel temp a opcao que esta no buffer para poder comparar
      nread = read(fd,buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);
      if(subscricao[0]==1 && strcmp(temp,"16")==0)//verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Cancelar subscricao geral
        subscricao[0]=0; //atualiza a flag de subscricao
        shared_mem->elimina=0; //indica a posicao dos array deste tipo de subscricao
        quit_sub(); //funcao que elimina o processo de subscricao e o respetivo socket
      }
      else if(subscricao[1]==1 && strcmp(temp,"17")==0)//verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Cancelar subscricao de duracao de chamadas
        subscricao[1]=0;
        shared_mem->elimina=1;
        quit_sub();
      }
      else if(subscricao[2]==1 && strcmp(temp,"18")==0)//verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Cancelar subscricao de chamadas recebidas
        subscricao[2]=0;
        shared_mem->elimina=2;
        quit_sub();
      }
      else if(subscricao[3]==1 && strcmp(temp,"19")==0)//verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Cancelar subscricao de chamadas perdidas
        subscricao[3]=0;
        shared_mem->elimina=3;
        quit_sub();
      }
      else if(subscricao[4]==1 && strcmp(temp,"20")==0)//verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Cancelar subscricao de chamadas feitas
        subscricao[4]=0;
        shared_mem->elimina=4;
        quit_sub();
      }
      else if(subscricao[5]==1 && strcmp(temp,"21")==0)//verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Cancelar subscricao de SMS's recebidos
        subscricao[5]=0;
        shared_mem->elimina=5;
        quit_sub();
      }
      else if(subscricao[6]==1 && strcmp(temp,"22")==0)//verificar se tal subscricao ja existe e se foi esta a opcao escolhida
      {
        //Cancelar subscricao de SMS's mandados
        subscricao[6]=0;
        shared_mem->elimina=6;
        quit_sub();
      }
    }
    //EXIT
    else if(strcmp(buffer,"23")==0)
    {
      y=-1;
      for (int i = 0; i < 7; i++)
      {
        if(subscricao[i]==1) //se existir respectiva subscricao
        {
          shared_mem->elimina=i;
          quit_sub(); //destruir todos os processos de subscricao existentes e os respetivos sockets
        }
      }
      shmctl(shm_id,IPC_RMID, NULL); //destroi shared memory
    }
  }
  close(fd);
  exit(0);
}

//FUNCAO DE DETECAO DE ERROS
void erro(char *msg)
{
  printf("Erro: %s\n", msg);
  exit(-1);
}

//FUNCAO DOS PROCESSOS DE SUBSCRICAO DO CLIENT
void miniclient(char *argmc,int porto, int num) //sendo num a referência dos arrays de acordo com a opção selecionada
{
  char endServer[100];
  char buffer[BUF_SIZE];
  int nread = 0;

  struct sockaddr_in addr;
  struct hostent *hostPtr;

  strcpy(endServer, argmc);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    erro("Nao consegui obter endereço");

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short)porto);

  if((shared_mem->fd2[num] = socket(AF_INET,SOCK_STREAM,0)) == -1)
    erro("socket");
  if( connect(shared_mem->fd2[num],(struct sockaddr *)&addr,sizeof (addr)) < 0)
    perror("Connect");
  //Guardar os pid's na shared memory, para mais tarde conseguir eliminar
  if(num==0)    //geral
    shared_mem->child_pid[0]=getpid();
  else if(num==1)   //duracao das chamadas
    shared_mem->child_pid[1]=getpid();
  else if(num==2)   //chamadas recebidas
    shared_mem->child_pid[2]=getpid();
  else if(num==3)   //chamadas perdidas
    shared_mem->child_pid[3]=getpid();
  else if(num==4) //chamadas feitas
    shared_mem->child_pid[4]=getpid();
  else if(num==5)   //sms's mandados
    shared_mem->child_pid[5]=getpid();
  else if(num==6)   //sms's recebidos
    shared_mem->child_pid[6]=getpid();

  //No client os processos de subscricao vao estar sempre a ler o que vem do server
  while(1)
  {
      nread = read(shared_mem->fd2[num],buffer,BUF_SIZE-1);
      buffer[nread] = '\0';
      printf("%s\n", buffer);
  }
}
