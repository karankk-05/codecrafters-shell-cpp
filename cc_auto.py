#!/usr/bin/env python3
from __future__ import annotations
"""
CodeCrafters Automation Script
===============================
Reusable automation for the CodeCrafters workflow:
  1. mark_complete(stage_slug) - Marks a stage as complete on the website
  2. get_current_task()        - Gets current task instructions via CLI
  3. submit()                  - Submits code via CLI and returns test output
  4. full_cycle(stage_slug)    - submit + mark_complete in one call

Usage:
  python3 cc_auto.py mark <stage_slug>    # e.g. python3 cc_auto.py mark ei0
  python3 cc_auto.py task                 # Print current task
  python3 cc_auto.py submit               # Submit and print results
  python3 cc_auto.py cycle <stage_slug>   # Submit + mark complete

Requires: playwright (pip install playwright && playwright install chromium)
"""

import asyncio
import subprocess
import sys
import os

# ──────────────────────────────────────────────────────────────────────
# Configuration – update these if your credentials change
# ──────────────────────────────────────────────────────────────────────
SESSION_TOKEN = "cb645fab47ae7d5e490909c399f8f48708819d06dd56e136b10de46c2c1e7e04b4907039afe43b06f7e743d27b1825372b9dc4b8a5a11d1aee22849423892432"
USER_ID = "fe54778b-33f9-4002-8999-8dabd4d27de2"
USERNAME = "karankk-05"
BASE_URL = "https://app.codecrafters.io"
COURSE = "shell"  # Change this when switching to a different challenge

# Repo root – auto-detected from this script's location
REPO_ROOT = os.path.dirname(os.path.abspath(__file__))


# ──────────────────────────────────────────────────────────────────────
# 1. Mark a stage as complete using headless Playwright
# ──────────────────────────────────────────────────────────────────────
async def _mark_complete_async(stage_slug: str, course: str = COURSE) -> bool:
    """
    Opens the stage page in a headless browser, injects the session token,
    and clicks the 'Mark stage as complete' button.
    Returns True on success.
    """
    from playwright.async_api import async_playwright

    url = f"{BASE_URL}/courses/{course}/stages/{stage_slug}"
    print(f"[mark_complete] Opening {url} ...")

    async with async_playwright() as p:
        browser = await p.chromium.launch(headless=True)
        context = await browser.new_context(viewport={"width": 1280, "height": 800})
        page = await context.new_page()

        # Step 1: Load the site so we can write to localStorage
        await page.goto(BASE_URL, wait_until="domcontentloaded")
        await page.evaluate(f"""() => {{
            localStorage.setItem('cc-frontend:session_token_v1', '{SESSION_TOKEN}');
            localStorage.setItem('cc-frontend:current_user_cache_v1:user_id', '{USER_ID}');
            localStorage.setItem('cc-frontend:current_user_cache_v1:username', '{USERNAME}');
        }}""")

        # Step 2: Navigate to the stage page
        await page.goto(url)
        await page.wait_for_timeout(6000)  # Let Ember app fully render

        # Step 3: Try clicking "Tests passed!" banner to open the popup
        try:
            banner = page.locator('text="Tests passed!"')
            if await banner.count() > 0:
                await banner.click()
                await page.wait_for_timeout(2000)
        except Exception:
            pass

        # Step 4: Find and click "Mark stage as complete"
        clicked = False
        try:
            elements = await page.locator("button, a, div[role='button']").all()
            for el in elements:
                text = (await el.inner_text()).strip().lower()
                if "mark stage as complete" in text:
                    print(f"[mark_complete] Found button! Clicking ...")
                    await el.click()
                    await page.wait_for_timeout(5000)
                    clicked = True
                    break
        except Exception as e:
            print(f"[mark_complete] Error: {e}")

        if not clicked:
            print("[mark_complete] ⚠  Could not find 'Mark stage as complete' button.")
            print("[mark_complete]    The stage may not have passed tests yet,")
            print("[mark_complete]    or the page structure has changed.")
            # Take a debug screenshot
            debug_path = os.path.join(REPO_ROOT, "cc_debug_screenshot.png")
            await page.screenshot(path=debug_path)
            print(f"[mark_complete]    Debug screenshot saved to {debug_path}")
        else:
            print(f"[mark_complete] ✅ Stage '{stage_slug}' marked as complete!")

        await browser.close()
        return clicked


def mark_complete(stage_slug: str, course: str = COURSE) -> bool:
    """Synchronous wrapper for _mark_complete_async."""
    return asyncio.run(_mark_complete_async(stage_slug, course))


# ──────────────────────────────────────────────────────────────────────
# 2. Get current task instructions
# ──────────────────────────────────────────────────────────────────────
def get_current_task() -> str:
    """Runs `codecrafters task` and returns the output."""
    result = subprocess.run(
        ["codecrafters", "task"],
        capture_output=True, text=True, cwd=REPO_ROOT
    )
    return result.stdout


# ──────────────────────────────────────────────────────────────────────
# 3. Submit code
# ──────────────────────────────────────────────────────────────────────
def submit() -> tuple[bool, str]:
    """
    Runs `codecrafters submit` and returns (passed, output).
    """
    print("[submit] Submitting code ...")
    result = subprocess.run(
        ["codecrafters", "submit"],
        capture_output=True, text=True, cwd=REPO_ROOT
    )
    output = result.stdout + result.stderr
    passed = "Test passed." in output or "Congrats!" in output
    status = "✅ PASSED" if passed else "❌ FAILED"
    print(f"[submit] {status}")
    return passed, output


# ──────────────────────────────────────────────────────────────────────
# 4. Full cycle: submit + mark complete
# ──────────────────────────────────────────────────────────────────────
def full_cycle(stage_slug: str, course: str = COURSE) -> bool:
    """Submit code, and if tests pass, mark the stage as complete."""
    passed, output = submit()
    if not passed:
        print("[full_cycle] Tests did not pass. Skipping mark_complete.")
        print(output)
        return False

    print(f"[full_cycle] Tests passed! Marking stage '{stage_slug}' as complete ...")
    return mark_complete(stage_slug, course)


# ──────────────────────────────────────────────────────────────────────
# 5. Extract the current stage slug from task output
# ──────────────────────────────────────────────────────────────────────
def get_current_stage_slug() -> str | None:
    """Parses the current stage slug (e.g. 'ei0') from `codecrafters task` output."""
    import re
    task_output = get_current_task()
    # Looks for patterns like (#ei0) or (#ra6)
    match = re.search(r'\(#([a-zA-Z0-9]+)\)', task_output)
    if match:
        return match.group(1).lower()
    return None


# ──────────────────────────────────────────────────────────────────────
# CLI interface
# ──────────────────────────────────────────────────────────────────────
def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1].lower()

    if cmd == "mark":
        slug = sys.argv[2] if len(sys.argv) > 2 else get_current_stage_slug()
        if not slug:
            print("Error: could not determine stage slug. Pass it as an argument.")
            sys.exit(1)
        mark_complete(slug)

    elif cmd == "task":
        print(get_current_task())

    elif cmd == "submit":
        passed, output = submit()
        print(output)
        sys.exit(0 if passed else 1)

    elif cmd == "cycle":
        slug = sys.argv[2] if len(sys.argv) > 2 else get_current_stage_slug()
        if not slug:
            print("Error: could not determine stage slug. Pass it as an argument.")
            sys.exit(1)
        success = full_cycle(slug)
        sys.exit(0 if success else 1)

    elif cmd == "slug":
        slug = get_current_stage_slug()
        print(slug or "Could not determine stage slug")

    else:
        print(f"Unknown command: {cmd}")
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
