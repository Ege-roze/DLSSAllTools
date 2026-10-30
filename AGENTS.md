# Codex instructions for DLSSAllTools

## Attribution in Git commits

When Codex materially contributes to changes included in a Git commit, add this trailer to the **end of the commit message**, separated from the description by a blank line:

```text
Co-authored-by: Codex <codex@openai.com>
```

Only use this attribution for work Codex actually helped produce. Do not add the trailer to unrelated or purely human-authored commits. Do not rewrite or force-push existing history just to add attribution.

This repository instruction applies when Codex prepares the commit. If a developer creates commits manually in Visual Studio or GitHub, they must add the same trailer to the commit description themselves; an AGENTS.md file cannot alter local Git hooks or Visual Studio commit settings.

Keep attribution edits separate from runtime behavior changes.
