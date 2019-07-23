import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
import operator

def set_axis_style(ax, labels):
    ax.get_xaxis().set_tick_params(direction='out')
    ax.xaxis.set_ticks_position('bottom')
    ax.set_xticks(np.arange(1, len(labels) + 1))
    ax.set_xticklabels(labels)
    ax.set_xlim(0.25, len(labels) + 0.75)

latency_1_cpu = []
latency_2_cpu = []
latency_4_cpu = []
latency_6_cpu = []

skipped = 0
THRESHOLD_NS = 5000
PLOT_LIMIT=2500

fname = "1core.txt"
f = open(fname, "r")
for line in f:
    if len(line.strip()) != 0:
        data = line.split()

        if data[0].split('=')[0] != "idx":
            continue;

        order = data[1].split('=')[1]
        latency = data[2].split('=')[1]

        # 5000 cycles?
        if (float(latency) < 5000):
            latency_1_cpu.append(float(latency))
        else:
            skipped += 1

latency_1_cpu = sorted(latency_1_cpu)
string = "Plotting threshold is " + str(THRESHOLD_NS) + ". Skipped " + str(skipped)
print(string)
print("Max latency in plotting:", latency_1_cpu[-1])
print("1CPU Average:", np.sum(latency_1_cpu)/len(latency_1_cpu))

fname = "2core.txt"
f = open(fname, "r")
for line in f:
    if len(line.strip()) != 0:
        data = line.split()

        if data[0].split('=')[0] != "idx":
            continue;

        order = data[1].split('=')[1]
        latency = data[2].split('=')[1]

        if (float(latency) < THRESHOLD_NS):
            latency_2_cpu.append(float(latency))
        else:
            skipped += 1

latency_2_cpu = sorted(latency_2_cpu)
string = "Plotting threshold is " + str(THRESHOLD_NS) + ". Skipped " + str(skipped)
print(string)
print("Max latency in plotting:", latency_2_cpu[-1])
print("2CPU Average:", np.sum(latency_2_cpu)/len(latency_2_cpu))

fname = "4core.txt"
f = open(fname, "r")
for line in f:
    if len(line.strip()) != 0:
        data = line.split()

        if data[0].split('=')[0] != "idx":
            continue;

        order = data[1].split('=')[1]
        latency = data[2].split('=')[1]

        if (float(latency) < THRESHOLD_NS):
            latency_4_cpu.append(float(latency))
        else:
            skipped += 1

latency_4_cpu = sorted(latency_4_cpu)
string = "Plotting threshold is " + str(THRESHOLD_NS) + ". Skipped " + str(skipped)
print(string)
print("Max latency in plotting:", latency_4_cpu[-1])
print("4CPU Average:", np.sum(latency_4_cpu)/len(latency_4_cpu))

fname = "6core.txt"
f = open(fname, "r")
for line in f:
    if len(line.strip()) != 0:
        data = line.split()

        if data[0].split('=')[0] != "idx":
            continue;

        order = data[1].split('=')[1]
        latency = data[2].split('=')[1]

        if (float(latency) < THRESHOLD_NS):
            latency_6_cpu.append(float(latency))
        else:
            skipped += 1

latency_6_cpu = sorted(latency_6_cpu)
string = "Plotting threshold is " + str(THRESHOLD_NS) + ". Skipped " + str(skipped)
print(string)
print("Max latency in plotting:", latency_6_cpu[-1])
print("6CPU Average:", np.sum(latency_6_cpu)/len(latency_6_cpu))

fig, ax = plt.subplots()

# plot the cumulative histogram
combined = [latency_1_cpu, latency_2_cpu, latency_4_cpu, latency_6_cpu]
ax.violinplot(combined)


#ax.set_xlim(left=0, right=PLOT_LIMIT)
#ax.set_ylim([0, 1])

# set style for the axes
labels = ['1', '2', '4', '6']

set_axis_style(ax, labels)

ax.set_title("Buddy Allocator (Order-0)")
ax.set_xlabel('Number of Threads')
ax.set_ylabel('Latency (CPU Cycles)')
ax.set_ylim([100, 600])
ax.grid(True)

#plt.show()
pp = PdfPages('fig_buddy_violin.pdf')
pp.savefig(bbox_inches = "tight")
pp.close()
