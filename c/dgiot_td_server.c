#include <stdlib.h>
#include <stdio.h>
#include "mosquitto.h"
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <taos.h>

static TAOS * taos;
struct userdata_td {
    char tdhost[64];
	char sql[64];
	char debug[64];
	char connect[64];
	char ack[64];
};

#define BUF_SIZE 1024

void do_cmd(struct mosquitto *mosq, const char *topic, const char *netcmd) {
    char cmd[BUF_SIZE] = "";
    char payload[BUF_SIZE] = "";
    snprintf(cmd, BUF_SIZE, "%s", netcmd);
    printf("%s\n", cmd);
    FILE * p_file = NULL;

    p_file = popen(cmd, "r");
    if (!p_file) {
        fprintf(stderr, "Erro to popen");
    }

    while (fgets(payload, BUF_SIZE, p_file) != NULL) {
        fprintf(stdout, "%s", payload);
        mosquitto_publish(mosq, NULL, topic, BUF_SIZE, payload, 0, 0);
    }
    pclose(p_file);
 }

 char* get_key() {
    char cmd[BUF_SIZE] = "";
    char sn[BUF_SIZE] = "";
    char* string;
    int i = 0;

    snprintf(cmd, BUF_SIZE, "dmidecode | grep  'Serial Number' | md5sum | cut -d ' ' -f1 ");
    FILE * p_file = NULL;
    p_file = popen(cmd, "r");
    if (!p_file) {
     fprintf(stderr, "Erro to popen");
    }
    while (fgets(sn, BUF_SIZE, p_file) != NULL) {
//      fprintf(stdout, "%s", sn);
    }
    pclose(p_file);

    size_t length = strlen(sn) + 1;
    if ((string = (char*) malloc(length)) == NULL ){
        return NULL ;
    }

    for (i = 0; i < 16; i++){
        string[i] = sn[i];
    }
    return string;
  }

int on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
    if(strcmp(msg->topic, (const char *)(((struct userdata_td *)userdata)->debug)) == 0){
        if (taos_query(taos, msg->payload)) {
            char ack[128];
            snprintf(ack, 128, "%s\n", taos_errstr(taos));
            mosquitto_publish(mosq, NULL, (const char *)(((struct userdata_td *)userdata)->ack), (int)strlen(ack)+1, ack, 0, 0);
        }
    }
   
   if(strcmp(msg->topic, (const char *)(((struct userdata_td *)userdata)->sql)) == 0){
            if (taos_query(taos, msg->payload)) {
                char ack[128];
                snprintf(ack, 128, "%s\n", taos_errstr(taos));
                mosquitto_publish(mosq, NULL, (const char *)(((struct userdata_td *)userdata)->ack), (int)strlen(ack)+1, ack, 0, 0);
            }
    }else if(strcmp(msg->topic, (const char *)(((struct userdata_td *)userdata)->connect)) == 0){
        taos_close(taos);
        taos = taos_connect((const char *)(((struct userdata_td *)userdata)->tdhost), "root", msg->payload, NULL, 0);
        char ack[64];
        if (taos == NULL) {
            snprintf(ack, 64, "connect failed, reason:%s\n", taos_errstr(taos));
            mosquitto_publish(mosq, NULL, (const char *)(((struct userdata_td *)userdata)->ack), (int)strlen(ack)+1, ack, 0, 0);
        }else{
            snprintf(ack, 64, "connect success username:root password:%s\n", msg->payload);
            mosquitto_publish(mosq, NULL, (const char *)(((struct userdata_td *)userdata)->ack), (int)strlen(ack)+1, ack, 0, 0);
        }
        printf("%s",ack);
    }
	return 0;
}

int main(int argc, char *argv[])
{
   int rc;
   struct userdata_td userdata;
   char mqttserver[64], sub_topic[64], cliend_id[32];
   char username[64],password[64];
   snprintf(userdata.tdhost, 128, "127.0.0.1");
   strcpy(username, "taos");
   strcpy(password, "taosdata");
   if ( argc == 1 ) {
       printf("usage: %s mqttserver  \n", argv[0]);
       exit(0);
   }
    if ( argc >= 1 ) strcpy(mqttserver, argv[1]);
    // init TAOS
    taos_init();
    mosquitto_lib_init();

    snprintf(cliend_id, 32, "%s", get_key());
    snprintf(sub_topic, 64, "tdpool/%s/#",cliend_id);
    snprintf(userdata.ack, 64, "dgiot/%s", cliend_id);
    snprintf(userdata.sql, 64, "tdpool/%s/sql", cliend_id);
    snprintf(userdata.debug, 64, "tdpool/%s/debug", cliend_id);
    snprintf(userdata.connect, 64, "tdpool/%s/connect", cliend_id);

    rc = mosquitto_subscribe_callback(
        on_message, &userdata,
        sub_topic, 0,
        mqttserver, 1883,
        cliend_id, 60, true,
        username, password,
        NULL, NULL);

     if(rc){
	    printf("Error: %s\n", mosquitto_strerror(rc));
     }

     mosquitto_lib_cleanup();

     return rc;
}
