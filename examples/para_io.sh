#/bin/sh

for i in `seq $3`
do
    ./ibtl_io $1.$i $2 &
done
wait
echo "Total: $3 GB" 