import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
import operator

latency_1_cpu = []
latency_2_cpu = []
latency_4_cpu = []
latency_6_cpu = []

latency_cpu1 = []
latency_cpu2 = []
latency_cpu4 = []
latency_cpu6 = []

skipped = 0
THRESHOLD_NS = 10000
PLOT_LIMIT=10000

fname = "1core.txt"
f = open(fname, "r")
for line in f:
    if len(line.strip()) != 0:
        data = line.split()

        if data[0].split('=')[0] != "idx":
            continue;

        order = data[1].split('=')[1]
        latency = data[2].split('=')[1]

        latency_cpu1.append(float(latency))
        if (float(latency) < THRESHOLD_NS):
            latency_1_cpu.append(float(latency))
        else:
            skipped += 1

latency_1_cpu = sorted(latency_1_cpu)
string = "1 CPU Plotting threshold is " + str(THRESHOLD_NS) + ". Skipped " + str(skipped)
print(string)

fname = "2core.txt"
f = open(fname, "r")
for line in f:
    if len(line.strip()) != 0:
        data = line.split()

        if data[0].split('=')[0] != "idx":
            continue;

        order = data[1].split('=')[1]
        latency = data[2].split('=')[1]

        latency_cpu2.append(float(latency))
        if (float(latency) < THRESHOLD_NS):
            latency_2_cpu.append(float(latency))
        else:
            skipped += 1

latency_2_cpu = sorted(latency_2_cpu)
string = "2 CPU Plotting threshold is " + str(THRESHOLD_NS) + ". Skipped " + str(skipped)
print(string)

fname = "4core.txt"
f = open(fname, "r")
for line in f:
    if len(line.strip()) != 0:
        data = line.split()

        if data[0].split('=')[0] != "idx":
            continue;

        order = data[1].split('=')[1]
        latency = data[2].split('=')[1]

        latency_cpu4.append(float(latency))
        if (float(latency) < THRESHOLD_NS):
            latency_4_cpu.append(float(latency))
        else:
            skipped += 1

latency_4_cpu = sorted(latency_4_cpu)
string = "4 CPU Plotting threshold is " + str(THRESHOLD_NS) + ". Skipped " + str(skipped)
print(string)

fname = "6core.txt"
f = open(fname, "r")
for line in f:
    if len(line.strip()) != 0:
        data = line.split()

        if data[0].split('=')[0] != "idx":
            continue;

        order = data[1].split('=')[1]
        latency = data[2].split('=')[1]

        latency_cpu6.append(float(latency))
        if (float(latency) < THRESHOLD_NS):
            latency_6_cpu.append(float(latency))
        else:
            skipped += 1

latency_6_cpu = sorted(latency_6_cpu)
string = "6CPU Plotting threshold is " + str(THRESHOLD_NS) + ". Skipped " + str(skipped)
print(string)

#
# Okay, we have the data.
# Now just plot
#

fig, ax = plt.subplots(nrows=1, ncols=1)

# plot the cumulative histogram
n, bins, patches = ax.hist(latency_1_cpu, len(latency_1_cpu), density=1, histtype='step', cumulative=True, label='1 thread')
patches[0].set_xy(patches[0].get_xy()[:-1])

n, bins, patches = ax.hist(latency_2_cpu, len(latency_2_cpu), density=1, histtype='step', cumulative=True, label='2 threads')
patches[0].set_xy(patches[0].get_xy()[:-1])

n, bins, patches = ax.hist(latency_4_cpu, len(latency_4_cpu), density=1, histtype='step', cumulative=True, label='4 threads')
patches[0].set_xy(patches[0].get_xy()[:-1])

n, bins, patches = ax.hist(latency_6_cpu, len(latency_6_cpu), density=1, histtype='step', cumulative=True, label='6 threads')
patches[0].set_xy(patches[0].get_xy()[:-1])

# tidy up the figure
ax.grid(True)
ax.legend(loc="lower center")
ax.set_title("Buddy Allocator (Order-0)")
ax.set_xlabel('Latency (CPU Cycles)')
ax.set_ylabel('CDF')

ax.set_xlim(left=0)
ax.set_ylim([0, 1])

#
# Now the bar Axe
#
total_avg = []
total_95p = []
total_99p = []

total_avg.append(np.average(latency_cpu1))
total_avg.append(np.average(latency_cpu2))
total_avg.append(np.average(latency_cpu4))
total_avg.append(np.average(latency_cpu6))

total_95p.append(np.percentile(latency_cpu1, 95))
total_95p.append(np.percentile(latency_cpu2, 95))
total_95p.append(np.percentile(latency_cpu4, 95))
total_95p.append(np.percentile(latency_cpu6, 95))

total_99p.append(np.percentile(latency_cpu1, 99))
total_99p.append(np.percentile(latency_cpu2, 99))
total_99p.append(np.percentile(latency_cpu4, 99))
total_99p.append(np.percentile(latency_cpu6, 99))

print("Avg", total_avg)
print("95P", total_95p)
print("99P", total_99p)

#plt.show()
pp = PdfPages('fig_buddy.pdf')
pp.savefig(bbox_inches = "tight")
pp.close()
