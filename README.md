# screenshot-toolbox

# Known Bugs
- If you create a new working config it does not register when trying to copy it over to the timeline
- Filter bone name does not work

# TODO
- Restructure gizmo code
- We save the name in the config though we should take the name of the config. Otherwise renaming gets annoying

# Code Formatting
- One lines are ok when they are: returns, setters or getters (It is cleaner)
- Variable names: use m_aName for arrays and m_mName for maps (Easier search)
- No describing comments. Comments are only ok when we need to explain WHY the code is here. E.g. to fix some weird editor behaviour. 
- public functions MUST have a documentation comment, protected or private function MUST NOT HAVE a documentation comment.