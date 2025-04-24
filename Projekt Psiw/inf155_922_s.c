#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

struct msgbuf{ // struktura wiadomości w kolejce komunikatów
    long type;
    char text[1024];
    int pid;
    int prior;
};

struct info{ // informacja na temat uzytkownika który subskrybuje dany temat
    int id;
    int how_long;
};

struct name{ // nazwa uzytkownika
    char name[10];
};

struct topic{ // struktura tematu
    struct info użytkownicy[10];
    int created;
};

//zmienne i struktury globalne na których działamy
int pipes[10]; // tablica przechowująca id kolejek komunikatów poszczególnych klientów
struct name names[10]; // tablica przechowująca nazwy użytkowników
struct topic topics[10]; // tablica, przechowująca tematy oraz informacje o użytkownikach, którzy je subskrybują
int pid[10] = {-1}; // tablica zawierająca pid użytkowników


int login(int p_id, int idc, char name[]){ // funkcja logująca użytkownika
    int i = 0;
    while(pipes[i] != -1){ // sprawdzanie czy nazwa się nie powtarza
        if(strcmp(names[i].name, name) == 0){
            return -1;
        }
        i++;
    }

    //przypisywanie informacji o kliencie
    pipes[i] = idc;
    strcpy(names[i].name, name);
    pid[i] = p_id;
    return 1;
};

int sub(int id, int topicnr, int how_long){ // funkcja subskrybująca temat dla danego użytkownika
    int issub = 0;
        if(topics[topicnr].użytkownicy[id].id == id){ // jeśli użytkownik już sukskrybował
            issub == 1;
            if(topics[topicnr].użytkownicy[id].how_long == -1) return -1; // jeśli temat został już zasubskrybowany w trybie trwałym
            else {
                if(how_long == -1) { // zmiana subskrybcji z przejściowej na trwałą
                    topics[topicnr].użytkownicy[id].how_long = -1;
                    return 1;
                } 
                else { // przedłużenie subskrybcji
                    topics[topicnr].użytkownicy[id].how_long += how_long;
                    return topics[topicnr].użytkownicy[id].how_long;
                }
            }
        }
    
    if(issub == 0){ // jeśli użytkownik nie subskrybował
        topics[topicnr].użytkownicy[id].id = id;
        topics[topicnr].użytkownicy[id].how_long = how_long;
    }
    return 1;
};

int new(int pid, int newtopic){ // funckja tworząca nowy temat
    if(topics[newtopic - 1].created == 0){ // jeśli temat nie isnieje
        topics[newtopic - 1].created = 1;
        return 1;
    }
    else return -1;
};

void sendtoall(char text[]){ // wysyłanie wiadomośći do wszystkich użytkowników
    struct msgbuf msg1;
    msg1.type = 100;
    strcpy(msg1.text, text);
    int i = 0;
    while(pipes[i] != -1){
        msgsnd(pipes[i], &msg1, sizeof(struct msgbuf) - sizeof(long), IPC_NOWAIT);
        i++;
    }
    return;
};

void sendto(int idc, char text[]){ // wysyłanie wiadomosci do konkretnego użytkownika
    struct msgbuf msg1;
    msg1.type = 100;
    strcpy(msg1.text, text);
    msgsnd(idc, &msg1, sizeof(struct msgbuf) - sizeof(long), IPC_NOWAIT);
    return;
};

int id(int p_id){ // funkcja zwracająca id użytkownika podając pid procesu
    int i = 0;
    while(pid[i] != p_id) i++;
    if(i <= 9) return i;
    return -1;
};

