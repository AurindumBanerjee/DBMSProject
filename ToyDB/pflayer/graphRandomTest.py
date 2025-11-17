import subprocess
import matplotlib.pyplot as plt
import numpy as np

# Run a given binary (MRU or LRU)
def run_test(binary, read_ratio, write_ratio):
    env = {
        "READ_RATIO": str(read_ratio),
        "WRITE_RATIO": str(write_ratio),
    }

    result = subprocess.run([binary], capture_output=True, text=True, env=env)
    output = result.stdout

    logical_io = int(output.split("Logical I/Os:")[1].split("\n")[0].strip())
    physical_io = int(output.split("Physical I/Os:")[1].split("\n")[0].strip())
    disk_reads = int(output.split("Disk Reads:")[1].split("\n")[0].strip())
    disk_writes = int(output.split("Disk Writes:")[1].split("\n")[0].strip())

    return logical_io, physical_io, disk_reads, disk_writes


def collect_statistics(binary):
    read_ratios = np.linspace(0, 100, 101)
    write_ratios = 100 - read_ratios

    stats = {
        "read_ratios": [],
        "logical_io": [],
        "physical_io": [],
        "disk_reads": [],
        "disk_writes": [],
    }

    for read_ratio, write_ratio in zip(read_ratios, write_ratios):
        logical_io, physical_io, disk_reads, disk_writes = run_test(binary, read_ratio, write_ratio)
        stats["read_ratios"].append(read_ratio)
        stats["logical_io"].append(logical_io)
        stats["physical_io"].append(physical_io)
        stats["disk_reads"].append(disk_reads)
        stats["disk_writes"].append(disk_writes)

    return stats


def plot_graph(stats, title, outname):
    read_ratios = stats["read_ratios"]
    logical_io = stats["logical_io"]
    physical_io = stats["physical_io"]
    disk_reads = stats["disk_reads"]
    disk_writes = stats["disk_writes"]

    plt.figure(figsize=(10, 6))

    plt.plot(read_ratios, logical_io, label="Logical I/O")
    plt.plot(read_ratios, physical_io, label="Physical I/O")
    plt.plot(read_ratios, disk_reads, label="Disk Reads")
    plt.plot(read_ratios, disk_writes, label="Disk Writes")

    plt.text(0.5, 0.95, title,
             horizontalalignment='center',
             verticalalignment='center',
             transform=plt.gca().transAxes,
             fontsize=12,
             bbox=dict(facecolor='white', alpha=0.5))

    plt.xlabel("Read Ratio (%)")
    plt.ylabel("I/O Counts")
    plt.title("I/O Statistics vs Read/Write Mixtures")
    plt.legend()
    plt.grid(True)
    plt.savefig(outname)
    plt.show()


def main():
    print("Running MRU first (Random Access)...")
    stats_mru = collect_statistics("./testpf_MRU")
    plot_graph(stats_mru, "Strategy: MRU Random Access", "RandomIO_MRU.png")

    print("Running LRU second (Random Access)...")
    stats_lru = collect_statistics("./testpf_LRU")
    plot_graph(stats_lru, "Strategy: LRU Random Access", "RandomIO_LRU.png")

    print("Done. Saved RandomIO_MRU.png and RandomIO_LRU.png")


if __name__ == "__main__":
    main()
