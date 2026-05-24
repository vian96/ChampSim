import os
import re
import pandas as pd
import matplotlib.pyplot as plt
from scipy.stats import gmean
from pathlib import Path
from typing import List, Dict, Any


class ChampSimAnalyzer:
    def __init__(self, results_dir: str):
        self.results_dir = Path(results_dir)
        # Updated to cache replacement policies
        self.policies = ["lru", "lru_bip", "lru_lip", "plru", "srrip"]
        self.data_list: List[Dict[str, Any]] = []

    def parse_file(self, file_path: Path, policy: str, trace_name: str) -> None:
        """Parses a single ChampSim result file using regex for IPC and L2 Miss Rate."""
        try:
            content = file_path.read_text()

            # Find the ROI (Region of Interest) IPC
            ipc_match = re.search(r"CPU 0 cumulative IPC: ([\d.]+)", content)

            # Find L2 Cache Statistics (Total Accesses and Misses)
            # Example: cpu0->cpu0_L2C TOTAL        ACCESS:       1994 HIT:        650 MISS:       1344
            l2_match = re.search(
                r"cpu0->cpu0_L2C TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+\d+\s+MISS:\s+(\d+)",
                content,
            )

            if ipc_match and l2_match:
                accesses = int(l2_match.group(1))
                misses = int(l2_match.group(2))
                miss_rate = (misses / accesses) if accesses > 0 else 0

                self.data_list.append(
                    {
                        "Trace": trace_name.removesuffix(".champsimtrace.xz.txt"),
                        "Policy": policy,
                        "IPC": float(ipc_match.group(1)),
                        "L2_MissRate": miss_rate,
                    }
                )
            else:
                print(f"Warning: Could not find metrics in {file_path}")
        except Exception as e:
            print(f"Error parsing {file_path}: {e}")

    def run(self):
        # 1. Collect Data
        for pol in self.policies:
            pol_path = self.results_dir / pol
            if not pol_path.exists():
                print(f"Directory not found: {pol_path}")
                continue

            for file in pol_path.glob("*.txt"):
                self.parse_file(file, pol, file.name)

        if not self.data_list:
            print("No data collected. Check your directory structure.")
            return

        df = pd.DataFrame(self.data_list)

        # 2. Calculate GMEAN
        gmean_stats = []
        for pol in self.policies:
            pol_df = df[df["Policy"] == pol]
            if not pol_df.empty:
                gmean_stats.append(
                    {
                        "Policy": pol,
                        "IPC_GMEAN": gmean(pol_df["IPC"]),
                        "L2_MissRate_GMEAN": gmean(pol_df["L2_MissRate"]),
                    }
                )

        df_gmean = pd.DataFrame(gmean_stats)
        print("\n=== Geometric Mean Results ===")
        print(df_gmean.to_string(index=False))
        df_gmean.to_markdown("gmean_stats.md", index=False)

        # 3. Plotting
        self.plot_metric(df, "IPC", "Instructions Per Cycle (Higher is Better)")
        self.plot_metric(df, "L2_MissRate", "L2 Cache Miss Rate (Lower is Better)")

        # Plot GMEANs
        self.plot_gmeans(df_gmean)

    def plot_metric(self, df: pd.DataFrame, metric: str, ylabel: str):
        """Creates a grouped bar chart for a specific metric across all traces."""
        pivot_df = df.pivot(index="Trace", columns="Policy", values=metric)

        # Ensure policies are in the specified order for comparison
        available_cols = [p for p in self.policies if p in pivot_df.columns]
        pivot_df = pivot_df[available_cols]

        ax = pivot_df.plot(kind="bar", figsize=(14, 7), width=0.8)
        plt.title(f"ChampSim: {metric} comparison across traces", fontsize=14)
        plt.ylabel(ylabel, fontsize=12)
        plt.xlabel("Traces", fontsize=12)
        plt.xticks(rotation=45, ha="right")
        plt.grid(axis="y", linestyle="--", alpha=0.7)
        plt.legend(title="Replacement Policies")
        plt.tight_layout()
        plt.savefig(f"champsim_{metric.lower()}_comparison.png", dpi=300)
        plt.show()

    def plot_gmeans(self, df_gmean: pd.DataFrame):
        """Creates a simple comparison of GMEANs."""
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

        df_gmean.plot(
            x="Policy",
            y="IPC_GMEAN",
            kind="bar",
            ax=ax1,
            color="skyblue",
            legend=False,
        )
        ax1.set_title("GMEAN IPC")
        ax1.set_ylabel("IPC")

        df_gmean.plot(
            x="Policy",
            y="L2_MissRate_GMEAN",
            kind="bar",
            ax=ax2,
            color="salmon",
            legend=False,
        )
        ax2.set_title("GMEAN L2 Miss Rate")
        ax2.set_ylabel("Miss Rate")

        plt.tight_layout()
        plt.savefig("champsim_gmean_summary.png", dpi=300)
        plt.show()


if __name__ == "__main__":
    # Pointing to the 'results' folder which contains subfolders for each policy
    analyzer = ChampSimAnalyzer(results_dir="./")
    analyzer.run()
