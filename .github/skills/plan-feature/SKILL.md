---
name: plan-feature
description: 'Plan a new feature in plan mode and save the plan as a markdown file in the plans folder. Use when: the user says "let''s plan a new feature", "plan a feature", "plan this feature", or asks for a feature plan.'
argument-hint: '[feature name or short description]'
---

# Plan a New Feature

When the user says something like "let's plan a new feature", plan it first, then store the plan.

## Procedure

1. **Switch to plan mode.** While planning, do not edit firmware or source files. Only read files in `docs/`, `code/src/`, `code/tests/`, and `plans/` as needed. The only write allowed during this procedure is creating the plan file in Step 4.
2. **Clarify.** If the feature name or goals are unclear, ask the user (ask_user tool) before drafting. Ask with prepopulated alternatives so it's easier and quicker for the user to give feedback or take descisions.
3. **Draft the plan** with these sections:
   - Problem and goal
   - Recommended approach
   - Affected files/components (link to them)
   - Step-by-step tasks, in order
   - Risks, open questions, memory/CPU/timing considerations
   - Testing and validation (see `code/tests/`)
4. **Store the plan** as `plans\<kebab-case-feature-name>.md` in the repo root `plans` folder. Match the style of existing plans there. Never overwrite an existing plan without asking.
5. **Report** the file path and a short summary. Do not start implementing until the user approves.