int message(int p_id, int type, int prior, char text[]){ // wysyłanie wiadomości na dany temat
    if(topics[type - 1].created == 0) return -1; // temat nie istnieje
    int carry = 1;
    for(int i = 0; i < 10; i++){ // sprawdzanie czy użytkownik subskrybuje dany temat
        if(topics[type - 1].użytkownicy[i].id == id(p_id)){
            carry = 0;
            break;
        }
    }
    if(carry == 0){
        char nazwa[10] = "";
        strcpy(nazwa, names[id(p_id)].name);
        struct msgbuf msg;
        msg.type = id(p_id) + 1;
        msg.prior = prior;
        sprintf(msg.text, "%s%s%d%s%s\n", nazwa, " w temacie ", type, ": ", text);
        for(int i = 0; i < 10; i++){ // wysyłanie do odpowiednich użytkowników
            int id = topics[type - 1].użytkownicy[i].id;
            if(id != -1 && pid[id] != p_id){
                msgsnd(pipes[id], &msg, sizeof(struct msgbuf) - sizeof(long), IPC_NOWAIT);
                if(topics[type - 1].użytkownicy[i].how_long != -1){ // how_long == -1 oznacza sub. trwałą
                    topics[type - 1].użytkownicy[i].how_long -= 1;
                    if(topics[type - 1].użytkownicy[i].how_long == 0){
                        topics[type - 1].użytkownicy[i].id = -1; // id == -1 oznacza brak subskrypcji
                        sendto(pipes[id], "Subskrypcja powyższego tematu zakończyła się\n");
                    }
                };
            }
        }
        return 1; // powodzenie
    } else return -2;  // użytkownik nie subskrybuje

};

