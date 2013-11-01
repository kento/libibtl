#ifndef FDMI_DATATYPE_H
#define FDMI_DATATYPE_H


enum fdmi_dtype {
  /* NOTEf: order has to be same as "fdmi_dtype_op dtype_op" in fdmi_datatype.c*/
  FDMI_INT,
  FDMI_DOUBLE,
  FDMI_FLOAT,
  FDMI_BYTE,
  FDMI_CHAR,
  FDMI_USRDEF
};

enum fdmi_op {
  FMI_MAX,
  FMI_MIN,
  FMI_SUM,
  FMI_NON
};

struct fdmi_datatype {
  enum fdmi_dtype dtype;
  size_t size;
  int count;
  int blength;
  int stride;
};
typedef struct fdmi_datatype FMI_Datatype;

extern struct fdmi_datatype	FMI_BYTE;//    = {FDMI_INT, sizeof(int)};
extern struct fdmi_datatype	FMI_CHAR;//    = {FDMI_INT, sizeof(int)};
extern struct fdmi_datatype	FMI_INT;//    = {FDMI_INT, sizeof(int)};
extern struct fdmi_datatype	FMI_FLOAT;//  = {FDMI_FLOAT, sizeof(float)};
extern struct fdmi_datatype	FMI_DOUBLE;// = {FDMI_DOUBLE, sizeof(double)};



size_t fdmi_datatype_size(struct fdmi_datatype dtype);
int fdmi_datatype_op(int fdmi_op, struct fdmi_datatype dtype, void *target, void *source);

int fdmi_type_vector(int count, int blocklength, int stride, struct fdmi_datatype oldtype, struct fdmi_datatype *newtype);
int fdmi_type_commit(struct fdmi_datatype *datatype);


#endif
