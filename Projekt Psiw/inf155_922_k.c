#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>

struct msgbuf{ // bufor na wiadomości kolejki komunikatów
    long type;
    char text[1024];
    int pid;
    int prior;
};

struct messages{ // wiadomość używana w tablicy zapisanych wiadomości w przypadku odbioru asychronicznego
    char text[1024];
    int prior, displayed, expire;
};

int display(int saved_mess, struct messages mess[]){
    for(int j = 0; j < saved_mess; j++){
        int min = 10000;
        int min_i;
        for(int i = 0; i < saved_mess; i++){ // szukanie wiadomości o najniższym priorytecie
            if(mess[i].displayed == 0 && mess[i].prior < min){
                min_i = i;
                min = mess[i].prior;
            }
        }
        mess[min_i].displayed = 1; // zapamiętanie, że została wyświetlona
        printf("%s\n", mess[min_i].text); // wyświetlenie jej
    }
    return 1;
};

int main(int argc, char* argv[]){
    int key = getpid();
    int idc = msgget(key, 0666 | IPC_CREAT);
    int ids = msgget(0x234, 0666 | IPC_CREAT);

    if(atoi(argv[1]) == 1){
        if (fork() == 0){
            while(1){
                char buf[1024] = "";
                int bytes = read(0, buf, 1024);
                struct msgbuf msg;
                msg.type = atoi(&buf[0]);
                msg.pid = key;
                for(int i = 0; i < bytes - 2; i++){
                    msg.text[i] = buf[i+2];
                }
                msgsnd(ids, &msg, sizeof(struct msgbuf) - sizeof(long), 0);
            }
        } else {
            int blocked[10] = {0};
            while(1){
                char buff[1024];
                struct msgbuf msg;
                msgrcv(idc, &msg, sizeof(struct msgbuf) - sizeof(long), 0, 0);
                switch (msg.type){
                    case 100:
                        printf("%s\n", msg.text);
                        break;
                    case 12:
                        if(blocked[atoi(msg.text)] == 0){
                            blocked[atoi(msg.text)] = 1;
                            printf("%s\n", "Zablokowano użytkownika");
                        } else {
                            blocked[atoi(msg.text)] = 0;
                            printf("%s\n", "Odblokowano użytkownika");
                        }
                        break;
                    default:
                        if(blocked[msg.type - 1] == 0){
                            printf("%s\n", msg.text);
                        }
                        break;
                }
            }
        }
    } else {
        if (fork() == 0){ // kod procesu potomnego odpowiedzialnego za odbiór wejścia i wysłyłanie poleceń
            while(1){
                char buf[1024] = "";
                int bytes = read(0, buf, 1024);
                struct msgbuf msg;
                msg.type = atoi(&buf[0]);
                msg.pid = key;
                for(int i = 0; i < bytes - 2; i++){
                    msg.text[i] = buf[i+2];
                }
                msgsnd(ids, &msg, sizeof(struct msgbuf) - sizeof(long), 0);
            }
        } else {
            int blocked[10] = {0};
            struct messages mess[100];
            int saved_mess = 0;
            while(1){
                char buff[1024];
                struct msgbuf msg;
                msgrcv(idc, &msg, sizeof(struct msgbuf) - sizeof(long), 0, 0);
                switch (msg.type){
                    case 100:
                        if (strcmp(msg.text, "Subskrypcja powyższego tematu zakończyła się\n") != 0) printf("%s\n", msg.text); // ta informacja dotyczy tylko odbioru synchronicznego
                        break;
                    case 12:
                        if(blocked[atoi(msg.text)] == 0){
                            blocked[atoi(msg.text)] = 1;
                            printf("%s\n", "Zablokowano użytkownika");
                        } else {
                            blocked[atoi(msg.text)] = 0;
                            printf("%s\n", "Odblokowano użytkownika");
                        }
                        break;
                    case 50:
                        display(saved_mess, mess);
                        for(int i = 0; i < saved_mess; i++){
                            strcpy(mess[i].text, "");
                            mess[i].displayed = 0;
                        }
                        saved_mess = 0;
                        break;
                    default:
                        if(blocked[msg.type - 1] == 0){ // wyświetlanie jeśli użytkownik nie jest zablokowany
                            strcpy(mess[saved_mess].text, msg.text);
                            mess[saved_mess].prior = msg.prior;
                            saved_mess++;
                        }
                        break;
                }
            }
        }
    }
}