int main(){
    for (int i = 0; i < 10; i++) { // inicjalizacja zmiennych
        topics[i].created = 0;
        for(int j = 0; j < 10; j++){
            topics[i].użytkownicy[j].id = -1;
            topics[i].użytkownicy[j].how_long = 0;
        } 
        pipes[i] = -1;
        pid[i] = -1;
        strcpy(names[i].name, "");
    }
    int ids = msgget(0x234, 0666 | IPC_CREAT);
    while(1){
        struct msgbuf msg;
        int size = sizeof(struct msgbuf) - sizeof(long); //rozmiar wiadomości
        msgrcv(ids, &msg, size, 0, 0);
        char text[1024] = "";
        int carry = 0;
        char nazwa[10] = "";
        switch (msg.type){ // serwer podejmuje różne działania w zależności od typu wiadomości
            case 1: // logowanie
                for(int i = 0; i < 10; i++){ // sprawdzanie czy użytkownik czasami już się nie zalogował
                    if(pid[i] == msg.pid){
                        sendto(pipes[i], "Już jesteś zalogowany");
                        carry = 1;
                    }
                }
                if(carry == 0){ 
                    int idc = msgget(msg.pid, 0666 | IPC_CREAT); // wyłuskanie id kolejki klienta
                    sscanf(msg.text, "%10[^\n]", nazwa); // wyłoskanie nazwy
                    int a = login(msg.pid, idc, nazwa); // zarejestrowanie klienta w serwerze
                    if(a == -1){ // obsługa błędu
                        strcpy(text,"Logowanie nie powiodło się - ta nazwa juz istnieje\n");
                        sendto(idc, text);
                    }else if(a == 1){ // powodzenie
                        strcpy(text,"Logowanie powiodło się\n");
                        sendto(pipes[id(msg.pid)], text);
                    }
                }
                break;
            case 2: // subskrybcja
                carry = 1;
                for(int i = 0; i < 10; i++){ // sprawdzanie czy użytkownik jest zalogowany
                    if(pid[i] == msg.pid){
                        carry = 0;
                    }
                }
                if(carry == 0){
                    int topicnr;
                    int how;
                    sscanf(msg.text, "%d%d", &topicnr, &how);
                    if(topics[topicnr - 1].created == 0) { // sprawdzanie czy temat istnieje
                        sendto(pipes[id(msg.pid)], "Podany temat nie istnieje\n");
                    } else {
                        int how_long = -1;
                        if(how == 2) sscanf(msg.text, "%*d%*d%d", &how_long); // how_long domyślnie -1, chyba że how == 2, to przypisujemy wartość z wejścia
                        int c = sub(id(msg.pid), topicnr - 1, how_long);
                        if(c == 1 ){ // pomyślna subskrybcja
                            strcpy(text, "Zasubskrybowano temat ");
                            if(how == 1) sprintf(text, "%s%d%s", text, topicnr, " w trybie trwałym\n");
                            else sprintf(text, "%s%d%s%d%s", text, topicnr, " w trybie przejściowym na ", how_long, " wiadomości\n");
                            sendto(pipes[id(msg.pid)], text);
                        } else if(c != -1) { // przedłużenie subskrybcji
                            sprintf(text, "%s%d%s%d%s%d%s", "Przedłużyłeś subskrypcję tematu ", topicnr, " o ", how_long, " wiadomości, twoja subskrypcja obejmuje jeszcze ", c, " wiadomości\n");
                            sendto(pipes[id(msg.pid)], text);
                        } else { // błąd
                            strcpy(text, "Zasubskrybowałeś już ten temat\n");
                            sendto(pipes[id(msg.pid)], text);
                        }
                    }
                }
                break;
            case 3: // tworzenie tematu
                carry = 1;
                for(int i = 0; i < 10; i++){ // sprawdzanie czy użytkownik jest zalogowany
                    if(pid[i] == msg.pid){
                        carry = 0;
                    }
                }
                if(carry == 0){
                    int newtopic;
                    sscanf(msg.text, "%d", &newtopic);
                    int b = new(msg.pid, newtopic); 
                    if(b == -1){ // błąd
                        sprintf(text, "%s%d%s", "Temat ", newtopic, " juz istnieje\n");
                        sendto(pipes[id(msg.pid)], text);
                    }else if(b == 1){ // powodzenie
                        sprintf(text, "%s%d%s", "Utworzono temat ", newtopic, "\n");
                        topics[newtopic - 1].created = 1;
                        for(int i = 0; i < 10; i++){
                        }
                        sendtoall(text); // wysyłanie do wszystkich zalogowanych użytkowników
                    }
                }
                break;
            case 4: // nowa wiadomość na dany temat
                carry = 1;
                for(int i = 0; i < 10; i++){ // sprawdzanie czy użytkownik jest zalogowany
                    if(pid[i] == msg.pid){
                        carry = 0;
                    }
                }
                if(carry == 0){
                    int typ, pior;
                    sscanf(msg.text, "%d%d%*c%[^\n]", &typ, &pior, text);
                    int d = message(msg.pid, typ, pior, text);
                    if (d == 1){ // powodzenie
                        sendto(pipes[id(msg.pid)], "Wysłano wiadomość\n");
                    } else if(d == -1) { // błąd
                        sendto(pipes[id(msg.pid)], "Podany temat nie isnieje\n");
                    } else if (d == -2){ // błąd
                        sendto(pipes[id(msg.pid)], "Nie subskrybujesz tego tematu\n");
                    }
                }
                break;
            case 5: // blokowanie użytkownika
                carry = 1;
                for(int i = 0; i < 10; i++){ // sprawdzanie czy użytkownik jest zalogowany
                    if(pid[i] == msg.pid){
                        carry = 0;
                    }
                }
                if(carry == 0){
                    sscanf(msg.text, "%s", nazwa);
                    for(int i = 0; i < 10; i++){
                        if(strcmp(names[i].name, nazwa) == 0){ // odnalezienie użytkownika o podanej nazwie
                            struct msgbuf msg1;
                            msg1.type = 12;
                            sprintf(msg1.text, "%d\n", i);
                            msgsnd(pipes[id(msg.pid)], &msg1, size, IPC_NOWAIT);
                        }
                    }
                }
                break;
            case 6:
                struct msgbuf msg1;
                msg1.type = 50;
                msgsnd(pipes[id(msg.pid)], &msg1, size, IPC_NOWAIT);
                break;
            default:
                break;
        }
    }
};