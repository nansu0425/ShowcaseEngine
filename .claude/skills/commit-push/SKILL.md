---
name: commit-push
description: Review changes against project conventions, auto-fix issues, then commit and push
disable-model-invocation: false
argument-hint: [branch (optional)]
---

Review all uncommitted changes against project conventions and for bugs, auto-fix issues, then commit and push.

## Phase 1: Gather Context

1. Run `git status` (never use `-uall`), `git diff`, and `git diff --name-only` to understand all changes. If there are no changes, report that and stop.
2. Read the following project files for review context:
   - `CLAUDE.md` (project rules and gotchas)
   - `docs/CODING_CONVENTION.md` (naming, formatting, style rules)
   - `docs/ARCHITECTURE.md` (only if the diff includes new/renamed/deleted files, changes to CMakeLists, class hierarchies, or include structures)

## Phase 2: Code Review & Auto-Fix

3. Launch **3 parallel review agents** (use the Agent tool). Provide each agent with the full `git diff` output, the list of changed files, and the contents of `CLAUDE.md` and `docs/CODING_CONVENTION.md`.

   **Agent 1 — Convention Compliance:**
   Check all changed code against `docs/CODING_CONVENTION.md`:
   - Naming: PascalCase types/functions, camelCase locals, `m_` private members, `s_` statics, `k` prefix constexpr constants, PascalCase enum values, PascalCase filenames
   - Formatting: K&R braces, 4-space indent, 120 char max, braces always (except single-line early return)
   - Headers: `#pragma once`, include order (corresponding → project → third-party → platform → stdlib), forward declarations preferred
   - `auto` usage: only for pointer-from-getter, iterators, range-based for, structured bindings
   - Comments: `//` inline, `///` for non-obvious public API (no @param/@return tags), section banners `// ──`
   - Function parameters: context first, input next, output last with `out` prefix
   - `[[nodiscard]]` on `bool Init()` / `Load()` functions
   - `using` aliases instead of `typedef`
   - `enum class` not plain `enum`
   - No indentation inside top-level namespace

   **Agent 2 — Bug Detection:**
   Scan changed code for bugs, referencing `CLAUDE.md` gotchas:
   - HLSL `cbuffer` matrices must use `row_major float4x4` (not column-major default)
   - Use LH matrix functions (`XMMatrixLookAtLH` / `XMMatrixPerspectiveFovLH`), not SimpleMath RH defaults
   - Constant buffer writes must use offset-based approach (`UpdateDataAtOffset`), not single-buffer rewrite
   - Shader paths must resolve relative to exe directory
   - `HRESULT` return values must be checked
   - `ComPtr<>` ownership correctness
   - Null/dangling pointer access risks
   - Missing fence synchronization before GPU resource access
   - Use-after-move, off-by-one, missing break in switch

   **Agent 3 — Architecture Alignment:**
   Check structural rules from `CLAUDE.md` and `docs/ARCHITECTURE.md`:
   - Engine (`engine/`) must NOT depend on ImGui — only editor may use ImGui
   - All code in `showcase` namespace
   - Public headers use `<showcase/module/File.h>` include paths
   - One primary type per file
   - Shader output naming: `{NAME_WE}_{type}.cso`
   - Engine-editor dependency direction (editor depends on engine, not reverse)

   Each agent must return issues in this format:
   ```
   - File: <path>
     Line: <number>
     Issue: <description>
     Rule: <quote from convention/CLAUDE.md>
     Fix: <concrete code change>
     Confidence: <0-100>
   ```

   **Confidence scoring rubric** (provide this to each agent):
   - 0: False positive — does not hold up to scrutiny, or pre-existing issue in unchanged code
   - 25: Might be real but unverified; stylistic issue not explicitly in CODING_CONVENTION.md
   - 50: Real issue but minor nitpick, not important relative to the change
   - 75: Verified real issue — directly impacts code quality/functionality, or explicitly mentioned in conventions
   - 100: Confirmed — definite bug or clear convention rule violation

   **False positives to exclude** (provide this to each agent):
   - Pre-existing issues in lines NOT modified by the current changes
   - Style preferences not explicitly documented in CODING_CONVENTION.md or CLAUDE.md
   - Issues the MSVC compiler or CMake build would catch (type errors, missing includes, linker errors)
   - Intentional deviations marked with explanatory comments
   - Changes in functionality that are clearly intentional

4. Collect all issues from the 3 agents. **Filter out any issue with confidence below 80.** If no issues remain, skip to Phase 3.

5. **Apply auto-fixes** using the Edit tool:
   - For each issue with confidence ≥ 80, apply the suggested fix
   - Group fixes by file to minimize edit operations
   - Report all fixes applied in this format:
     ```
     ## Pre-Commit Review
     Found N issues:
     1. [File:Line] Description (Convention: "rule quote") [Confidence: XX] → Fixed
     ```

6. **Re-review (max 1 cycle):**
   - Run `git diff` on only the files modified during fixing
   - Launch a single agent to verify fixes are correct and no new issues were introduced
   - If new issues found with confidence ≥ 80, apply those fixes
   - **Hard stop after this re-review** — do not loop further
   - If issues remain after re-review, report them and proceed to commit

## Phase 3: Commit & Push

7. Run `git status` and `git diff` to see the final state (original changes + review fixes)
8. Run `git log --oneline -5` to match the repository's commit message style
9. Stage relevant files with `git add <specific files>` (never `git add -A` or `git add .`)
10. Analyze all changes and draft a concise commit message (1-2 sentences) focusing on the "why" not the "what". Create the commit using a HEREDOC:
    ```
    git commit -m "$(cat <<'EOF'
    Commit message here.

    Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
    EOF
    )"
    ```
11. Push to remote: `git push` (or `git push origin $ARGUMENTS` if a branch name was provided)
12. Report the final result:
    ```
    ## Committed and Pushed
    Commit: <hash> | Branch: <branch> → origin/<branch>
    Message: "<commit message>"
    Review: N issues found, M auto-fixed
    ```

## Rules

- Do NOT commit files that likely contain secrets (.env, credentials.json, etc.)
- Do NOT use `git add -A` or `git add .` — always add specific files
- Do NOT amend existing commits
- Do NOT force push
- If there are no changes to commit, report that and stop
- Maximum 2 review iterations (initial review + 1 re-review) to prevent infinite loops
