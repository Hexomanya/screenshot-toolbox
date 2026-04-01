# screenshot-toolbox

# Known Bugs
- If so switch the config while editing the bone, editin breaks.
- Reset bones (bones with all default values) do still get saved once they are in the config.
- We save the name in the config though we should take the name of the config. Otherwise renaming gets annoying
- The rotation gizmo gets the wrong rotation. Maybe after the parent moves
- When using anims, the overlay sometimes breaks
- When resetting a bone, we do not reset to the original anim rotation, but zero it
- If you create a new working config it does not register when trying to copy it over to the timeline
- Rerender on onEnter