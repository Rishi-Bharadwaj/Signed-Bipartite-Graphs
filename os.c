#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define MAX_QUEUE_SIZE 1000
#define soft_limit 5
typedef struct PassengerRequest{
    int requestId;
    int startFloor;
    int requestedFloor;
}PassengerRequest;

typedef struct MainSharedMemory{
    char authStrings[100][21];
    char elevatorMovementInstructions[100];
    PassengerRequest newPassengerRequests[30];
    int elevatorFloors[100];
    int droppedPassengers[1000];
    int pickedUpPassengers[1000][2];
} MainSharedMemory;

typedef struct SolverRequest{
    long mtype;
    int elevatorNumber;
    char authStringGuess[21];
} SolverRequest;

typedef struct SolverResponse{
    long mtype;
    int guessIsCorrect;
}SolverResponse;

typedef struct TurnChangeResponse{
    long mtype;
    int turnNumber;
    int newPassengerRequestCount;
    int errorOccured;
    int finished;
}TurnChangeResponse;

typedef struct TurnChangeRequest{
    long mtype;
    int droppedPassengersCount;
    int pickedupPassengersCount;
} TurnChangeRequest;



typedef struct {
    PassengerRequest requests[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} PassengerQueue;

int passenger_already_assigned(int pass_id, int n, int going_to[][soft_limit][4]) {
    for(int i = 0; i < n; i++) {
        for(int j = 0; j < soft_limit; j++) {
            if(going_to[i][j][0] == pass_id) {
                return 1;
            }
        }
    }
    return 0;
}


void initQueue(PassengerQueue* queue) {
    queue->front =0;
    queue->rear = -1;
    queue->size =0;
}

int enqueue(PassengerQueue* queue, PassengerRequest request) {
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    queue->requests[queue->rear] = request;
    queue->size++;
    return 0;
}

int dequeue(PassengerQueue* queue, PassengerRequest* request) {
    if (queue->size == 0) {
        return -1;
    }
    *request = queue->requests[queue->front];
    queue->front = (queue->front +1) % MAX_QUEUE_SIZE;
    queue->size--;
    return 0;
}

int is_empty(PassengerQueue* queue)
{
    if(queue->size==0)
    return 1;
    else
    return 0;
}
double power(double base, int exp) {
    double result = 1.0;
    for (int i = 0; i < abs(exp); i++) {
        result *= base;
    }
    if (exp < 0) {
        result = 1.0 / result;
    }
    return result;
}

int guesser(int n,int id, int elevatorNumber, char* ans)
{
    char letters[]="abcdef";
    char current_guess[n+1];
    int arr[n];
    SolverRequest req;
    req.mtype=2;
    req.elevatorNumber=elevatorNumber;
    //printf("elevator number :%d",elevatorNumber);
    msgsnd(id,&req, sizeof(SolverRequest)-sizeof(long),0);
    req.mtype=3;
    SolverResponse rep;
    memset(arr, 0, sizeof(arr));
    int attempts=0;
    int max_attempts=power(6,n);
    while(attempts<max_attempts)
    {
        for(int i=n-1;i>=0;i--)
        {
            if(arr[i]>=6)
            {
                arr[i]=0;
                if (i == 0) {
                    return 0;
                }
                arr[i-1]++;
            }
        }
        for(int i=0;i<n;i++)
        {
            current_guess[i]=letters[arr[i]];
        }
        current_guess[n]='\0';
        //printf("Current guess: %s\n", current_guess);
        strcpy(req.authStringGuess, current_guess);
        msgsnd(id,&req, sizeof(SolverRequest)-sizeof(long),0);
        msgrcv(id,&rep, sizeof(SolverResponse)-sizeof(long),4,0);
        int right=rep.guessIsCorrect;
        if(right!=0)
        {
            strcpy(ans, current_guess);
            //printf("done\n");
            return 1;
        }
        arr[n-1]++;
        attempts++;
    }
    return 0;
}

void delete_ele(int going_to[][soft_limit][4],int i, int max_pass, int target_j)
{
    for(int s= target_j;s<max_pass-1;s++)
    {
        going_to[i][s][0]=going_to[i][s+1][0];
        going_to[i][s][1]=going_to[i][s+1][1];
        going_to[i][s][2]=going_to[i][s+1][2];
        going_to[i][s][3]=going_to[i][s+1][3];
    }
    going_to[i][max_pass-1][0]=-1;
    going_to[i][max_pass-1][1]=-1;
    going_to[i][max_pass-1][2]=-1;
    going_to[i][max_pass-1][3]=-1;
}


int main(int argc, char* argv[])
{
    FILE* i;
    char input[12]="input.txt";
    i =fopen(input,"r");
    fseek(i, 0, SEEK_END);
    long file_size = ftell(i);
    rewind(i);
    char* i_buffer=(char*)malloc(file_size+1);
    fread(i_buffer, 1, file_size, i);
    i_buffer[file_size]='\n';
    char *delimiters = " \n";
    char *token = strtok(i_buffer, delimiters);
    int n =atoi(token);
    token= strtok(NULL,delimiters);
    int k = atoi(token);
    token= strtok(NULL,delimiters);
    int m = atoi(token);
    token= strtok(NULL,delimiters);
    int t= atoi(token);
    token= strtok(NULL,delimiters);
    key_t shared_mem_key = (key_t)strtol(token,NULL,10);
    token= strtok(NULL,delimiters);
    key_t main_msg_key = (key_t)strtol(token,NULL,10);

    key_t solver_keys [m];
    for(int i=0;i<m;i++)
    {
        token= strtok(NULL,delimiters);
        solver_keys[i]= (key_t)strtol(token,NULL,10);
    }
    MainSharedMemory* shmptr;
    int shmid = shmget(shared_mem_key, sizeof(MainSharedMemory), 0);
    shmptr=shmat(shmid, NULL, 0);

    int solverId[m];
    for(int i=0;i<m;i++)
    {
        solverId[i]=msgget(solver_keys[i], 0);
    }
    TurnChangeResponse new_state;
    int main_msgid = msgget(main_msg_key,0);

    int total_picked_up=0, total_dropped_off=0; 
    int num_people[n];
    int going_to[n][soft_limit][4];
    int dir[n][2];
    int carrying[n];
    int moved[n];
    int to_subtract[n];
    memset(to_subtract,0,sizeof(to_subtract));
    memset(moved,0,sizeof(moved));
    // int ppl_for_auth[n];
    // memset(ppl_for_auth,0,sizeof(ppl_for_auth));
    memset(carrying,0,sizeof(carrying));
    memset(dir,0,sizeof(dir));
    memset(going_to,-1,sizeof(going_to));
    memset(num_people,0,sizeof(num_people));

    PassengerQueue queue;
    initQueue(&queue);

    while(1)
    {
        printf("Waiting for TurnChangeResponse from main process\n");
msgrcv(main_msgid, &new_state, sizeof(TurnChangeResponse)-sizeof(long),2,0);
printf("Received TurnChangeResponse\n");

        int new_passengers=new_state.newPassengerRequestCount;
        if(new_passengers)
        printf("new passengers: %d\n", new_passengers);
        printf("turn number: %d\n", new_state.turnNumber);
        printf("new_state finished: %d\n", new_state.finished);
        int dropped=0;
        memset(to_subtract,0,sizeof(to_subtract));
        
        for(int i=0;i<n;i++)
        {
            for(int j=0;j<soft_limit;j++)
            {
                if(going_to[i][j][0] != -1 && going_to[i][j][1]==shmptr->elevatorFloors[i] && going_to[i][j][3]==1)
                {
                    shmptr->droppedPassengers[dropped++]=going_to[i][j][0];
                    printf("person %d dropped at %d\n", going_to[i][j][0], going_to[i][j][1]);
                    total_dropped_off++;
                    //printf("initial dir : %d\n", dir[i][0]);
                    if(moved[i]>0)
                    {
                        dir[i][0]--;
                    }
                    
                    else if (moved[i]<0)
                    {
                        dir[i][1]--;
                    }
                    //printf("after dir: %d\n", dir[i][0]);
                    delete_ele(going_to, i , soft_limit, j);
                    to_subtract[i]--;
                    num_people[i]--;
                    j--;
                }
            }
        }
        int picked_up=0;
        for(int i=0;i<queue.size;i++)
        {
            PassengerRequest nextRequest;
            if(dequeue(&queue, &nextRequest)==0)
            {
            int start_floor=nextRequest.startFloor;
            int pass_id=nextRequest.requestId;
            int req_floor=nextRequest.requestedFloor;
            int min_distance=k;
            int best_ele=-1;
            if(passenger_already_assigned(pass_id, n, going_to)) {
        continue;
    }
            if(start_floor==req_floor)
            continue;
            for(int j=0;j<n;j++)
            {
                int going=start_floor-shmptr->elevatorFloors[j];
                if(abs(shmptr->elevatorFloors[j]-start_floor)<=min_distance && num_people[j]<soft_limit && going>0 &&moved[j]>0)
                {
                    min_distance=abs(shmptr->elevatorFloors[j]-start_floor);
                    best_ele=j;
                }
                else if(abs(shmptr->elevatorFloors[j]-start_floor)<=min_distance && num_people[j]<soft_limit && going<0 &&moved[j]<0)
                {
                    min_distance=abs(shmptr->elevatorFloors[j]-start_floor);
                    best_ele=j;
                }
            }
            if(best_ele==-1)
            {
                for(int j=0;j<n;j++)
                {
                    if(abs(shmptr->elevatorFloors[j]-start_floor)<=min_distance && num_people[j]<soft_limit)
                    {
                        min_distance=abs(shmptr->elevatorFloors[j]-start_floor);
                        best_ele=j;
                    }
                }
            }

            if(best_ele==-1)
            {
                enqueue(&queue, nextRequest);
                continue;
            }

            going_to[best_ele][num_people[best_ele]][0]=pass_id;
            going_to[best_ele][num_people[best_ele]][1]=req_floor;
            going_to[best_ele][num_people[best_ele]][2]=start_floor;
            num_people[best_ele]++;
            }
        }
        for(int i=0;i<new_passengers;i++)
        {
            int start_floor=shmptr->newPassengerRequests[i].startFloor;
            int pass_id=shmptr->newPassengerRequests[i].requestId;
            int req_floor=shmptr->newPassengerRequests[i].requestedFloor;
            int min_distance=k;
            int best_ele=-1;
            printf("person id: %d\n",pass_id );
            printf("start_floor %d\n", start_floor);
            printf("req_floor: %d\n", req_floor);
            if(start_floor==req_floor)
            continue;
            if(passenger_already_assigned(pass_id, n, going_to)) {
        continue;
    }
            for(int j=0;j<n;j++)
            {
                int going=start_floor-shmptr->elevatorFloors[j];
                if(abs(shmptr->elevatorFloors[j]-start_floor)<=min_distance && num_people[j]<soft_limit && going>0 && moved[j]>0)
                {
                    min_distance=abs(shmptr->elevatorFloors[j]-start_floor);
                    best_ele=j;
                }
                else if(abs(shmptr->elevatorFloors[j]-start_floor)<=min_distance && num_people[j]<soft_limit && going<0 && moved[j]<0)
                {
                    min_distance=abs(shmptr->elevatorFloors[j]-start_floor);
                    best_ele=j;
                }
            }
            if(best_ele==-1)
            {
                for(int j=0;j<n;j++)
                {
                    if(abs(shmptr->elevatorFloors[j]-start_floor)<=min_distance && num_people[j]<soft_limit)
                    {
                        min_distance=abs(shmptr->elevatorFloors[j]-start_floor);
                        best_ele=j;
                    }
                }
            }

            if(best_ele==-1)
            {
                PassengerRequest oldRequest=shmptr->newPassengerRequests[i];
                enqueue(&queue,oldRequest);
                continue;
            }

            going_to[best_ele][num_people[best_ele]][0]=pass_id;
            going_to[best_ele][num_people[best_ele]][1]=req_floor;
            going_to[best_ele][num_people[best_ele]][2]=start_floor;
            num_people[best_ele]++;
        }
        memset(dir,0,sizeof(dir));
        for(int i=0;i<n;i++)
        {
            for(int j=0;j<soft_limit;j++)
            {
                if(going_to[i][j][3]==1)
                {
                    if(going_to[i][j][0]!=-1 && going_to[i][j][1]>shmptr->elevatorFloors[i])
                    {
                        dir[i][0]++;
                    }
                    else if(going_to[i][j][0]!=-1 && going_to[i][j][1]<shmptr->elevatorFloors[i])
                    {
                        dir[i][1]++;
                    }
                }
                else
                {
                    if(going_to[i][j][0]!=-1 && going_to[i][j][2]>shmptr->elevatorFloors[i])
                    {
                        dir[i][0]++;
                    }
                    else if(going_to[i][j][0]!=-1 && going_to[i][j][2]<shmptr->elevatorFloors[i])
                    {
                        dir[i][1]++;
                    }
                }
                
            }
        }
        int old_moved[n];
        for(int i=0;i<n;i++)
        {
            old_moved[i]=moved[i];
        }
        memset(moved,0,sizeof(moved));
        for(int i=0;i<n;i++)
        {
            shmptr->elevatorMovementInstructions[i]='s';
            moved[i]=0;
            if(num_people[i]!=0 && (dir[i][0]>dir[i][1]) && shmptr->elevatorFloors[i]!=k-1)
            {
                shmptr->elevatorMovementInstructions[i]='u';
                moved[i]=1;
            }
            else if(num_people[i]!=0 && dir[i][0]<dir[i][1] && shmptr->elevatorFloors[i]!=0)
            {
                shmptr->elevatorMovementInstructions[i]='d';
                moved[i]=-1;
            }
            else if(num_people[i]!=0 && dir[i][0]==dir[i][1])
            {
                if(old_moved[i]==-1 && shmptr->elevatorFloors[i]!=0)
                {
                    shmptr->elevatorMovementInstructions[i]='d';
                    moved[i]=-1;
                }
                else if(old_moved[i]==1 && shmptr->elevatorFloors[i]!=k-1)
                {
                    shmptr->elevatorMovementInstructions[i]='u';
                    moved[i]=1;
                }
            }
            if(i==4)
            {
                for(int k=0;k<soft_limit;k++)
                {
                    printf("pass id: %d\n", going_to[i][k][0]);
                    printf("start_floor: %d\n", going_to[i][k][2]);
                    printf("req floor: %d\n", going_to[i][k][1]);
                }
                printf("dir[i][0]: %d dir[i][1] : %d\n",dir[i][0],dir[i][1]);
                 printf("floor:%d \n", shmptr->elevatorFloors[i]);
                 printf("moved: %c\n",shmptr->elevatorMovementInstructions[i]);
            }
            // printf("elevator number: %d\n", i);
            // printf("moved: %c\n",shmptr->elevatorMovementInstructions[i]);
            // printf("floor:%d \n", shmptr->elevatorFloors[i]);
            
        }

        int solvers[m];
        memset(solvers,0,sizeof(solvers));
        for(int i=0;i<n;i++)
        {
            if(moved[i]!=0 && carrying[i]!=0)
            {
                char* ans= (char*)malloc(21*sizeof(char));

                for(int j=0;j<m;j++)
                {
                    if(solvers[j]==0)
                    {
                        solvers[j]=1;
                        //printf("carrying : %d",carrying[i]);
                        printf("Calling guesser with n = %d for elevator %d\n", carrying[i], i);
                        guesser(carrying[i],solverId[j],i,ans);
                        strcpy(shmptr->authStrings[i],ans);
                        solvers[j]=0;
                        break;
                    }
                }
                free(ans);
            }
        }
        for(int i=0;i<n;i++)
        {
            carrying[i]=carrying[i]+to_subtract[i];
        }
        for(int i=0;i<n;i++)
        {
            for(int j=0;j<soft_limit;j++)
            {
                if(going_to[i][j][0] != -1 && going_to[i][j][2]==shmptr->elevatorFloors[i] && going_to[i][j][3] == -1)
                {
                    if(carrying[i] >= soft_limit)
                break;
                    //printf("going_to: %d", going_to[i][j][2]);
                    //printf("elev floor, %d",shmptr->elevatorFloors[i]);
                    if(going_to[i][j][0]==30)
                    {
                        printf("picked_up");
                    }
                    shmptr->pickedUpPassengers[picked_up][0]=going_to[i][j][0];
                    shmptr->pickedUpPassengers[picked_up++][1]=i;
                    total_picked_up++;
                    going_to[i][j][3]=1;
                    carrying[i]++;
                    //printf("carrying : %d",carrying[i]);
                }
            }
        }
        printf("picked up: %d\n", total_picked_up);
        printf("dropped off: %d\n",total_dropped_off);
        
        TurnChangeRequest request;
        request.mtype=1;
        request.droppedPassengersCount=dropped;
        request.pickedupPassengersCount=picked_up;
        printf("Sending TurnChangeRequest to main process\n");
msgsnd(main_msgid,&request,sizeof(TurnChangeRequest)-sizeof(long),0);
printf("Sent TurnChangeRequest\n");
    }
}