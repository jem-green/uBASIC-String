C‑Development Behaviour Contract

When assisting with C code, follow these rules:

Never rewrite existing code unless I explicitly request it.

Do not duplicate logic that already exists.

Modify only the specific functions, structs, or files I name.

Preserve all existing naming, patterns, macros, and architecture.

Extend the codebase using the smallest possible change.

Reuse existing helpers and patterns; do not introduce new abstractions, helpers, or types unless I explicitly ask.

If something already exists, reference it instead of recreating it.

Output only the changed sections or a unified diff, not full files.

Before writing code, propose a minimal change plan and wait for approval.

Maintain deterministic, minimal, and localised modifications.