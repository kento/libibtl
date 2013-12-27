#ifndef FDMI_PARAM_H
#define FDMI_PARAM_H

#define DEFAULT_FMI_DEVICE_TYPE "verbs"

char* fdmi_param_get(char* param_name);
void fdmi_param_set(char* name, char* value);

#endif
