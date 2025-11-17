import subprocess
import matplotlib.pyplot as plt
import numpy as np

# Function to run the test program with a specific read/write ratio
def run_test(read_ratio, write_ratio):
    # Set environment variables for the test program
    env = {
        "READ_RATIO": str(read_ratio),
        "WRITE_RATIO": str(write_ratio),
    }

    # Run the test program (e.g., testpf) and capture its output
    result = subprocess.run(["./testpf_seq"], capture_output=True, text=True, env=env)

    # Parse the output to extract statistics
    output = result.stdout
    logical_io = int(output.split("Logical I/Os:")[1].split("\n")[0].strip())
    physical_io = int(output.split("Physical I/Os:")[1].split("\n")[0].strip())
    disk_reads = int(output.split("Disk Reads:")[1].split("\n")[0].strip())
    disk_writes = int(output.split("Disk Writes:")[1].split("\n")[0].strip())

    return logical_io, physical_io, disk_reads, disk_writes

# Function to simulate different read/write mixtures and collect statistics
def collect_statistics():

    read_ratios = np.linspace(0, 100, 5)  # Read ratios from 0% to 100% in steps of 1%
    write_ratios = 100 - read_ratios       # Corresponding write ratios

    stats = {
        "read_ratios": [],
        "logical_io": [],
        "physical_io": [],
        "disk_reads": [],
        "disk_writes": [],
    }

    for read_ratio, write_ratio in zip(read_ratios, write_ratios):
        logical_io, physical_io, disk_reads, disk_writes = run_test(read_ratio, write_ratio)
        stats["read_ratios"].append(read_ratio)
        stats["logical_io"].append(logical_io)
        stats["physical_io"].append(physical_io)
        stats["disk_reads"].append(disk_reads)
        stats["disk_writes"].append(disk_writes)

    return stats

# Function to plot the graph
def plot_graph(stats):
    read_ratios = stats["read_ratios"]
    logical_io = stats["logical_io"]
    physical_io = stats["physical_io"]
    disk_reads = stats["disk_reads"]
    disk_writes = stats["disk_writes"]

    plt.figure(figsize=(10, 6))

    # Plot Logical I/O
    plt.plot(read_ratios, logical_io, label="Logical I/O", marker="o")

    # Plot Physical I/O
    plt.plot(read_ratios, physical_io, label="Physical I/O", marker="o")

    # Plot Disk Reads
    plt.plot(read_ratios, disk_reads, label="Disk Reads", marker="o")

    # Plot Disk Writes
    plt.plot(read_ratios, disk_writes, label="Disk Writes", marker="o")

    plt.text(0.5, 0.95, "Strategy: LRU Sequential Access", horizontalalignment='center', verticalalignment='center', transform=plt.gca().transAxes, fontsize=12, bbox=dict(facecolor='white', alpha=0.5))


    plt.xlabel("Read Ratio (%)")
    plt.ylabel("I/O Counts")
    plt.title("I/O Statistics vs Read/Write Mixtures")
    plt.legend()
    plt.grid(True)
    plt.savefig("SequentialIO_statistics.png")  # Save the graph as a PNG file
    plt.show()

# Main function
def main():
    print("Collecting statistics for different read/write mixtures...")
    stats = collect_statistics()
    print("Statistics collected. Generating graph...")
    plot_graph(stats)
    print("Graph saved as 'SequentialIO_statistics.png'.")

if __name__ == "__main__":
    main()