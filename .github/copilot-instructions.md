\# uBasic-String — Project-Level Copilot Instructions



These rules apply only within this repository and override global Copilot instructions.  

Language-specific rules (e.g., C development constraints) are defined in separate instruction files.



\---



\## 1. Project Identity

This repository implements:

\- A Tiny BASIC–derived language (Pittman-style)

\- A top down descendent parser (Jack Crenshaw)

\- A uBasic based interpreter (Adam Dunkels)

\- A deterministic integer interpreter
\- A deterministic string interpreter

\- A token-driven execution model



All AI-generated output must respect these constraints.



\---



\## 2. File and Architecture Boundaries

\- Do not invent new modules, files, or subsystems.

\- Modify only the files explicitly referenced by the user.

\- Preserve the existing directory structure.

\- Maintain the interpreter’s architectural philosophy: small, explicit, deterministic components.



\---



\## 3. Language-Specific Rules

If a C development contract exists in:

\- `.github/instructions/C\_DEVELOPMENT\_CONTRACT.md`, or

\- `COPILOT\_C\_CONTRACT.md`



…then those rules MUST be followed for all C-related tasks.



Do not apply C rules to other languages (Arduino, C#, IL descriptions, etc.).



\---



\## 4. uBasic Grammar Rules (Project-Specific)

The interpreter implements a uBasic dialect with:

\- Line-numbered statements

\- Integer expressions

\- String expressions

\- Deterministic tokenisation

\- No floating point

\- No implicit type coercion

\- No dynamic memory allocation



When generating grammar, parsing logic, or examples:

\- Follow Pittman-style Tiny BASIC semantics.

\- Follow Adam Dunkels uBasic BASIC semantics.

\- Do not introduce new keywords or syntax.

\- Maintain compatibility with the existing interpreter.



\---



\## 5. Tokenisation Rules

Tokenisation must be:

\- Deterministic

\- Reversible

\- Free of ambiguity

\- Stable across runs



Tokens must:

\- Preserve identity

\- Carry explicit type information

\- Follow the existing token enum and layout

\- Never be merged, split, or reordered unless explicitly requested



When generating or modifying token logic:

\- Reuse existing token definitions

\- Do not invent new token types without approval



\---



\## 6. Interpreter Behaviour Rules

Interpreter behaviour must remain:

\- Deterministic

\- Integer-only

\- Line-number driven

\- Single-pass tokenisation

\- Predictable control flow



Do not introduce:

\- Floating point

\- Dynamic typing

\- Implicit conversions

\- New control structures

\- New keywords



\---



\## 7. Change Management Rules

Before generating code:

1\. Summarise the minimal change plan.

2\. Wait for explicit approval.

3\. Apply only the approved changes.

4\. Output diffs or changed sections only.



Do not:

\- Rewrite entire files

\- Refactor unrelated code

\- Introduce new abstractions

\- Duplicate existing logic



\---



\## 8. Documentation and Explanation Rules

When explaining interpreter, IL, or grammar behaviour:

\- Use precise terminology

\- Avoid ambiguity

\- Provide structured, stepwise reasoning

\- Reference existing project structures

\- Do not invent undocumented behaviour



\---



\# End of Project-Level Instructions



