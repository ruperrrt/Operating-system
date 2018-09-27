#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>

#include "cs402.h"

#include "my402list.h"

typedef struct tagTrans{
    char type;
    unsigned int timestamp;
    int amount;
    char descrp [25];
}trans;

int lineNum = 0;

void printerr (char * error){
    fprintf(stderr, "Error: input file is not in the right format. %s Error at Line:%d\n", error, lineNum);
    exit(-1);
}

void print_command_err (char * error){
    fprintf(stderr, "Error: %s\n", error);
    exit(-1);
}

int arguments (int argc, char *argv[]){
    int specifyfile = 0;
    if(argc == 2 || argc == 3){
        if(!strcmp(argv[1], "sort")){
            if(argc==3)
                specifyfile = 1;
        }
        else
            print_command_err("Malformed command! argument not found. Usage: warmup1 sort [tfile]");
    }
    else
        print_command_err("Malformed command! Usage: warmup1 sort [tfile]");
    return specifyfile;
}

void checkType (char str[]){
    if(strlen(str)!=1 || (str[0]!='+'&&str[0]!='-'))
        printerr("Invalid type input! Type must be + or -");
}

void checkTime (char str[]){
    if(strlen(str)>=11)
        printerr("Invalid timestamp length!");
    int i = 0;
    while(str[i]!='\0'){
        if(str[i]>'9'||str[i]<'0')
            printerr("Invalid timestamp! Timestamp must be a number");
        i++;
    }
    unsigned int curtime;
    curtime = (int)time(NULL);
    if(atoi(str) > curtime)
        printerr("Invalid timestamp! Timestamp cannot exceed current time");
}

void checkAmount (char str[]){
    if(str[0]=='-')
        printerr("Invalid amount! Amount must be positive");
    int i = 0;
    while(str[i]!='.'){
        if(str[i]>'9'||str[i]<'0'){
            printerr("Invalid timestamp! Amount must be a number");
        }
        i++;
    }
    if(i > 7)
        printerr("Invalid amount! Amount must be < 10,000,000");
    if(str[i+1]>'9'||str[i+1]<'0'||str[i+2]>'9'||str[i+2]<'0'||str[i+3]!='\0'){
        printerr("Invalid timestamp! Amount must be a number with 2 decimals");
    }
}

/* return the num of spaces in the front*/
void checkDescri(char str[]){
    if(strlen(str)==0)
        printerr("Invalid description! a transaction description must not be empty");
}

int readInput(FILE *fp, My402List *list){
    char buf[1026];
    while(1){
        trans* temp=(trans*)malloc(sizeof(trans));
        if (fgets(buf, sizeof(buf), fp) == NULL){
            if(!list->num_members){
                printerr("The file contains no transactions!");
            }
            return TRUE;
        }
        else{
            lineNum++;
            
            /* parse type*/
            char *start_ptr = buf;
            char *tab_ptr = strchr(start_ptr, '\t');
            if(tab_ptr != NULL)
                *tab_ptr++ = '\0';
            checkType(start_ptr);
            temp->type = start_ptr[0];
            
            /*parse timestamp*/
            start_ptr = tab_ptr;
            tab_ptr = strchr(start_ptr, '\t');
            if (tab_ptr != NULL)
                *tab_ptr++ = '\0';
            checkTime(start_ptr);
            temp->timestamp = atoi(start_ptr);
            
            /*parse amount*/
            start_ptr = tab_ptr;
            tab_ptr = strchr(start_ptr, '\t');
            if (tab_ptr != NULL)
                *tab_ptr++ = '\0';
            checkAmount(start_ptr);
            int amount_len = (int) strlen(start_ptr);
            int idxToDel = amount_len - 3;
            memmove(&start_ptr[idxToDel], &start_ptr[idxToDel + 1], strlen(start_ptr) - idxToDel);
            int amount = atoi(start_ptr);
            temp->amount = amount;
            
            /*parse description*/
            start_ptr = tab_ptr;
            tab_ptr = strchr(start_ptr, '\t');
            if(tab_ptr != NULL)
                printerr("Too many fields!");
            tab_ptr = strchr(start_ptr, '\n');
            if (tab_ptr != NULL)
                *tab_ptr++ = '\0';
            while(*start_ptr && isspace(*start_ptr))
                start_ptr++;
            checkDescri(start_ptr);
            int k = (int) strlen(start_ptr);
            while(k<24){
                start_ptr[k] = ' ';
                k++;
            }
            start_ptr[24] = '\0';             // truncate the description string
            strcpy(temp->descrp, start_ptr);
            
            My402ListAppend(list, (void *)temp);
        }
    }
    return TRUE;
}

