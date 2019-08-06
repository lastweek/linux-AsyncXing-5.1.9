import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
import operator

def autolabel(rects):
    """Attach a text label above each bar in *rects*, displaying its height."""
    for rect in rects:
        height = rect.get_height()
        ax.annotate('{}'.format(height),
                    xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 1),  # 3 points vertical offset
                    textcoords="offset points",
                    ha='center', va='bottom')

latency_1_cpu = []
latency_2_cpu = []
latency_4_cpu = []
latency_6_cpu = []

latency_cpu1 = []
latency_cpu2 = []
latency_cpu4 = []
latency_cpu6 = []

skipped = 0
THRESHOLD_NS = 1000000
PLOT_LIMIT=10000

fname = "log_core_1.txt"
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

fname = "log_core_2.txt"
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

fname = "log_core_4.txt"
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

fname = "log_core_6.txt"
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

fig, (ax, ax2) = plt.subplots(nrows=2, ncols=1)

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
ax.legend(loc="lower right")
ax.set_title("LRU List Insertion Latency (# Threads)")
ax.set_xlabel('Latency (Cycles)')
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

tmp1 = []
tmp2 = []
tmp3 = []
tmp4 = []

tmp1.append(total_avg[0])
tmp1.append(total_95p[0])
tmp1.append(total_99p[0])
tmp2.append(total_avg[1])
tmp2.append(total_95p[1])
tmp2.append(total_99p[1])
tmp3.append(total_avg[2])
tmp3.append(total_95p[2])
tmp3.append(total_99p[2])
tmp4.append(total_avg[3])
tmp4.append(total_95p[3])
tmp4.append(total_99p[3])

print(tmp1)
print(tmp2)
print(tmp3)
print(tmp4)

index = ['Avg', '95P', '99P']
index_pos = np.arange(len(index))

width = 0.2

rects1 = ax2.bar(index_pos - width*2, tmp1, width, label='1 thread')
rects2 = ax2.bar(index_pos - width*1, tmp2, width, label='2 threads')
rects3 = ax2.bar(index_pos, tmp3, width, label='4 threads')
rects4 = ax2.bar(index_pos + width, tmp4, width, label='6 threads')

ax2.set_xticks(index_pos)
ax2.set_xticklabels(index)
ax2.set_ylabel("Latency (Cycles)")
ax2.set_xlabel("Average, 95- and 99-percentile.");
ax2.legend()

#autolabel(rects1)
#autolabel(rects2)
#autolabel(rects3)
#autolabel(rects4)

plt.tight_layout(pad=0.4, w_pad=0.5, h_pad=1.0)

#plt.show()
pp = PdfPages('fig_lru.pdf')
pp.savefig(bbox_inches = "tight")
pp.close()
