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
        self.predictors = ["bimodal", "gag", "gap", "pap"]
        self.data_list: List[Dict[str, Any]] = []

    def parse_file(self, file_path: Path, predictor: str, trace_name: str) -> None:
        """Parses a single ChampSim result file using regex."""
        try:
            content = file_path.read_text()

            # Find the ROI (Region of Interest) IPC
            # Note: We look specifically for the line after '=== Simulation ==='
            ipc_match = re.search(r"CPU 0 cumulative IPC: ([\d.]+)", content)
            # Find MPKI
            mpki_match = re.search(r"MPKI: ([\d.]+)", content)

            if ipc_match and mpki_match:
                self.data_list.append(
                    {
                        "Trace": trace_name.removesuffix(".champsimtrace.xz.txt"),
                        "Predictor": predictor,
                        "IPC": float(ipc_match.group(1)),
                        "MPKI": float(mpki_match.group(1)),
                    }
                )
            else:
                print(f"Warning: Could not find metrics in {file_path}")
        except Exception as e:
            print(f"Error parsing {file_path}: {e}")

    def run(self):
        # 1. Collect Data
        for pred in self.predictors:
            pred_path = self.results_dir / pred
            if not pred_path.exists():
                print(f"Directory not found: {pred_path}")
                continue

            for file in pred_path.glob("*.txt"):
                self.parse_file(file, pred, file.name)

        if not self.data_list:
            print("No data collected. Check your directory structure.")
            return

        df = pd.DataFrame(self.data_list)

        # 2. Calculate GMEAN
        gmean_stats = []
        for pred in self.predictors:
            pred_df = df[df["Predictor"] == pred]
            if not pred_df.empty:
                gmean_stats.append(
                    {
                        "Predictor": pred,
                        "IPC_GMEAN": gmean(pred_df["IPC"]),
                        "MPKI_GMEAN": gmean(pred_df["MPKI"]),
                    }
                )

        df_gmean = pd.DataFrame(gmean_stats)
        print("\n=== Geometric Mean Results ===")
        print(df_gmean.to_string(index=False))
        df_gmean.to_markdown("gmean_stats.md", index=False)

        # 3. Plotting
        self.plot_metric(df, "IPC", "Instructions Per Cycle (Higher is Better)")
        self.plot_metric(df, "MPKI", "Misses Per Kilo-Instruction (Lower is Better)")

        # Plot GMEANs
        self.plot_gmeans(df_gmean)

    def plot_metric(self, df: pd.DataFrame, metric: str, ylabel: str):
        """Creates a grouped bar chart for a specific metric across all traces."""
        pivot_df = df.pivot(index="Trace", columns="Predictor", values=metric)

        # Ensure predictors are in the specified order for comparison
        available_cols = [p for p in self.predictors if p in pivot_df.columns]
        pivot_df = pivot_df[available_cols]

        ax = pivot_df.plot(kind="bar", figsize=(14, 7), width=0.8)
        plt.title(f"ChampSim: {metric} comparison across traces", fontsize=14)
        plt.ylabel(ylabel, fontsize=12)
        plt.xlabel("Traces", fontsize=12)
        plt.xticks(rotation=45, ha="right")
        plt.grid(axis="y", linestyle="--", alpha=0.7)
        plt.legend(title="Predictor Schemes")
        plt.tight_layout()
        plt.savefig(f"champsim_{metric.lower()}_comparison.png", dpi=300)
        plt.show()

    def plot_gmeans(self, df_gmean: pd.DataFrame):
        """Creates a simple comparison of GMEANs."""
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

        df_gmean.plot(
            x="Predictor",
            y="IPC_GMEAN",
            kind="bar",
            ax=ax1,
            color="skyblue",
            legend=False,
        )
        ax1.set_title("GMEAN IPC")
        ax1.set_ylabel("IPC")

        df_gmean.plot(
            x="Predictor",
            y="MPKI_GMEAN",
            kind="bar",
            ax=ax2,
            color="salmon",
            legend=False,
        )
        ax2.set_title("GMEAN MPKI")
        ax2.set_ylabel("MPKI")

        plt.tight_layout()
        plt.savefig("champsim_gmean_summary.png", dpi=300)
        plt.show()


if __name__ == "__main__":
    # Ensure your folder structure is ./results/<predictor>/<trace>.txt
    analyzer = ChampSimAnalyzer(results_dir="./")
    analyzer.run()
