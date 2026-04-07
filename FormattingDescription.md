# DAB Scripting Standards

## 1. Variable Naming
Variables must use specific prefixes to make their underlying data types identifiable at a glance. This improves scannability and makes global searches more efficient.
We deviate from BI on arrays and maps, to make variable easier to search for.

* **`m_a[Name]`**: Arrays (e.g., `m_aPoseModifications`).
* **`m_m[Name]`**: Maps (e.g., `m_mBaseRotationCache`).
* **`m_s[Name]`**: Strings or ResourceNames (e.g., `m_sBoneName`, `m_sWorkingPoseModification`).
* **`m_b[Name]`**: Booleans (e.g., `m_bApplyModifications`).
* **`m_f[Name]`**: Floats (e.g., `m_fScale`).
* **`m_v[Name]`**: Vectors (e.g., `m_vRotationOffset`).

## 2. Formatting & Structure
* **One-Liners**: Simple `return` statements, `setters`, or `getters` should be kept on a single line to reduce vertical bloat and keep the code clean. (This deviates from BIs code)
* **Separation**: Use a horizontal rule of dashes to clearly separate functions.
    * Example: Over each function: `//-----------------------------------------------------------------------`.
    * Example: To seperate public from private functions or similar:  `// ── Private: CreateNewConfig stages ─────────────────────────────────── `.

## 3. Documentation & Comments
* **Public API**: Every `public` function **MUST** have a documentation comment starting with `//!` explaining its purpose.
* **Internal Logic**: `protected` or `private` functions **MUST NOT** have documentation comments.
* **Commenting Philosophy**: 
    * No describing comments. Do not explain *what* the code is doing (e.g., "looping through array").
    * Comments are only permitted to explain **WHY** the code is there, such as fixing specific engine bugs or handling non-obvious editor behaviors.

## 4. Class Organization
* **Attributes**: Place all `[Attribute]` fields at the top of the class for easy configuration within the Workbench.
* **Member Variables**: Group internal variables (non-attributes) immediately below the attributes.
* **Methods**: Organize methods by their access level or lifecycle: `override` methods first, followed by `public`, then `protected`.

---

### Example Implementation
```cpp
class DAB_ExampleComponent : ScriptComponent
{
    [Attribute("1")]
    protected bool m_bEnableLogic;

    protected ref array<string> m_aNames = {};

    // ── Public ────────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    //! Public function: Documentation required.
    void SetEnabled(bool state) { m_bEnableLogic = state; }

    // ── Private ───────────────────────────────────────────────────────────
    //-----------------------------------------------------------------------
    protected void InternalProcess()
    {
        if (!m_bEnableLogic) return;
        
        m_aNames.Clear(); //The Workbench fails to clear this buffer when the entity is deselected, leading to memory leaks.
    }
}