#!/usr/bin/env python3
"""
Migrate ig_people.xlsx and photographers.txt into the unified ig_people.json
used by the Qt C++ application.

Uses pandas (already installed) — no extra dependencies needed.

Usage: run from the IG_Sorter directory:
    python migrate_database.py
"""

import json
import sys
import os
import pandas as pd


def migrate(xlsx_path: str, txt_path: str, json_path: str) -> list[dict]:
    """Perform the migration."""
    # --- Read XLSX ---
    df = pd.read_excel(xlsx_path, engine="openpyxl")

    # Normalize column names to lowercase
    df.columns = [str(c).strip().lower() for c in df.columns]

    # Find the account and name columns (flexible matching)
    account_col = None
    name_col = None
    for col in df.columns:
        if "account" in col:
            account_col = col
        if "name" in col:
            name_col = col

    if name_col is None:
        print("ERROR: Could not find a 'Name' column in ig_people.xlsx")
        print(f"  Available columns: {list(df.columns)}")
        sys.exit(1)

    # --- Read photographers.txt ---
    photographer_accounts: set[str] = set()
    if os.path.isfile(txt_path):
        with open(txt_path, "r", encoding="utf-8") as f:
            photographer_accounts = {line.strip().lower() for line in f if line.strip()}

    # --- Build JSON entries ---
    json_entries: list[dict] = []
    seen_accounts: set[str] = set()

    for _, row in df.iterrows():
        acct_raw = str(row[account_col]).strip() if account_col is not None and pd.notna(row.get(account_col)) else ""
        name_raw = str(row[name_col]).strip() if pd.notna(row.get(name_col)) else ""

        if not name_raw or name_raw.lower() in ("nan", "none", ""):
            continue

        acct_lower = acct_raw.lower() if acct_raw and acct_raw.lower() not in ("nan", "none", "") else ""

        # Determine type
        if acct_lower in photographer_accounts:
            entry_type = "curator"
        elif acct_lower:
            entry_type = "personal"
        else:
            entry_type = "irl_only"

        # Avoid duplicate accounts
        if acct_lower and acct_lower in seen_accounts:
            print(f"  SKIP duplicate account: {acct_raw} -> {name_raw}")
            continue
        if acct_lower:
            seen_accounts.add(acct_lower)

        json_entries.append({
            "account": acct_raw if acct_lower else None,
            "name": name_raw,
            "type": entry_type,
        })

    # Add any photographers that were NOT in the xlsx
    for acct in sorted(photographer_accounts):
        if acct not in seen_accounts:
            print(f"  ADDING photographer-only account: {acct}")
            json_entries.append({
                "account": acct,
                "name": "",
                "type": "curator",
            })
            seen_accounts.add(acct)

    # Write JSON
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(json_entries, f, indent=2, ensure_ascii=False)

    return json_entries


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    xlsx_path = os.path.join(script_dir, "ig_people.xlsx")
    txt_path = os.path.join(script_dir, "photographers.txt")
    json_path = os.path.join(script_dir, "ig_people.json")

    if not os.path.isfile(xlsx_path):
        print(f"ERROR: {xlsx_path} not found.")
        sys.exit(1)

    print("=== IG_Sorter Database Migration ===")
    print()
    print(f"  Source XLSX:  {xlsx_path}")
    print(f"  Source TXT:   {txt_path}")
    print(f"  Output JSON:  {json_path}")
    print()

    entries = migrate(xlsx_path, txt_path, json_path)

    print(f"\nMigration complete!")
    print(f"  Total entries in JSON: {len(entries)}")

    # Summary by type
    type_counts: dict[str, int] = {}
    for e in entries:
        t = e.get("type", "unknown")
        type_counts[t] = type_counts.get(t, 0) + 1
    for t, count in sorted(type_counts.items()):
        print(f"    {t}: {count}")

    # Warn about curator entries with empty names
    empty_names = [e for e in entries if e["type"] == "curator" and not e.get("name")]
    if empty_names:
        print(f"\nWARNING: {len(empty_names)} curator account(s) have no IRL name:")
        for e in empty_names:
            print(f"    • {e['account']}")
        print("  You will need to fill in these names in the app or by editing the JSON.")

    print(f"\nDone! Update your Qt C++ app settings to point to:\n  {json_path}")


if __name__ == "__main__":
    main()
