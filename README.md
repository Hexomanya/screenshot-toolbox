# screenshot-toolbox

# Known Bugs
- We save the name in the config though we should take the name of the config. Otherwise renaming gets annoying
- The rotation gizmo gets the wrong rotation. Maybe after the parent moves
- When using anims, the overlay sometimes breaks
- When resetting a bone, we do not reset to the original anim rotation, but zero it
- If you create a new working config it does not register when trying to copy it over to the timeline
- When recompiling while tool is open we loose the skeletons
- For some reason the face breaks sometimes