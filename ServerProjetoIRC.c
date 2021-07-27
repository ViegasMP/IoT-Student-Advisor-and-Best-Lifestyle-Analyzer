/*******************************************************************
 * SERVIDOR no porto 9000, à escuta de novos clientes.  Quando surjem
 * novos clientes os dados por eles enviados são lidos e descarregados no ecran.
 *******************************************************************/
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <errno.h>

#define SERVER_PORT     9000
#define BUF_SIZE	2048

//declaracao de funcoes
void process_client(int client_fd,int novo_porto);
void erro(char *msg);
void miniserver(int novo_porto,char *option, int num);

//variaveis globais
int novo_porto=9000;
pid_t child_pid[7];

//Necessitou se de criar shared memory para poder destruir os processos para cancelar as susbscricoes
typedef struct shm{
  pid_t child_pid[8]; //array que guarda os pid's dos processos para os conseguir matar para cancelar as subscricoes
  int elimina; //flag que identifica a subscricao a eliminar
  int client2[7]; //array que guarda os client para cada subscricao feita
}shm;
int shm_id;

shm* shared_mem; 

//funcao que elimina os processos das subscricoes e fecha os sockets
void quit_sub()
{
	pid_t self = getpid();
	if(shared_mem->child_pid[7]==self)
	{
	    close(shared_mem->client2[shared_mem->elimina]);
	    kill(shared_mem->child_pid[shared_mem->elimina],SIGKILL);
	}
}

//---------funcoes disponibilizadas no github------------
struct string 
{
	char *ptr;
	size_t len;
};

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
	size_t new_len = s->len + size*nmemb;
	s->ptr = realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL) 
	{
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr + s->len, ptr, size*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size*nmemb;
}

void init_string(struct string *s) 
{
	s->len = 0;
	s->ptr = malloc(s->len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

struct json_object *get_student_data()
{
	struct string s;
	struct json_object *jobj;

	//Intialize the CURL request
	CURL *hnd = curl_easy_init();

	//Initilize the char array (string)
	init_string(&s);

	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
	//To run on department network uncomment this request and comment the other
	//curl_easy_setopt(hnd, CURLOPT_URL, "http://10.3.4.75:9014/v2/entities?options=keyValues&type=student&attrs=activity,calls_duration,calls_made,calls_missed,calls_received,department,location,sms_received,sms_sent&limit=1000");
    //To run from outside
	curl_easy_setopt(hnd, CURLOPT_URL, "http://socialiteorion2.dei.uc.pt:9014/v2/entities?options=keyValues&type=student&attrs=activity,calls_duration,calls_made,calls_missed,calls_received,department,location,sms_received,sms_sent&limit=1000");
	
	//Add headers
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "cache-control: no-cache");
	headers = curl_slist_append(headers, "fiware-servicepath: /");
	headers = curl_slist_append(headers, "fiware-service: socialite");

	//Set some options
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writefunc); //Give the write function here
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &s); //Give the char array address here

	//Perform the request
	CURLcode ret = curl_easy_perform(hnd);
	if (ret != CURLE_OK)
	{
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(ret));

		/*jobj will return empty object*/
		jobj = json_tokener_parse(s.ptr);

		/* always cleanup */
		curl_easy_cleanup(hnd);
		return jobj;
	}

	else if (CURLE_OK == ret) 
	{
		jobj = json_tokener_parse(s.ptr);
		free(s.ptr);

		/* always cleanup */
		curl_easy_cleanup(hnd);
		return jobj;
	}

}
//-----------------------------------

//Funcao que verifica se a pessoa existe na base de dados(se sim, return 1; se nao, return 0)
int existe_pessoa(char *id)
{
	struct json_object *jobj_array, *jobj_obj;
    struct json_object *jobj_object_id;
    int arraylen = 0;
    int i;

	jobj_array = get_student_data();
    arraylen = json_object_array_length(jobj_array);
    for (i = 0; i < arraylen; i++) {
		//get the i-th object in jobj_array
		jobj_obj = json_object_array_get_idx(jobj_array, i);
		//get the name attribute in the i-th object
		jobj_object_id = json_object_object_get(jobj_obj, "id");
		if(strcmp(json_object_get_string(jobj_object_id),id)==0)
			return 1;
	}
	return 0;
}

int main() {
	int fd, client;
	struct sockaddr_in addr, client_addr;
	int client_addr_size;

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(SERVER_PORT);

	if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) //Crias o socket
		erro("na funcao socket");
	if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0) //Liga o socket
		erro("na funcao bind");
	if( listen(fd, 5) < 0) //Até 5 utilizadores em espera
		erro("na funcao listen");

  	while (1) {
    	client_addr_size = sizeof(client_addr);
    	client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
    	if (client > 0) {
    		novo_porto=novo_porto+20; //portos reservados para as subscricoes de cada cliente
    		if (fork() == 0) {
    			close(fd);
        		process_client(client,novo_porto);
        		exit(0);
      		}
    		close(client);
    	}
  	}
  	return 0;
}

