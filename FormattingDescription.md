# DAB Scripting Standards (v2)

---

## 1. Variable Naming

Prefix all **member variables** with a type identifier. This improves scanability and makes variables easier to locate via global search.

> We intentionally deviate from BI conventions (only for arrays and maps) to improve searchability.

| Prefix | Type                  | Example                 |
| ------ | --------------------- | ----------------------- |
| `m_a`  | Array                 | `m_aPoseModifications`  |
| `m_m`  | Map                   | `m_mBaseRotationCache`  |
| `m_s`  | String / ResourceName | `m_sBoneName`           |
| `m_b`  | Boolean               | `m_bApplyModifications` |
| `m_f`  | Float                 | `m_fScale`              |
| `m_v`  | Vector                | `m_vRotationOffset`     |

### Naming clarity

* Prefer **fully written, descriptive names** over abbreviations.
* Example: use `rotation` instead of `rot`, `position` instead of `pos`.
* Abbreviations are acceptable only when:

  * They are widely understood (`id`, `ui`, etc.), or
  * The full name becomes excessively long and harms readability.

### Scope

* Prefix rules apply **only to member variables**.
* Local variables and parameters should use clean, readable names without prefixes.

---

## 2. Formatting & Structure

### One-liners

Simple getters, setters, and trivial early returns may be written on a single line to reduce vertical bloat.

> Deviates from BI style intentionally.

Use judgment — if readability suffers, expand to multi-line.

### Variable declarations

* Do not vertically align declarations.
* Use a single space between type, name, and assignment.

### Separators

Use visual separators to improve readability in the Enfusion Workbench editor.

```cpp
// Function separator — place above every function:
//-----------------------------------------------------------------------

// Section separator — place between logical groups:
// ── Private: CreateNewConfig stages ──────────────────────────────────
```

These are intentionally **mandatory**, as they significantly improve navigation in the editor.

---

## 3. Minimising Nesting

Deep nesting reduces readability and makes code harder to reason about.

Prefer flatter control flow **when it improves clarity**.

### Guidelines

* **Guard clauses first** — validate and exit early.
* **Use `continue` in loops** to skip invalid cases.
* Avoid wrapping large blocks in `if` statements when only one path matters.

### Preferred

```cpp
if (!modificationsArr)
{
    RedrawOverlay();
    return;
}

for (int i = 0; i < modificationsArr.Count(); i++)
{
    BaseContainer entry = modificationsArr.Get(i);

    DAB_BoneTransform newTransform = CreateNewTransform(compoundBoneName);
    if (!newTransform) continue;

    // Apply values
}

RedrawOverlay();
```

### Acceptable

Small, shallow nesting is fine when it improves readability.

The goal is not “no nesting”, but **avoiding unnecessary nesting**.

---

## 4. Documentation & Comments

### Public functions

Public APIs should generally include a doc comment using `//!`:

```cpp
//! Enables or disables the component logic at runtime.
void SetEnabled(bool state) { m_bEnableLogic = state; }
```

### Protected / private functions

* Prefer inline comments over doc comments.
* Add documentation only when the behavior is non-obvious or complex.

### Inline comments

Comments should explain:

* **Why** something is done
* Engine quirks, limitations, or workarounds
* Non-obvious intent

Avoid restating what the code already clearly expresses.

```cpp
// BAD: Loop through the names array
// GOOD: Workbench fails to clear this buffer on deselect, causing memory leaks
```

### Print statements

* Always include a `LogLevel`
* Always prefix with `ClassName.FunctionName:`

```cpp
PrintFormat("DAB_Example.OnButtonPressed: Early exit — reason.", LogLevel.WARNING);
```

* Debug prints must be removed or marked:

```cpp
// TODO: Remove Debug Print
```

* UI-triggered early exits should show a dialog before logging:

```cpp
Workbench.Dialog("Title", "Descriptive message");
```

---

## 5. Class Organisation

Order class members as follows:

1. **`[Attribute]` fields** — grouped and sorted
2. **Member variables** — internal state
3. **Override methods**
4. **Public methods**
5. **Protected methods**

### Notes

* Group methods by logical purpose within each section
* Keep related functionality together
* Use section separators to clearly mark boundaries

---

## Example

```cpp
class DAB_ExampleComponent : ScriptComponent
{
    [Attribute("1")]
    protected bool m_bEnableLogic;

    protected ref array<string> m_aNames = {};

    // ── Public ────────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    //! Enables or disables the component logic at runtime.
    void SetEnabled(bool state) { m_bEnableLogic = state; }

    // ── Private ───────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    protected void InternalProcess()
    {
        if (!m_bEnableLogic) return;

        m_aNames.Clear(); // Workbench fails to clear this buffer when the entity is deselected, causing memory leaks.
    }
}
```