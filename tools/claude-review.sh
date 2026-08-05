#!/usr/bin/env bash
#
# claude-review.sh — get a second-opinion review from Claude Code (headless, read-only).
#
# Usage:
#   ./tools/claude-review.sh diff              # review uncommitted changes (git diff HEAD)
#   ./tools/claude-review.sh staged            # review staged changes only
#   ./tools/claude-review.sh range <A..B>      # review a commit range
#   echo "<plan text>" | ./tools/claude-review.sh plan   # critique a proposed plan (stdin)
#
# Runs Claude in plan (read-only) mode: it can open files, grep, and build to
# VERIFY claims against the codebase, but it cannot modify any files. Output is a
# severity-ranked critique. Pin a model with CLAUDE_REVIEW_MODEL, e.g.:
#   CLAUDE_REVIEW_MODEL=opus ./tools/claude-review.sh diff

set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

mode="${1:-diff}"
prompt_file="tools/claude-reviewer-prompt.md"
[ -f "$prompt_file" ] || { echo "missing $prompt_file" >&2; exit 1; }

case "$mode" in
  diff)   payload="$(git diff HEAD)";                                         label="uncommitted diff (git diff HEAD)";;
  staged) payload="$(git diff --staged)";                                     label="staged diff";;
  range)  payload="$(git diff "${2:?usage: claude-review.sh range <A..B>}")"; label="range ${2}";;
  plan)   payload="$(cat)";                                                   label="proposed plan (from stdin)";;
  *) echo "usage: $0 {diff|staged|range <A..B>|plan}" >&2; exit 2;;
esac

# Bail cleanly if there's nothing to look at.
if [ -z "${payload//[$'\t\r\n ']/}" ]; then
  echo "Nothing to review (empty ${label})." >&2
  exit 0
fi

model_arg=()
[ -n "${CLAUDE_REVIEW_MODEL:-}" ] && model_arg=(--model "$CLAUDE_REVIEW_MODEL")

{
  cat "$prompt_file"
  printf '\n\n=== REVIEW TARGET: %s ===\n\n' "$label"
  printf '%s\n' "$payload"
} | claude -p --permission-mode plan "${model_arg[@]}"