void process_client(int client_fd,int novo_porto)
{
	//Criacao da shared memory
	shm_id=shmget(IPC_PRIVATE,sizeof(shm),IPC_CREAT|0766);
	if(shm_id==-1)
    {
        printf("A memória partilhada não foi criada");
        exit(0);
    }
    shared_mem=(shm*)shmat(shm_id,NULL,0);

	shared_mem->child_pid[7]=getpid(); //guarda o pid deste processo na setima posiçao do array, sera preciso para matar os processos filhos
	int nread = 0;
	int x = 1; //variavel usada para controlar o while
	int y =1; //variavel usada para controlar o while
	char option[100];//guarda opção selecionada
	int subscrito[7]; //variavel para verificar se o cliente encontra-se subscrito(sim = 1, nao = 0)
	//0-geral 1-duracao de chamadas 2-chamadas recebidas 3-chamadas perdidas 4-chamadas feitas  5-sms's recebidos 6-sms's mandados
	char buffer[BUF_SIZE];
	int arraylen = 0; //variavel que guarda o tamanho do array da base de dados fornecida
  	int i;
  	char id_do_client[100]; //variavel que server de comparacao para a existencia do client_ID
  	double soma_call_duration = 0, soma_call_made = 0, soma_call_missed = 0, soma_call_received = 0, soma_sms_sent = 0, soma_sms_received = 0;

	struct json_object *jobj_array, *jobj_obj;
  	struct json_object *jobj_object_id, *jobj_object_type, *jobj_object_activity, *jobj_object_location, *jobj_object_callsduration, *jobj_object_callsmade, *jobj_object_callsmissed, *jobj_object_callsreceived, *jobj_object_department, *jobj_object_smsreceived, *jobj_object_smssent;

  	for(int i=0;i<7;i++)
  		subscrito[i]=0;  	  	  	  	  	

  	//INICIO DA COMUNICACAO COM O CLIENT//

 	write(client_fd,"Bem vindo!\nIntroduz o teu client ID:",BUF_SIZE-1);
	while(x>0)
	{
		nread = read(client_fd, buffer, BUF_SIZE-1);
		buffer[nread] = '\0';
	  	fflush(stdout);
	  	strcpy(id_do_client,buffer);
		if(existe_pessoa(buffer)==1) //caso o id exista na base de dados
		{
			while(y>0)
			{
                //variaveis utilizadas para o calculo de medias
				soma_call_duration=0;
				soma_call_made=0;
				soma_call_missed=0;
				soma_call_received=0;
				soma_sms_received=0;
				soma_sms_sent=0;
                
                //menu principal
				write(client_fd,"-----------------------MENU----------------------\n1-Ver os meus dados pessoais\n\n2-Ver os dados do grupo\n3-Ver os dados da duracao das chamadas do grupo\n4-Ver os dados das chamadas recebidas do grupo\n5-Ver os dados das chamadas perdidas do grupo\n6-Ver os dados das chamadas feitas do grupo\n7-Ver os dados dos SMS's mandados do grupo\n8-Ver os dados de SMS's recebidos do grupo\n\n9-Subscrever a todas atualizacoes\n10-Subscrever as atualizacoes de duracao de chamada\n11-Subscrever as atualizacoes de chamadas recebidas\n12-Subscrever as atualizacoes de chamadas perdidas\n13-Subscrever as atualizacoes de chamadas feitas\n14-Subscrever as atualizacoes de SMS's recebidos\n15-Subscrever as atualizacoes de SMS's mandados\n\n16-Cancelar subscricao a todas atualizacoes\n17-Cancelar subscricao as atualizacoes de duracao de chamada\n18-Cancelar subscricao as atualizacoes de chamadas recebidas\n19-Cancelar subscricao as atualizacoes de chamadas perdidas\n20-Cancelar subscricao as atualizacoes de chamadas feitas\n21-Cancelar subscricao as atualizacoes de SMS's recebidos\n22-Cancelar subscricao as atualizacoes de SMS's mandados\n\n23-Sair",BUF_SIZE-1);
				nread = read(client_fd, buffer, BUF_SIZE-1);
				buffer[nread] = '\0';
	            strcpy(option,buffer);//guarda o buffer com a opcao na variavel option
                
	            //INFORMACOES PESSOAIS
	  			if(strcmp(option,"1")==0)
	  			{
	  				printf("Um cliente pediu as suas informacoes\n");
	  				jobj_array = get_student_data();
					arraylen = json_object_array_length(jobj_array);
					for (i = 0; i < arraylen; i++)
					{
						//get the i-th object in jobj_array
						jobj_obj = json_object_array_get_idx(jobj_array, i);
						//get the name attribute in the i-th object
						jobj_object_id = json_object_object_get(jobj_obj, "id");
						if(strcmp(json_object_get_string(jobj_object_id),id_do_client)==0)  //assegura que disponibiliza apenas a informação do proprio cliente e mais ninguem
						{
							jobj_object_id = json_object_object_get(jobj_obj, "id");
							jobj_object_type = json_object_object_get(jobj_obj, "type");
							jobj_object_activity = json_object_object_get(jobj_obj, "activity");
							jobj_object_location = json_object_object_get(jobj_obj, "location");
							jobj_object_callsduration = json_object_object_get(jobj_obj, "calls_duration");
							jobj_object_callsmade = json_object_object_get(jobj_obj, "calls_made");
							jobj_object_callsmissed = json_object_object_get(jobj_obj, "calls_missed");
							jobj_object_callsreceived= json_object_object_get(jobj_obj, "calls_received");
							jobj_object_department = json_object_object_get(jobj_obj, "department");
							jobj_object_smsreceived = json_object_object_get(jobj_obj, "sms_received");
							jobj_object_smssent = json_object_object_get(jobj_obj, "sms_sent");
							//manda ao cliente as informacoes pessoais
                            buffer[0] = '\0';
							sprintf(buffer,"\n-----------------------DADOS----------------------\nId = %s\nType = %s\nActivity = %s\nLocation = %s\nCalls duration(s) = %s\nCalls made = %s\nCalls missed = %s\nCalls received = %s\nDepartment = %s\nSms received = %s\nSms sent = %s\n",json_object_get_string(jobj_object_id), json_object_get_string(jobj_object_type), json_object_get_string(jobj_object_activity), json_object_get_string(jobj_object_location), json_object_get_string(jobj_object_callsduration), json_object_get_string(jobj_object_callsmade), json_object_get_string(jobj_object_callsmissed), json_object_get_string(jobj_object_callsreceived), json_object_get_string(jobj_object_department), json_object_get_string(jobj_object_smsreceived), json_object_get_string(jobj_object_smssent));
					        write(client_fd,buffer,BUF_SIZE-1);
						}
	        		}
  				}
  				//INFORMACOES GERAIS DO GRUPO
  				else if(strcmp(option,"2")==0)
  				{
  					printf("Um cliente pediu as informacoes do grupo\n");
  					jobj_array = get_student_data();
			    	arraylen = json_object_array_length(jobj_array);
  					for (i = 0; i < arraylen; i++)
                    {
  						jobj_obj = json_object_array_get_idx(jobj_array, i);
						jobj_object_callsduration = json_object_object_get(jobj_obj, "calls_duration");
						jobj_object_callsmade = json_object_object_get(jobj_obj, "calls_made");
						jobj_object_callsmissed = json_object_object_get(jobj_obj, "calls_missed");
						jobj_object_callsreceived= json_object_object_get(jobj_obj, "calls_received");
						jobj_object_smsreceived = json_object_object_get(jobj_obj, "sms_received");
						jobj_object_smssent = json_object_object_get(jobj_obj, "sms_sent");

                        if(jobj_object_callsduration!=NULL && jobj_object_callsmade!=NULL && jobj_object_callsmissed!=NULL && jobj_object_callsreceived!=NULL && jobj_object_smsreceived!=NULL && jobj_object_smssent!=NULL) //soma apenas valores que nao sejam null
						{
                            //soma os totais de cada tipo informacao
							soma_call_duration += atoi(json_object_get_string(jobj_object_callsduration));
							soma_call_made += atoi(json_object_get_string(jobj_object_callsmade));
							soma_call_missed += atoi(json_object_get_string(jobj_object_callsmissed));
							soma_call_received += atoi(json_object_get_string(jobj_object_callsreceived));
							soma_sms_received += atoi(json_object_get_string(jobj_object_smsreceived));
							soma_sms_sent += atoi(json_object_get_string(jobj_object_smssent));
						}
				  	}
                    //manda a media de cada dado ao cliente
				  	buffer[0] = '\0';
				  	sprintf(buffer,"\n----------------ESTATISTICAS DO GRUPO---------------\nCalls duration(s) = %.2lf\nCalls made = %.2lf\nCalls missed = %.2lf\nCalls received = %.2lf\nSms received = %.2lf\nSms sent = %.2lf\n",soma_call_duration/arraylen, soma_call_made/arraylen, soma_call_missed/arraylen, soma_call_received/arraylen, soma_sms_received/arraylen, soma_sms_sent/arraylen);
				  	write(client_fd,buffer,BUF_SIZE-1);
  				}
  				//INFORMACAO DA DURACAO DAS CHAMADAS DO GRUPO
        		else if(strcmp(option,"3")==0)
        		{
          			printf("Um cliente pediu as informacoes da duracao das chamadas do grupo\n");
          			jobj_array = get_student_data();
          			arraylen = json_object_array_length(jobj_array);
          			for (i = 0; i < arraylen; i++)
          			{
            			jobj_obj = json_object_array_get_idx(jobj_array, i);
            			jobj_object_callsduration = json_object_object_get(jobj_obj, "calls_duration");
            			if(jobj_object_callsduration!=NULL)
            			{
              				soma_call_duration += atoi(json_object_get_string(jobj_object_callsduration));
            			}
          			}
          			buffer[0] = '\0';
          			sprintf(buffer,"\n------------ESTATISTICAS DA DURACAO DAS CHAMADAS DO GRUPO-----------\nCalls duration(s) = %.2lf\n",soma_call_duration/arraylen);
          			write(client_fd,buffer,BUF_SIZE-1);

        		}
        		//INFORMACAO DAS CHAMADAS FEITAS DO GRUPO 
        		else if(strcmp(option,"4")==0)
        		{
          			printf("Um cliente pediu as informacoes das chamadas feitas do grupo\n");
          			jobj_array = get_student_data();
          			arraylen = json_object_array_length(jobj_array);
          			for (i = 0; i < arraylen; i++)
          			{
            			jobj_obj = json_object_array_get_idx(jobj_array, i);
            			jobj_object_callsmade = json_object_object_get(jobj_obj, "calls_made");

            			if(jobj_object_callsmade!=NULL)
            			{
              				soma_call_made += atoi(json_object_get_string(jobj_object_callsmade));
            			}
          			}
          			buffer[0] = '\0';
          			sprintf(buffer,"\n------------ESTATISTICAS DE CHAMADAS FEITAS DO GRUPO-----------\nCalls made = %.2lf\n", soma_call_made/arraylen);
          			write(client_fd,buffer,BUF_SIZE-1);
        		}
        		//INFORMACAO DAS CHAMADAS PERDIDAS DO GRUPO
        		else if(strcmp(option,"5")==0)
        		{
          			printf("Um cliente pediu as informacoes de chamadas perdidas do grupo\n");
          			jobj_array = get_student_data();
          			arraylen = json_object_array_length(jobj_array);
          			for (i = 0; i < arraylen; i++)
          			{
            			jobj_obj = json_object_array_get_idx(jobj_array, i);
            			jobj_object_callsmissed = json_object_object_get(jobj_obj, "calls_missed");

            			if(jobj_object_callsmissed!=NULL)
            			{
              				soma_call_missed += atoi(json_object_get_string(jobj_object_callsmissed));
           				}
          			}
          			buffer[0] = '\0';
          			sprintf(buffer,"\n------------ESTATISTICAS DE CHAMADAS PERDIDAS DO GRUPO-----------\nCalls missed = %.2lf\n", soma_call_missed/arraylen);
          			write(client_fd,buffer,BUF_SIZE-1);
        		}
        		//INFORMACAO DAS CHAMADAS RECEBIDAS DO GRUPO
        		else if(strcmp(option,"6")==0)
        		{
          			printf("Um cliente pediu as informacoes de chamadas recebidas do grupo\n");
          			jobj_array = get_student_data();
          			arraylen = json_object_array_length(jobj_array);
          			for (i = 0; i < arraylen; i++)
          			{
            			jobj_obj = json_object_array_get_idx(jobj_array, i);
            			jobj_object_callsreceived= json_object_object_get(jobj_obj, "calls_received");

            			if(jobj_object_callsreceived!=NULL)
            			{
              				soma_call_received += atoi(json_object_get_string(jobj_object_callsreceived));
            			}
          			}
          			buffer[0] = '\0';
          			sprintf(buffer,"\n------------ESTATISTICAS DE CHAMADAS RECEBIDAS DO GRUPO-----------\nCalls received = %.2lf\n",soma_call_received/arraylen);
          			write(client_fd,buffer,BUF_SIZE-1);
        		}
        		//INFORMACAO DOS SMS'S RECEBIDOS DO GRUPO
        		else if(strcmp(option,"7")==0)
        		{
          			printf("Um cliente pediu as informacoes de sms's recebidos do grupo\n");
          			jobj_array = get_student_data();
          			arraylen = json_object_array_length(jobj_array);
          			for (i = 0; i < arraylen; i++)
          			{
            			jobj_obj = json_object_array_get_idx(jobj_array, i);
            			jobj_object_smsreceived = json_object_object_get(jobj_obj, "sms_received");

            			if(jobj_object_smsreceived!=NULL)
            			{
              				soma_sms_received += atoi(json_object_get_string(jobj_object_smsreceived));
            			}
          			}
          			buffer[0] = '\0';
          			sprintf(buffer,"\n----------------ESTATISTICAS DE SMS'S RECEBIDOS DO GRUPO---------------\nSms received = %.2lf\n",soma_sms_received/arraylen);
          			write(client_fd,buffer,BUF_SIZE-1);
        		}
        		//INFORMACAO DOS SMS'S ENVIADOS DO GRUPO
        		else if(strcmp(option,"8")==0)
        		{
          			printf("Um cliente pediu as informacoes de SMS'S enviados do grupo\n");
          			jobj_array = get_student_data();
          			arraylen = json_object_array_length(jobj_array);
          			for (i = 0; i < arraylen; i++)
          			{
            			jobj_obj = json_object_array_get_idx(jobj_array, i);
            			jobj_object_smssent = json_object_object_get(jobj_obj, "sms_sent");

            			if(jobj_object_smssent!=NULL)
            			{
              				soma_sms_sent += atoi(json_object_get_string(jobj_object_smssent));
            			}
	          		}
	          		buffer[0] = '\0';
	          		sprintf(buffer,"\n----------------ESTATISTICAS DE SMS'S ENVIADOS DO GRUPO---------------\nSms sent = %.2lf\n", soma_sms_sent/arraylen);
	          		write(client_fd,buffer,BUF_SIZE-1);
        		}
        		//-----------//
        		//SUBSCRICOES//
        		//-----------//
  				else if(strcmp(option,"9")==0 || strcmp(option,"10")==0 || strcmp(option,"11")==0 || strcmp(option,"12")==0 || strcmp(option,"13")==0 || strcmp(option,"14")==0 || strcmp(option,"15")==0)
  				{
  					if(subscrito[0]==0 && strcmp(option,"9")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
  					{
                        //Subscricao geral
  						printf("Um cliente subscreveu-se as informaçoes do grupo\n");
  						write(client_fd,"Subscreveste-te as informações do grupo!\nSerás notificado cada vez que houver uma alteração",BUF_SIZE-1);
  						subscrito[0]=1; //registra que tal subscricao foi realizada
			    		novo_porto++; //muda o porto
                        sprintf(buffer,"%d\n",novo_porto); //guarda o novo porto no buffer
  						write(client_fd,buffer,BUF_SIZE-1);
  						if(fork()==0)
  						{
  							miniserver(novo_porto,option, 0); //é mandado o numero 0 que é a posição desta subscrição nos arrays da shared memory
  							exit(0);
  						}
  					}
                    else if(subscrito[0]==1 && strcmp(option,"9")==0) //caso tal subscricao ja exista, manda uma mensagem de aviso ao cliente

  					{
  						write(client_fd,"Já te encontras subscrito!",BUF_SIZE-1);
  					}
  					else if(subscrito[1]==0 && strcmp(option,"10")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
  					{
                        //Subscricao duracao de chamadas
  						printf("Um cliente subscreveu-se as informaçoes de duracao de chamadas\n");
  						write(client_fd,"Subscreveste-te as informações de duracao de chamadas!\nSerás notificado cada vez que houver uma alteração",BUF_SIZE-1);
  						subscrito[1]=1;//registra que tal subscricao foi realizada
			    		novo_porto++;//muda o porto
						sprintf(buffer,"%d\n",novo_porto); //guarda o novo porto no buffer
  						write(client_fd,buffer,BUF_SIZE-1);
  						if(fork()==0)
  						{
  							miniserver(novo_porto,option, 1); //é mandado o numero 1 que é a posição desta subscrição nos arrays da shared memory
  							exit(0);
  						}
  					}
  					else if(subscrito[1]==1 && strcmp(option,"10")==0) //caso tal subscricao ja exista, manda uma mensagem de aviso ao cliente
  					{
  						write(client_fd,"Já te encontras subscrito!",BUF_SIZE-1);
  					}
  					else if(subscrito[2]==0 && strcmp(option,"11")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
  					{
                        //Subscricao chamadas recebidas
  						printf("Um cliente subscreveu-se as informaçoes das chamadas recebidas\n");
  						write(client_fd,"Subscreveste-te as informações das chamadas recebidas!\nSerás notificado cada vez que houver uma alteração",BUF_SIZE-1);
  						subscrito[2]=1;//registra que tal subscricao foi realizada
			    		novo_porto++; //muda o porto
						sprintf(buffer,"%d\n",novo_porto); //guarda o novo porto no buffer
  						write(client_fd,buffer,BUF_SIZE-1);
  						if(fork()==0)
  						{
  							miniserver(novo_porto,option, 2); //é mandado o numero 2 que é a posição desta subscrição nos arrays da shared memory
  							exit(0);
  						}
  					}
  					else if(subscrito[2]==1 && strcmp(option,"11")==0) //caso tal subscricao ja exista, manda uma mensagem de aviso ao cliente
  					{
  						write(client_fd,"Já te encontras subscrito!",BUF_SIZE-1);
  					}
  					else if(subscrito[3]==0 && strcmp(option,"12")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
  					{
                        //Subscricao chamadas perdidas
  						printf("Um cliente subscreveu-se as informaçoes das chamadas perdidas\n");
  						write(client_fd,"Subscreveste-te as informações das chamadas perdidas!\nSerás notificado cada vez que houver uma alteração",BUF_SIZE-1);
  						subscrito[3]=1;//registra que tal subscricao foi realizada
			    		novo_porto++; //muda o porto
						sprintf(buffer,"%d\n",novo_porto); //guarda o novo porto no buffer
  						write(client_fd,buffer,BUF_SIZE-1);
  						if(fork()==0)
  						{
  							miniserver(novo_porto,option, 3); //é mandado o numero 3 que é a posição desta subscrição nos arrays da shared memory
  							exit(0);
  						}
  					}
  					else if(subscrito[3]==1 && strcmp(option,"12")==0) //caso tal subscricao ja exista, manda uma mensagem de aviso ao cliente
  					{
  						write(client_fd,"Já te encontras subscrito!",BUF_SIZE-1);
  					}
					if(subscrito[4]==0 && strcmp(option,"13")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
  					{
                        //Subscricao chamadas feitas
  						printf("Um cliente subscreveu-se as informaçoes das chamadas feitas\n");
  						write(client_fd,"Subscreveste-te as informações das chamadas feitas!\nSerás notificado cada vez que houver uma alteração",BUF_SIZE-1);
  						subscrito[4]=1;//registra que tal subscricao foi realizada
			    		novo_porto++; //muda o porto
						sprintf(buffer,"%d\n",novo_porto); //guarda o novo porto no buffer
  						write(client_fd,buffer,BUF_SIZE-1);
  						if(fork()==0)
  						{
  							miniserver(novo_porto,option, 4); //é mandado o numero 4 que é a posição desta subscrição nos arrays da shared memory
  							exit(0);
  						}
  					}
  					else if(subscrito[4]==1 && strcmp(option,"13")==0) //caso tal subscricao ja exista, manda uma mensagem de aviso ao cliente
  					{
  						write(client_fd,"Já te encontras subscrito!",BUF_SIZE-1);
  					}
  					if(subscrito[5]==0 && strcmp(option,"14")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
  					{
                        //Subscricao sms's recebidos
  						printf("Um cliente subscreveu-se as informaçoes de SMS's recebidos\n");
  						write(client_fd,"Subscreveste-te as informações de SMS's recebidos!\nSerás notificado cada vez que houver uma alteração",BUF_SIZE-1);
  						subscrito[5]=1;//registra que tal subscricao foi realizada
			    		novo_porto++; //muda o porto
						sprintf(buffer,"%d\n",novo_porto); //guarda o novo porto no buffer
  						write(client_fd,buffer,BUF_SIZE-1);
  						if(fork()==0)
  						{
  							miniserver(novo_porto,option, 5); //é mandado o numero 0 que é a posição desta subscrição nos arrays da shared memory
  							exit(0);
  						}
  					}
  					else if(subscrito[5]==1 && strcmp(option,"14")==0) //caso tal subscricao ja exista, manda uma mensagem de aviso ao cliente
  					{
  						write(client_fd,"Já te encontras subscrito!",BUF_SIZE-1);
  					}
  					if(subscrito[6]==0 && strcmp(option,"15")==0) //verificar se tal subscricao ja existe e se foi esta a opcao escolhida
  					{
                        //Subscricao sms's mandados
  						printf("Um cliente subscreveu-se as informaçoes de SMS's mandados\n");
  						write(client_fd,"Subscreveste-te as informações de SMS's mandados!\nSerás notificado cada vez que houver uma alteração",BUF_SIZE-1);
  						subscrito[6]=1;//registra que tal subscricao foi realizada
			    		novo_porto++; //muda o porto
						sprintf(buffer,"%d\n",novo_porto); //guarda o novo porto no buffer
  						write(client_fd,buffer,BUF_SIZE-1);
  						if(fork()==0)
  						{
  							miniserver(novo_porto,option, 6); //é mandado o numero 6 que é a posição desta subscrição nos arrays da shared memory
  							exit(0);
  						}
  					}
  					else if(subscrito[6]==1 && strcmp(option,"15")==0) //caso tal subscricao ja exista, manda uma mensagem de aviso ao cliente
  					{
  						write(client_fd,"Já te encontras subscrito!",BUF_SIZE-1);
  					}				
  				}
  				//--------------------//
  				//CANCELAR SUBSCRICOES//
  				//--------------------//
  				else if(strcmp(option,"16")==0 || strcmp(option,"17")==0 || strcmp(option,"18")==0 || strcmp(option,"19")==0 || strcmp(option,"20")==0 || strcmp(option,"21")==0 || strcmp(option,"22")==0)
  				{
  					if(subscrito[0]==1 && strcmp(option,"16")==0)
  					{
  						printf("Um cliente cancelou a sua subscricao\n");
  						write(client_fd,"A tua subscricao foi cancelada",BUF_SIZE-1);
  						subscrito[0]=0; //registra que não há tal subscricao
  						sleep(1); // sleep usado para que o processo do client seja terminado primeiro
        				shared_mem->elimina=0; //indica a posicao desta subscricao nos arrays
        				quit_sub(); //funcao que elimina o processo da subscricao
  					}
 					else if(subscrito[1]==1 && strcmp(option,"17")==0)
  					{
  						printf("Um cliente cancelou a sua subscricao\n");
  						write(client_fd,"A tua subscricao foi cancelada",BUF_SIZE-1);
  						subscrito[1]=0;//registra que não há tal subscricao
  						sleep(1);// sleep usado para que o processo do client seja terminado primeiro
        				shared_mem->elimina=1;//indica a posicao desta subscricao nos arrays
        				quit_sub();//funcao que elimina o processo da subscricao
  					}
 					else if(subscrito[2]==1 && strcmp(option,"18")==0)
  					{
  						printf("Um cliente cancelou a sua subscricao\n");
  						write(client_fd,"A tua subscricao foi cancelada",BUF_SIZE-1);
  						subscrito[2]=0;//registra que não há tal subscricao
  						sleep(1);// sleep usado para que o processo do client seja terminado primeiro
        				shared_mem->elimina=2;//indica a posicao desta subscricao nos arrays
        				quit_sub();//funcao que elimina o processo da subscricao
  					}
 					else if(subscrito[3]==1 && strcmp(option,"19")==0)
  					{
  						printf("Um cliente cancelou a sua subscricao\n");
  						write(client_fd,"A tua subscricao foi cancelada",BUF_SIZE-1);
  						subscrito[3]=0;//registra que não há tal subscricao
  						sleep(1);// sleep usado para que o processo do client seja terminado primeiro
        				shared_mem->elimina=3;//indica a posicao desta subscricao nos arrays
        				quit_sub();//funcao que elimina o processo da subscricao
  					}
 					else if(subscrito[4]==1 && strcmp(option,"20")==0)
  					{
  						printf("Um cliente cancelou a sua subscricao\n");
  						write(client_fd,"A tua subscricao foi cancelada",BUF_SIZE-1);
  						subscrito[4]=0;//registra que não há tal subscricao
  						sleep(1);// sleep usado para que o processo do client seja terminado primeiro
        				shared_mem->elimina=4;//indica a posicao desta subscricao nos arrays
        				quit_sub();//funcao que elimina o processo da subscricao
  					} 
 					else if(subscrito[5]==1 && strcmp(option,"21")==0)
  					{
  						printf("Um cliente cancelou a sua subscricao\n");
  						write(client_fd,"A tua subscricao foi cancelada",BUF_SIZE-1);
  						subscrito[5]=0;//registra que não há tal subscricao
  						sleep(1);// sleep usado para que o processo do client seja terminado primeiro
        				shared_mem->elimina=5;//indica a posicao desta subscricao nos arrays
        				quit_sub();//funcao que elimina o processo da subscricao
  					}   
 					else if(subscrito[6]==1 && strcmp(option,"22")==0)
  					{
  						printf("Um cliente cancelou a sua subscricao\n");
  						write(client_fd,"A tua subscricao foi cancelada",BUF_SIZE-1);
  						subscrito[6]=0;//registra que não há tal subscricao
  						sleep(1);// sleep usado para que o processo do client seja terminado primeiro
        				shared_mem->elimina=6;//indica a posicao desta subscricao nos arrays
        				quit_sub();//funcao que elimina o processo da subscricao
  					}												
  					else
  						write(client_fd,"Nao te encontras subscrito",BUF_SIZE-1); //mensagem caso o cliente tente tirar uma subscrição não existente
  				}
  				//EXIT
  				else if(strcmp(option,"23")==0)
  				{
          			y=-1; //variavel do while mudada para terminar o processo
          			for(int i=0;i<7;i++)
                    {
  						if(subscrito[i]==1) //caso tenha subscricao, fechamos respectivo socket
  						{
  							shared_mem->elimina=i;
  							quit_sub(); //destruir todos os processos de subscricao existentes e os respetivos sockets
  						}
                    }
                    shmctl(shm_id,IPC_RMID, NULL); //destruir a shared memory
                }
  		  		//INTRODUCAO DE UM COMANDO INVALIDO
				else
				{
          			printf("Foi introduzido um comando invalido");
          			write(client_fd,"Comando invalido",BUF_SIZE-1);
				}
		  	}
		}
		else
		{
		write(client_fd,"O teu ID não pertence á base de dados, tenta de novo!\n",BUF_SIZE-1);
		}
  	}
	close(client_fd);
}

//FUNCAO PARA DETECAO DE ERROS
void erro(char *msg)
{
	printf("Erro: %s\n", msg);
	exit(-1);
}

//FUNCAO PARA A CRIACAO DOS PROCESSOS DE SUBSCRICAO//
//-------------------------------------------------//
//Dependendo da 'option' passada, o novo processo  //
//ira imprimir as estatisticas relativas ao que foi//
//pedido pelo cliente                              //
//-------------------------------------------------//
void miniserver(int novo_porto, char *option, int num)  //sendo num a referência dos arrays de acordo com a opção selecionada
{
	int fd;
	struct sockaddr_in addr, client_addr;
	int client_addr_size;
	double soma_call_duration = 0, soma_call_made = 0, soma_call_missed = 0, soma_call_received = 0, soma_sms_sent = 0, soma_sms_received = 0;
	double media_call_duration = 0, media_call_made = 0, media_call_missed = 0, media_call_received = 0, media_sms_sent = 0, media_sms_received = 0;
	struct json_object *jobj_array, *jobj_obj;
	struct json_object *jobj_object_callsduration, *jobj_object_callsmade, *jobj_object_callsmissed, *jobj_object_callsreceived, *jobj_object_smsreceived, *jobj_object_smssent;
	int arraylen = 0;
	int i;

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY); //Uso do big-endian, recebe através de qualquer das placas
	addr.sin_port = htons((short)novo_porto);

	if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) //Crias o socket
		perror("na funcao socket");
	if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0) //Ligas o socket
		perror("na funcao bind2");
	if( listen(fd, 5) < 0) //Até 5 utilizadores
		perror("na funcao listen");

	while (1)
	{
		client_addr_size = sizeof(client_addr);
		shared_mem->client2[num] = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size); //guardar o client2 para mais tarde conseguir destruir o socket
		if (shared_mem->client2[num] > 0)
		{
		    close(fd);
		    char buffer[BUF_SIZE];
		    while(1)
			{
				printf("(SUBSCRICAO) Informacao atualizada - proxima atualizacao daqui a 10segundos\n");
				jobj_array = get_student_data();
				arraylen = json_object_array_length(jobj_array);
                //variaveis utilizadas para calcular as medias
				soma_call_duration=0;
				soma_call_made=0;
				soma_call_missed=0;
				soma_call_received=0;
				soma_sms_received=0;
				soma_sms_sent=0;
				//RECOLHER OS DADOS A BASE DE DADOS
		  		for (i = 0; i < arraylen; i++)
		  		{
		  			jobj_obj = json_object_array_get_idx(jobj_array, i);
					jobj_object_callsduration = json_object_object_get(jobj_obj, "calls_duration");
					jobj_object_callsmade = json_object_object_get(jobj_obj, "calls_made");
					jobj_object_callsmissed = json_object_object_get(jobj_obj, "calls_missed");
					jobj_object_callsreceived= json_object_object_get(jobj_obj, "calls_received");
					jobj_object_smsreceived = json_object_object_get(jobj_obj, "sms_received");
					jobj_object_smssent = json_object_object_get(jobj_obj, "sms_sent");

					if(jobj_object_callsduration!=NULL && jobj_object_callsmade!=NULL && jobj_object_callsmissed!=NULL && jobj_object_callsreceived!=NULL && jobj_object_smsreceived!=NULL && jobj_object_smssent!=NULL)
					{
						soma_call_duration += atoi(json_object_get_string(jobj_object_callsduration));
						soma_call_made += atoi(json_object_get_string(jobj_object_callsmade));
						soma_call_missed += atoi(json_object_get_string(jobj_object_callsmissed));
						soma_call_received += atoi(json_object_get_string(jobj_object_callsreceived));
						soma_sms_received += atoi(json_object_get_string(jobj_object_smsreceived));
						soma_sms_sent += atoi(json_object_get_string(jobj_object_smssent));
					}
				}
				if(media_call_duration!=soma_call_duration/arraylen || media_call_made!=soma_call_made/arraylen || media_call_missed!=soma_call_missed/arraylen || media_call_received!=soma_call_received/arraylen || media_sms_received!=soma_sms_received/arraylen || media_sms_sent!=soma_sms_sent/arraylen)
				{
					media_call_duration=soma_call_duration/arraylen;
					media_call_made=soma_call_made/arraylen;
					media_call_missed=soma_call_missed/arraylen;
					media_call_received=soma_call_received/arraylen;
					media_sms_received=soma_sms_received/arraylen;
					media_sms_sent=soma_sms_sent/arraylen;
					buffer[0] = '\0';
					//PRINT CONSOANTE A OPÇAO ESCOLHIDA
					if(strcmp(option,"9")==0)
					{
						shared_mem->child_pid[0]=getpid();
						sprintf(buffer,"\n--------------ATUALIZAÇAO-SUBSCRICAO-------------\nCalls duration(s) = %.2lf\nCalls made = %.2lf\nCalls missed = %.2lf\nCalls received = %.2lf\nSms received = %.2lf\nSms sent = %.2lf\n",media_call_duration, media_call_made, media_call_missed, media_call_received, media_sms_received, media_sms_sent);
						write(shared_mem->client2[num],buffer,BUF_SIZE-1);
					}
					else if(strcmp(option,"10")==0)
					{
						shared_mem->child_pid[1]=getpid();
						sprintf(buffer,"\n--------------ATUALIZAÇAO-SUBSCRICAO-------------\nCalls duration(s) = %.2lf\n",media_call_duration);
						write(shared_mem->client2[num],buffer,BUF_SIZE-1);
					}
					else if(strcmp(option,"11")==0)
					{
						shared_mem->child_pid[2]=getpid();
						sprintf(buffer,"\n--------------ATUALIZAÇAO-SUBSCRICAO-------------\nCalls made = %.2lf\n",media_call_made);
						write(shared_mem->client2[num],buffer,BUF_SIZE-1);
					}
					else if(strcmp(option,"12")==0)
					{
						shared_mem->child_pid[3]=getpid();
						sprintf(buffer,"\n--------------ATUALIZAÇAO-SUBSCRICAO-------------\nCalls missed = %.2lf\n",media_call_missed);
						write(shared_mem->client2[num],buffer,BUF_SIZE-1);
					}
					else if(strcmp(option,"13")==0)
					{
						shared_mem->child_pid[4]=getpid();
						sprintf(buffer,"\n--------------ATUALIZAÇAO-SUBSCRICAO-------------\nCalls received = %.2lf\n",media_call_received);
						write(shared_mem->client2[num],buffer,BUF_SIZE-1);
					}
					else if(strcmp(option,"14")==0)
					{
						shared_mem->child_pid[5]=getpid();
						sprintf(buffer,"\n--------------ATUALIZAÇAO-SUBSCRICAO-------------\nSms received = %.2lf\n",media_sms_received);
						write(shared_mem->client2[num],buffer,BUF_SIZE-1);
					}	
					else if(strcmp(option,"15")==0)
					{
						shared_mem->child_pid[6]=getpid();
						sprintf(buffer,"\n--------------ATUALIZAÇAO-SUBSCRICAO-------------\nSms sent = %.2lf\n",media_sms_sent);
						write(shared_mem->client2[num],buffer,BUF_SIZE-1);
					}		
				}
				sleep(10); //ATUALIZA A INFORMACAO DE 10 EM 10seg
			}
		}
	}
}
