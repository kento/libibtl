#ifndef FDMI_ERR_H
#define FDMI_ERR_H

int fdmi_err_init (void);

int fdmi_err(const char* fmt, ...);

int fdmi_alert(const char* fmt, ...);

int fdmi_dbg(const char* fmt, ...);
int fdmi_debug(const char* fmt, ...);
void fdmi_btrace();

void fdmi_exit(int no);

#endif
