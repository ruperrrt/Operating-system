#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "cs402.h"

#include "my402list.h"

#ifndef DETERMINISTIC
#define DETERMINISTIC 1
#define TRACE_DRIVEN 0
#endif /* ~DETERMINISTIC */

#ifndef PACKET_LIMIT
#define MAX_NUM 10000
#endif /* ~DETERMINISTIC */

typedef struct tagCommandline{
    double lambda;
    double mu;
    double r;
    int B;      //num of max token
    int P;
    int num;
    char * tsfile;
}cmdline;

typedef struct tagPacket{
    long int packet_id;
    int num_token_needed;
    double service_time;
    double internal_arrival_time;
    double arrive_time;
    double enter_q1_time;
    double leave_q1_time;
    double enter_q2_time;
    double leave_q2_time;
    double start_s_time;
    double end_s_time;
    int which_server;
}pkt;

pthread_t packet_arrival_t, token_arrival_t, server1_t, server_2_t, deal_with_large_num_pkt;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wakeup_server = PTHREAD_COND_INITIALIZER;
pthread_cond_t wakeup_extra = PTHREAD_COND_INITIALIZER;
int num_pkt, num_pkt_dropped, num_pkt_removed, mode = TRACE_DRIVEN, num_token, num_total_token = 0, num_token_dropped = 0, q2_num_pkt = 0, terminate_flag, control_c = FALSE;
double total_internal_arrival_time;
cmdline * argmnts;
My402List packet_list, packet_recycle, Q1, Q2;
struct timeval start_program, end_program;
sigset_t set;
int num_pkt_processed = 0, num_left = 0;


void printerr (char * error){
    fprintf(stderr, "Error: %s!\n", error);
    exit(-1);
}

void print_context (char * input, double time){
    char temp[20];
    strcpy(temp, "00000000.000");
    char num [20];
    sprintf(num, "%.3f", time);
    int len = (int)strlen(num);
    int i;
    for(i = 11; i>=12-len; i--){
        temp[i] = num[len-12+i];
    }
    fprintf(stdout, "    %sms: %s\n", temp, input);
}

void setToDefualt(cmdline * argmnts){
    argmnts->lambda = 1;
    argmnts->mu = 0.35;
    argmnts->B = 10;
    argmnts->r = 1.5;
    argmnts->P = 3;
    argmnts->num = 20;
}

double format_input (double input){
    if(input>10)
        return 10*1000;
    return (round(input*1000.0)/1000.0)*1000;
}

double to_usec (double sec){
    return sec * 1000000;
}

double time_elapse(struct timeval tv1, struct timeval tv2){
    return to_usec(tv2.tv_sec-tv1.tv_sec) + tv2.tv_usec - tv1.tv_usec;
}

int checkInt (char * input){
    int i = 0;
    while(input[i]!='\0'){
        if(input[i]>'9'||input[i]<'0'){
            printerr("malformed input! Not an integer");
        }
        i++;
    }
    return atoi(input);
}

double checkDouble (char * input){
    int i = 0;
    while(input[i]!='\0'){
        if((input[i]>'9'||input[i]<'0') && input[i]!='.'){
            printerr("malformed input! Not a number");
        }
        i++;
    }
    return atof(input);
}

