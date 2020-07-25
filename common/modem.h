// Return the status of a modem's data carrier detect line
inline int getDCD( int fd );
// Return the status of a modem's data terminal ready line
inline int getDTR( int fd );
// Set the status of a modem's data terminal ready line
inline void setDTR( int fd, int enable );