void PrintLine(){
    fprintf(stdout, "+-----------------+--------------------------+----------------+----------------+\n");
}

void PrintTitle(){
    fprintf(stdout, "|       Date      | Description              |         Amount |        Balance |\n");
}

void PrintDate(trans * trans){
    time_t t = (time_t) trans->timestamp;
    char format[] = "%a %b %d %Y";
    struct tm lt;
    char res[32];
    (void) localtime_r(&t, &lt);
    if (strftime(res, sizeof(res), format, &lt) == 0) {
        printerr("failed to transcript timestamp!");
    }
    if (res[8]=='0') res[8]=' ';
    fprintf(stdout, "| %s ",  res);
}

void PrintDes(trans * trans){
    fprintf(stdout, "| %s ",  trans->descrp);
}

void PrintNumber(int num, int pos){
    char des[13];
    if(num>=10000000){
        strcpy(des, "?,???,???.??");
    }
    char src[13];
    snprintf(des, 13, "            ");
    des[9] = '.';
    sprintf(src, "%d", num);
    int len = (int) strlen(src);
    des[11] = src[len-1];
    if(len==1){
        des[10] = '0';
        des[8] = '0';
    }
    if(len==2){
        des[10] = src[len-2];
        des[8] = '0';
    }
    des[10] = src[len-2];
    int i = len-2;
    int count = 0;
    while(i>0){
        if(count==3 || count==7){
            des[8-count] = ',';
            count++;
            continue;
        }
        des[8-count] = src[i-1];
        i--;
        count++;
    }
    if(pos)
        fprintf(stdout, "|  %s  ",  des);
    else
        fprintf(stdout, "| (%s) ",  des);
}

void PrintAmount(trans * trans){
    if (trans->type=='+') {
        PrintNumber(trans->amount, 1);
    }
    else{
        PrintNumber(trans->amount, 0);
    }
}

void PrintList(My402List *pList, int num_items)
{
    My402ListElem *elem=NULL;
    int balance = 0;
    
    if (My402ListLength(pList) != num_items) {
        fprintf(stderr, "List length is not %1d in PrintTestList().\n", num_items);
        exit(1);
    }
    PrintLine();
    PrintTitle();
    PrintLine();
    for (elem=My402ListFirst(pList); elem != NULL; elem=My402ListNext(pList, elem)) {
        trans *res = (trans *)(elem->obj);
        
        if (res->type=='+')
            balance += res->amount;
        else
            balance -= res->amount;
        
        //fprintf(stdout, "%d\n", res->amount);
        //fprintf(stdout, "%c\n", ival->type);
        PrintDate(res);
        PrintDes(res);
        PrintAmount(res);
        
        if(balance>=0)
            PrintNumber(balance, 1);
        else{
            balance = 0 - balance;
            PrintNumber(balance, 0);
            balance = 0 - balance;
        }
        fprintf(stdout, "|\n");
    }
    PrintLine();
}

void SortInput(My402List *list){
    My402ListElem * elem, * cur;
    trans * trans1, * trans2;
    int i,j;
    elem = My402ListFirst(list);
    for(i=0; i<list->num_members; i++){
        cur = elem;
        for(j=i+1; j<list->num_members; j++){
            cur = cur->next;
            trans1 = (trans*) elem->obj;
            trans2 = (trans*) cur->obj;
            if(trans1->timestamp==trans2->timestamp)
                printerr("There cannot be two identical timestamps!");
            else if (trans1->timestamp > trans2->timestamp){
                elem->obj = trans2;
                cur->obj = trans1;
            }
        }
        elem = elem->next;
    }
}

void process (char *fileName){
    FILE *fp;
    if(!fileName)
        fp = stdin;
    else{
        struct stat info;
        stat(fileName, &info);
        if(S_ISDIR(info.st_mode))
            print_command_err("the input path is a directory!");
        fp = fopen(fileName, "r");
        if(fp==NULL){
            //printf("%s",strerror(errno));
            print_command_err("The input file does not exist or cannot be opened!");
        }
    }
    My402List list;
    if (!My402ListInit(&list))
        print_command_err("Cannot initiate linkedlist");
    if(!readInput(fp, &list)){
        print_command_err("Cannot read in the file");
    }
    if (fp != stdin) fclose(fp);
    SortInput(&list);
    PrintList(&list, My402ListLength(&list));
    
}

int main (int argc, char *argv[]){
    if (arguments(argc, argv))
        process(argv[2]);
    else
        process(NULL);
    return 0;
}