int commandLine (int argc, char * argv[]){
    int set_lam, set_mu, set_r, set_B, set_P, set_n, set_t;
    set_lam = set_mu = set_r = set_B = set_P = set_n = set_t = 0;
    if(argc%2==0){
        printerr("malformed input! Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]");
    }
    int i;
    for(i=1; i<argc; i+=2){
        if (!strcmp(argv[i], "-lambda") && set_lam==0) {
            argmnts->lambda = atof(argv[i+1]);
            set_lam = 1;
            continue;
        }
        if (!strcmp(argv[i], "-mu") && set_mu==0) {
            argmnts->mu = atof(argv[i+1]);
            set_mu = 1;
            continue;
        }
        if (!strcmp(argv[i], "-r") && set_r==0) {
            argmnts->r = atof(argv[i+1]);
            set_r = 1;
            continue;
        }
        if (!strcmp(argv[i], "-B") && set_B==0) {
            argmnts->B = checkInt(argv[i+1]);
            set_B = 1;
            continue;
        }
        if (!strcmp(argv[i], "-P") && set_P==0) {
            argmnts->P = checkInt(argv[i+1]);
            set_P = 1;
            continue;
        }
        if (!strcmp(argv[i], "-n") && set_n==0) {
            argmnts->num = checkInt(argv[i+1]);
            set_n = 1;
            continue;
        }
        if (!strcmp(argv[i], "-t") && set_t==0) {
            argmnts->tsfile = argv[i+1];
            set_t = 1;
            mode = DETERMINISTIC;
            continue;
        }
        printerr("malformed input! Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]");
    }
    return mode;
}

void print_parameter (){
    fprintf(stdout, "    Emulation Parameters:\n");
    if(!mode){
        fprintf(stdout, "        lambda = %.6g\n", argmnts->lambda);
        fprintf(stdout, "        mu = %.6g\n", argmnts->mu);
    }
    fprintf(stdout, "        r = %.6g\n", argmnts->r);
    fprintf(stdout, "        B = %d\n", argmnts->B);
    if(!mode)
        fprintf(stdout, "        P = %d\n", argmnts->P);
    if(mode)
        fprintf(stdout, "        tsfile = %s\n", argmnts->tsfile);
}

int fileInput (int argc, char * argv[]){
    argmnts = (cmdline*)malloc(sizeof(cmdline));
    setToDefualt(argmnts);
    commandLine(argc, argv);
    
    if(argmnts->num > 2147483647){
        printerr("Input packt number exceeds the limit");
    }
    
    if(!My402ListInit(&packet_list))
        perror("Cannot initiate packet list!");
    
    if(!mode){
        int pkt_id = 1 + num_pkt_processed;
        int p_num = argmnts->num;
        if(argmnts->num > MAX_NUM)
            p_num = MAX_NUM;
        while(pkt_id <= p_num){
            pkt * temp1 = (pkt*)malloc(sizeof(pkt));
            if(!temp1){
                continue;
            }
            temp1->internal_arrival_time = format_input(1.0/(double)argmnts->lambda);
            temp1->service_time = format_input(1/argmnts->mu);
            temp1->num_token_needed = argmnts->P;
            temp1->packet_id = pkt_id;
            pkt_id++;
            My402ListAppend(&packet_list, (void *)temp1);
            //free(temp1);
        }
        return TRUE;
    }
    
    if(mode){
        FILE * fp;
        fp = fopen(argmnts->tsfile, "r");
        if(fp==NULL){
            fprintf(stderr, "%s", strerror(errno));
            printerr(" file cannot open  exit the program");
        }
        char buf [1026];
        if(fgets(buf, sizeof(buf), fp) == NULL)
            printf("The tsfile contains no data");

        if(buf[0] == '\0')
            printerr("malformed input - line 1 is not just a number");
        int i = 0;
        while(buf[i]!='\0'){
            if(!isdigit(buf[i]))
                printerr("malformed input - line 1 is not just a number");
            i++;
        }

        num_pkt = atoi(buf);
        int pkt_id = 1;
        while (1) {
            pkt *temp = (pkt*)malloc(sizeof(pkt));
            temp->packet_id = pkt_id;
            if(fgets(buf, sizeof(buf), fp)==NULL){
                return TRUE;
            }
            char *start_ptr = buf;
            char *tab_ptr = start_ptr;
            /* this while loop will find the first space/tab */
            while(!isspace(*tab_ptr))
                tab_ptr++;
            if(tab_ptr != NULL)
                *tab_ptr++ = '\0';
            temp->internal_arrival_time = atof(start_ptr);//inter-arrival time in milliseconds
            
            start_ptr = tab_ptr;
            while(isspace(*start_ptr)){
                start_ptr++;
            }
            tab_ptr = start_ptr;
            while(!isspace(*tab_ptr))
                tab_ptr++;
            if (tab_ptr != NULL)
                *tab_ptr++ = '\0';
            temp->num_token_needed = atoi(start_ptr);   //the number of tokens required
            
            start_ptr = tab_ptr;
            while(isspace(*start_ptr)){
                start_ptr++;
            }
            tab_ptr = strchr(start_ptr, ' ');
            if(tab_ptr != NULL)
                printerr("too many fields!");
            tab_ptr = strchr(start_ptr, '\t');
            if(tab_ptr != NULL)
                printerr("too many fields!");
            tab_ptr = strchr(start_ptr, '\n');
            if (tab_ptr != NULL)
                *tab_ptr++ = '\0';
            temp->service_time = atof(start_ptr);    // service time in milliseconds
            
            pkt_id++;
            My402ListAppend(&packet_list, (void *) temp);
        }
        return TRUE;
    }
    return FALSE;
}

