import subprocess
import logging
import concurrent.futures
from pathlib import Path
from typing import List, Optional

# --- Configuration ---
PREDICTORS = ["bimodal", "gag", "gap", "pap"]
TRACE_DIR = Path("../traces")
RESULTS_DIR = Path("results")
MAX_PARALLEL_RUNS = 4
WARMUP_INST = 5_000_000
SIM_INST = 25_000_000

# Setup Logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[logging.StreamHandler()],
)
logger = logging.getLogger(__name__)


def run_command(cmd: List[str], cwd: Optional[Path] = None) -> bool:
    """Helper to run shell commands and catch failures."""
    try:
        result = subprocess.run(
            cmd, cwd=cwd, check=True, capture_output=True, text=True
        )
        return True
    except subprocess.CalledProcessError as e:
        logger.error(f"Command failed: {' '.join(cmd)}")
        logger.error(f"Stderr: {e.stderr}")
        return False


def compile_simulator(config_name: str) -> bool:
    """Configures and builds the ChampSim binary for a specific predictor."""
    logger.info(f"--- Compiling for Predictor: {config_name} ---")

    config_json = f"{config_name}.json"
    if not Path(config_json).exists():
        logger.error(f"Config file {config_json} not found!")
        return False

    # Step 1: config.sh
    if not run_command(["./config.sh", config_json]):
        return False

    # Step 2: make
    # Using -j to speed up compilation itself
    if not run_command(["make", "-j8"]):
        return False

    return True


def run_single_trace(predictor: str, trace_path: Path) -> None:
    """Executes a single simulation and redirects output."""
    trace_name = trace_path.name
    output_dir = RESULTS_DIR / predictor
    output_dir.mkdir(parents=True, exist_ok=True)
    output_file = output_dir / f"{trace_name}.txt"

    # The default ChampSim binary name
    # Note: Modern ChampSim usually names it 'bin/champsim'
    binary_path = "./bin/champsim"

    cmd = [
        binary_path,
        "--warmup_instructions",
        str(WARMUP_INST),
        "--simulation_instructions",
        str(SIM_INST),
        str(trace_path),
    ]

    logger.info(f"Starting: {predictor} | {trace_name}")

    try:
        with open(output_file, "w") as f:
            subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, check=True)
        logger.info(f"Finished: {predictor} | {trace_name}")
    except subprocess.CalledProcessError:
        logger.error(f"Failed to run trace: {trace_name} with predictor {predictor}")


def main():
    # 1. Validation
    if not TRACE_DIR.exists():
        logger.error(f"Trace directory {TRACE_DIR} does not exist.")
        return

    # Find all traces (assuming .champsimtrace.xz or similar format)
    # Adjust the glob pattern if your traces have different extensions
    traces = sorted([p for p in TRACE_DIR.glob("*") if p.is_file()])

    if not traces:
        logger.error("No traces found in directory.")
        return

    # 2. Iterate Predictors
    for predictor in PREDICTORS:
        # Recompile (Sequential)
        success = compile_simulator(predictor)
        if not success:
            logger.error(f"Skipping predictor {predictor} due to build failure.")
            continue

        # 3. Iterate Traces (Parallel)
        logger.info(
            f"Launching {len(traces)} traces for {predictor} (Workers: {MAX_PARALLEL_RUNS})"
        )

        with concurrent.futures.ThreadPoolExecutor(
            max_workers=MAX_PARALLEL_RUNS
        ) as executor:
            futures = [
                executor.submit(run_single_trace, predictor, trace) for trace in traces
            ]
            concurrent.futures.wait(futures)

    logger.info("All experiments completed successfully.")


if __name__ == "__main__":
    main()
