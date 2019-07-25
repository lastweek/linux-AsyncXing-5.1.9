set -x
set -e

DIR=/sys/kernel/debug/tracing

# Presetup if any
echo ffffff > $DIR/tracing_cpumask

# Disable tracing and clear trace
echo 0 > $DIR/tracing_on
echo > $DIR/trace
echo > $DIR/set_ftrace_filter
echo > $DIR/set_graph_function

# Setup tracer type
echo function_graph > $DIR/current_tracer

#echo rmqueue >> $DIR/set_ftrace_filter
echo request_refill >> $DIR/set_ftrace_filter
echo refill_list >> $DIR/set_ftrace_filter

echo 1 > $DIR/tracing_on
cat  $DIR/enabled_functions