void print_stats(){
    char output [1024];
    sprintf(output, "emulation ends\n");
    print_context(output, time_elapse(start_program, end_program)/1000.0);
    
    //all time below is in usec
    double total_time_in_system = 0.0, total_time_in_q1 = 0.0, total_time_in_q2 = 0.0, total_time_in_s1 = 0.0, total_time_in_s2 = 0.0, total_service_time, total_emulation_time;
    int num_completed_pkt = My402ListLength(&packet_recycle);
    int total_num_pkt = num_pkt_dropped + num_pkt_removed + num_completed_pkt;
    
    My402ListElem * elem = NULL;
    for(elem = My402ListFirst(&packet_recycle); elem != NULL; elem = My402ListNext(&packet_recycle, elem)){
        pkt * packet = (pkt *) elem->obj;
        total_time_in_system += packet->end_s_time - packet->arrive_time;
        total_time_in_q1 += packet->leave_q1_time - packet->enter_q1_time;
        total_time_in_q2 += packet->leave_q2_time - packet->enter_q2_time;
        if(packet->which_server == 1){
            total_time_in_s1 += packet->end_s_time - packet->start_s_time;
        }
        else if(packet->which_server == 2){
            total_time_in_s2 += packet->end_s_time - packet->start_s_time;
        }
        //free(packet);
    }
    
    
    
    total_service_time = total_time_in_s1 + total_time_in_s2;
    total_emulation_time = time_elapse(start_program, end_program);
    double average_time_in_sys = (total_time_in_system/num_completed_pkt)/1000000.0;
    double variance = 0;
    My402ListElem * elemt = NULL;
    for(elemt = My402ListFirst(&packet_recycle); elemt != NULL; elemt = My402ListNext(&packet_recycle, elemt)){
        pkt * packet = (pkt *) elemt->obj;
        double diff = (packet->end_s_time - packet->arrive_time)/1000000.0 - average_time_in_sys;
        variance += diff * diff;
        //free(packet);
    }
    
    My402ListUnlinkAll(&packet_recycle);
    
    fprintf(stdout, "    Statistics:\n\n");
    fprintf(stdout, "        average packet internal-arrival time = %.6g\n", (total_internal_arrival_time/total_num_pkt)/1000000.0);
    
    if(num_completed_pkt){
        fprintf(stdout, "        average packet service time = %.6g\n\n", (total_service_time/num_completed_pkt)/1000000.0);
    }
    else{
        fprintf(stdout, "        average packet service time = N/A no packet completed service at this facility\n");
    }
    
    fprintf(stdout, "        average number of packets in Q1 = %.6g\n", total_time_in_q1/total_emulation_time);
    fprintf(stdout, "        average number of packets in Q2 = %.6g\n", total_time_in_q2/total_emulation_time);
    fprintf(stdout, "        average number of packets in S1 = %.6g\n", total_time_in_s1/total_emulation_time);
    fprintf(stdout, "        average number of packets in S2 = %.6g\n\n", total_time_in_s2/total_emulation_time);
    
    if(num_completed_pkt){
        fprintf(stdout, "        average time a packet spent in system = %.6g\n", average_time_in_sys);
    }
    else{
        fprintf(stdout, "        average time a packet spent in system = N/A no packet completed service at this facility\n");
    }
    
    fprintf(stdout, "        standard deviation for time spent in system = %.6g\n\n", sqrt(variance/num_completed_pkt));
    
    if(num_total_token){
        fprintf(stdout, "        token drop probability = %.6g\n", 1.0*num_token_dropped/num_total_token);
    }
    else{
        fprintf(stdout, "        token drop probability = N/A no token was deposied at this facility\n");
    }
    
    if(total_num_pkt){
        fprintf(stdout, "        packet drop probability = %.6g\n\n", 1.0*num_pkt_dropped/total_num_pkt);
    }
    else{
        fprintf(stdout, "        packet drop probability = N/A no packet arrived at this facility\n");
    }
    
}

