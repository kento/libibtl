
#define IBVIO_OP_MASK   (0xffff)
#define IBVIO_OP_OFFSET 16
#define IBVIO_FD_MASK   (0xffff)
#define IBVIO_FD_OFFSET 0


/**
 * 32-bit tag formed by packing:
 *
 * ( op 16bits | fd 16bits )
 **/
void ibvio_serialize_tag(int fd, int op, int *tag)
{
  *tag = 0;
  *tag = ( 
	  ( ((op)  & IBVIO_OP_MASK) << IBVIO_OP_OFFSET) | 
	  ( ((fd)  & IBVIO_FD_MASK) << IBVIO_FD_OFFSET) 
         );
  return;
}


void ibvio_deserialize_tag(int *fd, int *op, int tag)
{
  *op = 0;
  *op = (tag >> IBVIO_OP_OFFSET) & IBVIO_OP_MASK;

  *fd = 0;
  *fd = (tag >> IBVIO_FD_OFFSET) & IBVIO_FD_MASK;
  return;
}
