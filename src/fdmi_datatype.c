#include <stdlib.h>
#include <stdio.h>

#include "fdmi_err.h"
#include "fdmi_datatype.h"


#define FDMI_MAX(dtype, a, b) (*(dtype*)a  = (((*(dtype*)a) > (*(dtype*)b))? *(dtype*)a:*(dtype*)b))
#define FDMI_MIN(dtype, a, b) (*(dtype*)a  = (((*(dtype*)a) < (*(dtype*)b))? *(dtype*)a:*(dtype*)b))
//#define FDMI_MIN(dtype, a, b) (*(dtype*)a  = (((*(dtype*)a) < (*(dtype*)b))? *(dtype*)a:*(dtype*)b))
#define FDMI_SUM(dtype, a, b) (*(dtype*)a  =  (*(dtype*)a) + (*(dtype*)b)      )
#define FDMI_NON(dtype, a, b)  (*(dtype*)a  =  (*(dtype*)a))

#define FDMI_DTYPE_OP(op, dtype) fdmi_dtype_op_##op##_##dtype

#define FDMI_DATATYPE_OP(op, dtype, a, b)	 \
  void FDMI_DTYPE_OP(op, dtype)(void *a, void *b) {	 \
    FDMI_##op(dtype, a, b);			 \
  }


FDMI_DATATYPE_OP(MAX, int, a, b)
FDMI_DATATYPE_OP(MAX, double, a, b)
FDMI_DATATYPE_OP(MAX, float, a, b)
FDMI_DATATYPE_OP(MIN, int, a, b)
FDMI_DATATYPE_OP(MIN, double, a, b)
FDMI_DATATYPE_OP(MIN, float, a, b)
FDMI_DATATYPE_OP(SUM, int, a, b)
FDMI_DATATYPE_OP(SUM, double, a, b)
FDMI_DATATYPE_OP(SUM, float, a, b)
FDMI_DATATYPE_OP(NON, int, a, b)
FDMI_DATATYPE_OP(NON, double, a, b)
FDMI_DATATYPE_OP(NON, float, a, b)

typedef void(*fdmi_dtype_op)(void*, void*);

fdmi_dtype_op dtype_op[4][3] = {
  {FDMI_DTYPE_OP(MAX, int), FDMI_DTYPE_OP(MAX, double), FDMI_DTYPE_OP(MAX, float)},
  {FDMI_DTYPE_OP(MIN, int), FDMI_DTYPE_OP(MIN, double), FDMI_DTYPE_OP(MIN, float)},
  {FDMI_DTYPE_OP(SUM, int), FDMI_DTYPE_OP(SUM, double), FDMI_DTYPE_OP(SUM, float)},
  {FDMI_DTYPE_OP(NON, int), FDMI_DTYPE_OP(NON, double), FDMI_DTYPE_OP(NON, float)}
};

struct fdmi_datatype FMI_BYTE    = {FDMI_BYTE, 1, 1, 1, 0};
struct fdmi_datatype FMI_CHAR    = {FDMI_CHAR, sizeof(char), 1, 1, 0};
struct fdmi_datatype FMI_INT    = {FDMI_INT, sizeof(int), 1, 1, 0};
struct fdmi_datatype FMI_FLOAT  = {FDMI_FLOAT, sizeof(float), 1, 1, 0};
struct fdmi_datatype FMI_DOUBLE = {FDMI_DOUBLE, sizeof(double), 1, 1, 0};



size_t fdmi_datatype_size(struct fdmi_datatype dtype)
{
  return dtype.blength * dtype.count * dtype.size;
}

int fdmi_datatype_op(int fdmi_op, struct fdmi_datatype dtype, void *target, void *source)
{
  //  fdmi_dbg("befor dtype: %d %d %d %d", fdmi_op, dtype.dtype, *(int*)target, *(int*)source);
  //  fdmi_dbg("befor dtype: %d %d %f %f", fdmi_op, dtype.dtype, *(double*)target, *(double*)source);
  dtype_op[fdmi_op][dtype.dtype](target, source);
  //  fdmi_dbg("after dtype: %d %d %d %d", fdmi_op, dtype.dtype, *(int*)target, *(int*)source);
  //  fdmi_dbg("after dtype: %d %d %f %f", fdmi_op, dtype.dtype, *(double*)target, *(double*)source);
}


int fdmi_type_vector(int count, int blocklength, int stride, struct fdmi_datatype oldtype, struct fdmi_datatype *newtype) 
{
  /*TODO: optimization to make 'count' =1
   e.g. count=10,blocklength=2,stride=2
      =>count=1, blocklength=20,stride=0
  */
  newtype->dtype   = FDMI_USRDEF;
  newtype->size	   = oldtype.size;
  if (blocklength == stride) {
    newtype->count   = 1;
    newtype->blength = count * blocklength;
    newtype->stride  = 0;
  } else {
    newtype->count   = count;
    newtype->blength = blocklength;
    newtype->stride  = stride;
  }

}

int fdmi_type_commit(struct fdmi_datatype *datatype)
{
  /*Do nothing so far*/
  return;
}
