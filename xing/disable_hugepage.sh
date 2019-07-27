#
# Disable features that affect performance measuring
#

# Hugepage is evil always
echo "never" > /sys/kernel/mm/transparent_hugepage/enabled

echo 0 >/sys/kernel/mm/ksm/run

# Watchdog is evil sometimes
echo "0" > /proc/sys/kernel/watchdog
echo 0 > /proc/sys/kernel/nmi_watchdog
echo 0 > /proc/sys/kernel/soft_watchdog

echo 0 > /proc/sys/kernel/numa_balancing

echo 0 > /proc/sys/kernel/panic_on_oops
