#define BAUDLIST_SIZE 8

const speed_t baud_list[ BAUDLIST_SIZE];
const int     baud_alias[BAUDLIST_SIZE];

int loadModemConfig(conn ** headconn);
int configureModem(conn ** headconn, const char * path, int baud, const char * magic);
int telnetOptions(user * muser);

int socketCreate(conn ** headconn, int sock);
int connGarbage(conn ** headconn);

int setDTR(conn * mconn, int set);
int getDCD(conn * mconn);