void * packet_arrival(void * arg){
    My402ListElem * elem = NULL;
    struct timeval cur_time = start_program, last_arrive_time = start_program;
    double sleep_time;
    int token_needed;
    char output[1024];
    
    for(elem = My402ListFirst(&packet_list); elem != NULL; elem = My402ListNext(&packet_list, elem)){
        pkt * packet =  (pkt *) elem->obj;
        token_needed = packet->num_token_needed;
        
        /********************** sleep ********************/
        gettimeofday(&cur_time, NULL);
        if(packet->packet_id==1)
            gettimeofday(&last_arrive_time, NULL);
        
        sleep_time = packet->internal_arrival_time*1000 - time_elapse(cur_time, last_arrive_time);
        if(sleep_time < 0){
            sleep_time = 0;
        }
        usleep(sleep_time);
        
        /********************** wake up ******************/
        
        pthread_mutex_lock(&mutex);
        
        gettimeofday(&last_arrive_time, NULL);
        packet->arrive_time = time_elapse(start_program, last_arrive_time);
        
        if(token_needed > argmnts->B){
            num_pkt_dropped++;
            sprintf(output, "p%ld arrives, needs %d tokens, internal-arrival time = %.3fms, dropped", packet->packet_id, token_needed, time_elapse(cur_time, last_arrive_time)/1000.0);
            print_context(output, time_elapse(start_program, last_arrive_time)/1000.0);
            total_internal_arrival_time += time_elapse(cur_time, last_arrive_time);
            pthread_mutex_unlock(&mutex);
            continue;
        }
        
        sprintf(output, "p%ld arrives, needs %d tokens, internal-arrival time = %.3fms", packet->packet_id, token_needed, (double)time_elapse(cur_time, last_arrive_time)/1000.0);
        print_context(output, time_elapse(start_program, last_arrive_time)/1000.0);
        total_internal_arrival_time += time_elapse(cur_time, last_arrive_time);
        
        /* enqueues the packet to Q1 and timestamp the packet */
        My402ListAppend(&Q1, (void *)packet);
        struct timeval time_enter_q1;
        gettimeofday(&time_enter_q1, NULL);
        packet->enter_q1_time = time_elapse(start_program, time_enter_q1);
        sprintf(output, "p%ld enters Q1", packet->packet_id);
        print_context(output, packet->enter_q1_time/1000.0);
        
        /* moves the first packet in Q1 into Q2 if possible */
        My402ListElem * first_in_q1 = My402ListFirst(&Q1);
        pkt * first_pkt = (pkt*) first_in_q1->obj;
        
        if(num_token >= first_pkt->num_token_needed){
            My402ListUnlink(&Q1, first_in_q1);
            struct timeval time_leave_q1;
            gettimeofday(&time_leave_q1, NULL);
            first_pkt->leave_q1_time = time_elapse(start_program, time_leave_q1);
            sprintf(output, "p%ld leaves Q1, time in Q1 = %fms, token bucket now has %d token", first_pkt->packet_id, time_elapse(time_enter_q1, time_leave_q1)/1000.0, num_token);
            print_context(output, first_pkt->leave_q1_time/1000.0);
            num_token -= token_needed;
            int empty_before = My402ListEmpty(&Q2);
            
            My402ListAppend(&Q2, (void *)first_pkt);
            struct timeval time_enter_q2;
            gettimeofday(&time_enter_q2, NULL);
            q2_num_pkt++;
            
            /*if Q2 was empty before, need to signal or broadcast a queue-not-empty condition */
            if(!My402ListEmpty(&Q2) && empty_before)
                pthread_cond_broadcast(&wakeup_server);
            
            first_pkt->enter_q2_time = time_elapse(start_program, time_enter_q2);
            sprintf(output, "p%ld enters Q2", first_pkt->packet_id);
            print_context(output, first_pkt->enter_q2_time/1000.0);
            
        }
        pthread_mutex_unlock(&mutex);
    }
    pthread_mutex_lock(&mutex);
    My402ListUnlinkAll(&packet_list);
    
    if(My402ListEmpty(&Q1) && My402ListEmpty(&packet_list) && My402ListEmpty(&Q2) ){
        gettimeofday(&end_program, NULL);
        
        terminate_flag = TRUE;
        q2_num_pkt++;                   // to help terminate another server thread
        pthread_cond_broadcast(&wakeup_server);
    }
    
    pthread_mutex_unlock(&mutex);
    
    return 0;
}

