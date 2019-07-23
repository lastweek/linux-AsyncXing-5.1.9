MODULE=core.ko
TRACE_FILE=trace.txt

PARAM_MAX_ORDER=1
PARAM_ALLOC_PER_ORDER=10000

echo "Tested order range: [0, $PARAM_MAX_ORDER)"
echo "Number of allocations per order: $PARAM_ALLOC_PER_ORDER"

rmmod core.ko 2>/dev/null
dmesg -w  > trace.txt

insmod $MODULE \
       param_max_order=$PARAM_MAX_ORDER \
       param_alloc_per_order=$PARAM_ALLOC_PER_ORDER
