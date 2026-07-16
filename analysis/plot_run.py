#!/usr/bin/env python3
"""Plot macro and trade telemetry exported by aequitas_headless."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

RESOURCE_NAMES = {0: "FOOD", 1: "WOOD", 2: "STONE"}


def load_tables(out_dir: Path) -> tuple[pd.DataFrame, pd.DataFrame]:
    macro = pd.read_csv(out_dir / "macro.csv")
    trades_path = out_dir / "trades.csv"
    trades = pd.read_csv(trades_path) if trades_path.exists() else pd.DataFrame()
    return macro, trades


def plot_price_series(macro: pd.DataFrame, trades: pd.DataFrame, out_dir: Path) -> None:
    fig, ax = plt.subplots(figsize=(10, 4))
    if not trades.empty and "resource" in trades.columns:
        for rid, grp in trades.groupby("resource"):
            ax.plot(grp["tick"], grp["price"], label=RESOURCE_NAMES.get(int(rid), str(rid)), alpha=0.85)
    else:
        for col, name in (("food_price", "FOOD"), ("wood_price", "WOOD"), ("stone_price", "STONE")):
            if col in macro.columns:
                ax.plot(macro["tick"], macro[col], label=name, alpha=0.85)
    ax.set(xlabel="tick", ylabel="price (cents)", title="Resource prices")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / "price_series.png", dpi=150)
    plt.close(fig)


def plot_gini(macro: pd.DataFrame, out_dir: Path) -> None:
    fig, ax = plt.subplots(figsize=(10, 4))
    ax.plot(macro["tick"], macro["gini"], color="crimson")
    ax.set(xlabel="tick", ylabel="Gini", title="Wealth inequality (Gini)")
    ax.set_ylim(0, max(0.05, float(macro["gini"].max()) * 1.2))
    fig.tight_layout()
    fig.savefig(out_dir / "gini.png", dpi=150)
    plt.close(fig)


def plot_wealth_population(macro: pd.DataFrame, out_dir: Path) -> None:
    fig, ax1 = plt.subplots(figsize=(10, 4))
    ax1.plot(macro["tick"], macro["population"], color="steelblue", label="population")
    ax1.set_xlabel("tick")
    ax1.set_ylabel("population", color="steelblue")
    ax2 = ax1.twinx()
    wealth_col = "money_supply" if "money_supply" in macro.columns else "price_index"
    ax2.plot(macro["tick"], macro[wealth_col], color="darkgreen", label=wealth_col)
    ax2.set_ylabel(wealth_col, color="darkgreen")
    ax1.set_title("Population and money supply")
    fig.tight_layout()
    fig.savefig(out_dir / "wealth_population.png", dpi=150)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot Aequitas headless CSV output.")
    parser.add_argument(
        "--out",
        type=Path,
        required=True,
        help="Directory containing macro.csv and trades.csv",
    )
    args = parser.parse_args()
    out_dir = args.out.resolve()
    macro, trades = load_tables(out_dir)
    plot_price_series(macro, trades, out_dir)
    plot_gini(macro, out_dir)
    plot_wealth_population(macro, out_dir)
    print(f"Wrote PNGs to {out_dir}")


if __name__ == "__main__":
    main()