void * token_deposit(void * arg){
    struct timeval arrive_time;
    int token_id = 0;
    double token_interval = format_input(1/argmnts->r)*1000;      //interval in millisecond
    char output[1024];
    
    while(1){
        token_id++;
        
        /********************** sleep ********************/
        usleep(token_interval);
        
        /********************** wake up ******************/
        pthread_mutex_lock(&mutex);
        gettimeofday(&arrive_time, NULL);
        num_total_token++;
        
        if(My402ListEmpty(&Q1) && My402ListEmpty(&packet_list) && My402ListEmpty(&Q2)){
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        /************ drop a token when the bucketlist is full ************/
        if(num_token+1 > argmnts->B){
            num_token_dropped++;
            sprintf(output, "token t%d arrives, dropped", token_id);
            print_context(output, time_elapse(start_program, arrive_time)/1000.0);
            pthread_mutex_unlock(&mutex);
            continue;
        }
        
        num_token++;
        sprintf(output, "token t%d arrives, token bucket now has %d tokens", token_id, num_token);
        print_context(output, time_elapse(start_program, arrive_time)/1000.0);
        
        /* check if it can move first pkt in Q1 to Q2 */
        while(!My402ListEmpty(&Q1)){
            My402ListElem *first_q1 = My402ListFirst(&Q1);
            pkt *first_pkt_q1 = (pkt *)first_q1->obj;
            if(num_token >= first_pkt_q1->num_token_needed){
                num_token -= first_pkt_q1->num_token_needed;
                
                My402ListUnlink(&Q1, first_q1);
                struct timeval time_leave_q1;
                gettimeofday(&time_leave_q1, NULL);
                first_pkt_q1->leave_q1_time = time_elapse(start_program, time_leave_q1);
                sprintf(output, "p%ld leaves Q1, time in Q1 = %.3fms, token bucket now has %d token", first_pkt_q1->packet_id, (first_pkt_q1->leave_q1_time-first_pkt_q1->enter_q1_time)/1000.0, num_token);
                print_context(output, first_pkt_q1->leave_q1_time/1000.0);
                
                /* check if Q2 was empty before the new packet was appended */
                int q2_empty_before = My402ListEmpty(&Q2);
                
                My402ListAppend(&Q2, (void *)first_pkt_q1);
                struct timeval time_enter_q2;
                gettimeofday(&time_enter_q2, NULL);
                q2_num_pkt++;
                
                if(q2_empty_before)
                    pthread_cond_broadcast(&wakeup_server);
                
                first_pkt_q1->enter_q2_time = time_elapse(start_program, time_enter_q2);
                sprintf(output, "p%ld enters Q2", first_pkt_q1->packet_id);
                print_context(output, first_pkt_q1->enter_q2_time/1000.0);
            }
            else{
                break;
            }
        }
        
        pthread_mutex_unlock(&mutex);
    }
    
    return 0;
}

void * server(void * arg){
    int server_num = (int) arg;
    char output[1024];
    
    while(1){
        pthread_mutex_lock(&mutex);
        while(!q2_num_pkt){
            pthread_cond_wait(&wakeup_server, &mutex);
        }
        if(terminate_flag || control_c){
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        if(!My402ListEmpty(&Q2)){
            My402ListElem * first_q2 = My402ListFirst(&Q2);
            pkt * first_pkt_q2 = (pkt *) first_q2->obj;
            My402ListUnlink(&Q2, first_q2);
            q2_num_pkt--;
            
            struct timeval time_leave_q2;
            gettimeofday(&time_leave_q2, NULL);
            first_pkt_q2->leave_q2_time = time_elapse(start_program, time_leave_q2);
            sprintf(output, "p%ld leaves Q2, time in Q2 = %.3fms", first_pkt_q2->packet_id, (first_pkt_q2->leave_q2_time-first_pkt_q2->enter_q2_time)/1000.0);
            print_context(output, first_pkt_q2->leave_q2_time/1000.0);
            
            double time_to_sleep = first_pkt_q2->service_time * 1000;
            
            struct timeval time_begin_service;
            gettimeofday(&time_begin_service, NULL);
            first_pkt_q2->start_s_time = time_elapse(start_program, time_begin_service);
            sprintf(output, "p%ld begins service at S%d, requesting %dms of service", first_pkt_q2->packet_id, server_num, (int)(time_to_sleep/1000.0));
            print_context(output, time_elapse(start_program, time_begin_service)/1000.0);
            
            first_pkt_q2->which_server = server_num;
            
            pthread_mutex_unlock(&mutex);
            
            usleep(time_to_sleep);
            
            pthread_mutex_lock(&mutex);
            struct timeval time_leave_s;
            gettimeofday(&time_leave_s, NULL);
            first_pkt_q2->end_s_time = time_elapse(start_program, time_leave_s);
            sprintf(output, "p%ld departs from S%d, service time = %.3fms, time in system = %.3fms", first_pkt_q2->packet_id, server_num, (first_pkt_q2->end_s_time - first_pkt_q2->start_s_time)/1000.0, (first_pkt_q2->end_s_time - first_pkt_q2->arrive_time)/1000.0);
            print_context(output, time_elapse(start_program, time_leave_s)/1000.0);
            My402ListAppend(&packet_recycle, (void *)first_pkt_q2);
            
            if(My402ListEmpty(&Q1) && My402ListEmpty(&packet_list) && My402ListEmpty(&Q2) ){
                gettimeofday(&end_program, NULL);
                
                terminate_flag = TRUE;
                q2_num_pkt++;                   // to help terminate another server thread
                pthread_cond_broadcast(&wakeup_server);
                pthread_mutex_unlock(&mutex);
                break;
            }
            
            pthread_mutex_unlock(&mutex);
            
            
        }
        else{
            pthread_mutex_unlock(&mutex);
        }
        
        //if(control_c)
        //    break;
        
    }
    return 0;
}

void * monitor(void * arg){
    int sig = 0;
    int pkt_left_in_q1;
    int pkt_left_in_q2;
    char output [1024];
    
    sigwait(&set, &sig);
    control_c = TRUE;
    pthread_cancel(packet_arrival_t);
    pthread_cancel(token_arrival_t);
    
    pthread_mutex_lock(&mutex);
    
    control_c = TRUE;
    
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    fprintf(stdout, "\n");
    sprintf(output, "SIGINT caught, no new packets or tokens will be allowed");
    print_context(output, time_elapse(start_program, cur_time)/1000.0);
    //fprintf(stdout, "control-c success!\n");
    
    pkt_left_in_q1 = My402ListLength(&Q1);
    pkt_left_in_q2 = My402ListLength(&Q2);
    num_pkt_removed = pkt_left_in_q1 + pkt_left_in_q2;
    
    My402ListElem * elem = NULL;
    
    while(!My402ListEmpty(&Q1)){
        elem = My402ListFirst(&Q1);
        pkt * temp = (pkt*) elem->obj;
        My402ListUnlink(&Q1, elem);
        struct timeval cur_time;
        gettimeofday(&cur_time, NULL);
        sprintf(output, "p%ld removed from Q1", temp->packet_id);
        print_context(output, time_elapse(start_program, cur_time)/1000.0);
    }
    
    while(!My402ListEmpty(&Q2)){
        elem = My402ListFirst(&Q2);
        pkt * temp = (pkt*) elem->obj;
        My402ListUnlink(&Q2, elem);
        struct timeval cur_time;
        gettimeofday(&cur_time, NULL);
        sprintf(output, "p%ld removed from Q2", temp->packet_id);
        print_context(output, time_elapse(start_program, cur_time)/1000.0);
    }
    
    
    if(!My402ListEmpty(&packet_list)){
        My402ListUnlinkAll(&packet_list);
    }
    
    
    q2_num_pkt ++;       //help to terminate the server_guard
    pthread_cond_broadcast(&wakeup_server);
    gettimeofday(&end_program, NULL);
    
    pthread_mutex_unlock(&mutex);
    
    return 0;
}

void * too_many_packets(void * arg){
    int num = 0, i;
    double interval = 1000;
    pkt * temp1;

    while(num_left > 0 )
    {
        if(num_left >MAX_NUM)
        {
            num = MAX_NUM;
            num_left -= MAX_NUM;
        }
        else
        {
            num = num_left;
            num_left = 0;
        }
        pthread_mutex_lock(&mutex);
        for(i = 0; i < num; i++)
        {
            temp1 = (pkt*)malloc(sizeof(pkt));
            if(!temp1){
                continue;
            }
            temp1->internal_arrival_time = format_input(1.0/(double)argmnts->lambda);
            
            temp1->service_time = format_input(1/argmnts->mu);
            temp1->num_token_needed = argmnts->P;
            temp1->packet_id = num_left;
            My402ListAppend(&packet_list, (void *)temp1);
        }
        pthread_mutex_unlock(&mutex);
        usleep(interval);
    }
    return 0;
}

void initiate_threads(){
    gettimeofday(&start_program, NULL);         //get program start timestamp
    print_parameter();
    fprintf(stdout, "\n");
    print_context("emulation begins",0);
    
    
    if(!My402ListInit(&packet_recycle))
        perror("Cannot initiate packet recycle list!");
    if(!My402ListInit(&Q1))
        perror("Cannot initiate Q1 list!");
    if(!My402ListInit(&Q2))
        perror("Cannot initiate Q2 list!");
    
    /****** To use sigwait() --- no signal handler ******/
    
    pthread_t signal_t;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, 0);
    
    if(pthread_create(&signal_t, 0, monitor, 0))
        printerr("fail to create signal control thread");
    
    if(pthread_create(&packet_arrival_t, 0, packet_arrival, 0))
        printerr("fail to create packet arrival thread");
    if(pthread_create(&token_arrival_t, 0, token_deposit, 0))
        printerr("fail to create token arrival thread");
    if(pthread_create(&server1_t, 0, server, (void*)1))
        printerr("fail to create server 1 thread");
    if(pthread_create(&server_2_t, 0, server, (void*)2))
        printerr("fail to create server 2 thread");
    if(pthread_create(&deal_with_large_num_pkt, 0, too_many_packets, 0))
        printerr("fail to create server 2 thread");
    
    pthread_join(packet_arrival_t, 0);
    pthread_join(token_arrival_t, 0);
    pthread_join(server1_t, 0);
    pthread_join(server_2_t, 0);
    pthread_join(deal_with_large_num_pkt, 0);
    print_stats();
}

int main(int argc, char * argv[]) {
    fileInput(argc, argv);
    initiate_threads();
    return 0;
}

