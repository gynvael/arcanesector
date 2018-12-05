// Included in engine.cc.

// Note: Use the following "namespaces" (prefixes):
// "3d_" for textures, images needed only in 3D rendering
// "ui_" for general UI stuff
// "con_" for console stuff
// Note that the "global namespace" will be polluted by item textures carved out
// from sprites.

{
  // 3D textures and sprites.
  { "3d_sky", "gfx/sky1.png" },
  { "3d_dirtA", "gfx/dirt1A.png" },
  { "3d_dirtB", "gfx/dirt1B.png" },
  { "3d_dirtC", "gfx/dirt1C.png" },
  { "3d_dirtD", "gfx/dirt1D.png" },
  { "3d_wood", "gfx/wood1.png" },
  { "3d_rock", "gfx/rock6.png" },
  { "3d_rockL", "gfx/rock6_light.png" },
  { "3d_rockD", "gfx/rock6_dark.png" },
  { "3d_waterA", "gfx/waterA.png" },
  { "3d_waterB", "gfx/waterB.png" },
  { "3d_waterC", "gfx/waterC.png" },
  { "3d_waterD", "gfx/waterD.png" },
  { "3d_grassA", "gfx/grass2A.png" },
  { "3d_grassB", "gfx/grass2B.png" },
  { "3d_grassC", "gfx/grass2C.png" },
  { "3d_grassD", "gfx/grass2D.png" },
  { "3d_wall", "gfx/wall.png" },
  { "3d_wall_door", "gfx/wall_door.png" },
  { "3d_sandA", "gfx/sand1A.png" },
  { "3d_sandB", "gfx/sand1B.png" },
  { "3d_sandC", "gfx/sand1C.png" },
  { "3d_sandD", "gfx/sand1D.png" },
  { "3d_deadtree1", "gfx/Tree_Dead.png" },
  { "3d_deadtree2", "gfx/Tree_Dead_2.png" },
  { "3d_deadtree3", "gfx/Tree_Dead_3.png" },
  { "3d_pinetree1", "gfx/Tree_Pine1.png" },
  { "3d_pinetree2", "gfx/Tree_Pine2.png" },
  { "3d_pinetree3", "gfx/Tree_Pine3.png" },
  { "3d_pinetree4", "gfx/Tree_Pine4.png" },
  { "3d_pinetree5", "gfx/Tree_Pine5.png" },
  { "3d_tree_leaf_large", "gfx/Tree_Leaf_Large.png" },
  { "3d_tree_leaf_huge", "gfx/Tree_Leaf_Huge.png" },
  { "3d_tree_leaf_medium", "gfx/Tree_Leaf_Medium.png" },
  { "3d_tree_leaf_medium2", "gfx/Tree_Leaf_Medium2.png" },
  { "3d_tree_leaf_verylarge", "gfx/Tree_Leaf_VeryLarge.png" },
  { "3d_bush1", "gfx/Bush_1.png" },
  { "3d_bush2", "gfx/Bush_2.png" },
  { "3d_bush3", "gfx/Bush_3.png" },
  { "3d_rocky_roadA", "gfx/rocky_roadA.png" },
  { "3d_rocky_roadB", "gfx/rocky_roadB.png" },
  { "3d_rocky_roadC", "gfx/rocky_roadC.png" },
  { "3d_rocky_roadD", "gfx/rocky_roadD.png" },
  { "3d_mob_drow_f", "gfx/Female_Character_3.png" },
  { "3d_mob_drow_m", "gfx/Male_Character_3.png" },
  { "switch_on", "gfx/switch_on.png" },
  { "switch", "gfx/switch_off.png" },
  { "blocker", "gfx/wall.png" },
  { "blocker_open", "gfx/gold_frame.png" },

  // UI stuff.
  { "ui_map_bg", "gfx/map_bg.png" },
  { "ui_bg", "gfx/ui.png" },
  { "ui_map_icons", "gfx/map_icons.png" },
  { "ui_map_dir", "gfx/map_dir.png" },
  { "ui_font", "gfx/simple_6x8.png" },
  { "ui_rune_bg", "gfx/rune_bg.png" },
  { "ui_runes", "gfx/runes.png" },
  { "ui_hud_inv", "gfx/UI_frame_main_inventory_B.png" },
  { "ui_hud_spell", "gfx/UI_frame_main_spell_B.png" },
  { "ui_hand_left", "gfx/Empty_Left_hand.png" },
  { "ui_hand_right", "gfx/Empty_Right_hand.png" },
  { "ui_border_A", "gfx/decorative_frame.png" },
  { "ui_border_B", "gfx/decorative_frame_B.png" },
  { "ui_dir_frame", "gfx/direction_Frame.png" },
  { "ui_dir_N", "gfx/direction_North.png" },
  { "ui_dir_W", "gfx/direction_West.png" },
  { "ui_dir_S", "gfx/direction_South.png" },
  { "ui_dir_E", "gfx/direction_East.png" },
  { "ui_bar_mana", "gfx/UI_Bar_pink.png" },
  { "ui_bar_health", "gfx/UI_Bar_green.png" },
  { "ui_bar_mana_bg", "gfx/UI_Bar_pink_bg.png" },
  { "ui_bar_health_bg", "gfx/UI_Bar_green_bg.png" },
  { "ui_button_on", "gfx/button_24x24_onC.png" },
  { "ui_button_over", "gfx/button_24x24_overC.png" },
  { "ui_button_off", "gfx/button_24x24_off.png" },
  { "ui_portrait_f", "gfx/Female_portrait_3.png" },
  { "ui_portrait_m", "gfx/Male_portrait_3.png" },
  { "ui_select_cursor", "gfx/circle_dot_hit.png" },
  { "ui_icon_cast_hover", "gfx/CastingSpell_hover.png" },
  { "ui_icon_cast_idle", "gfx/CastingSpell_idle.png" },
  { "ui_icon_cast_pressed", "gfx/CastingSpell_pressed.png" },
  { "ui_icon_inv_hover", "gfx/Inventory_hover.png" },
  { "ui_icon_inv_idle", "gfx/Inventory_idle.png" },
  { "ui_icon_inv_pressed", "gfx/Inventory_pressed.png" },
  { "ui_icon_magic_hover", "gfx/Magic_hover.png" },
  { "ui_icon_magic_idle", "gfx/Magic_idle.png" },
  { "ui_icon_magic_pressed", "gfx/Magic_pressed.png" },

  // Console stuff.
  { "con_bg", "gfx/con_bg.png" },

  // Items.
  { "items_1", "gfx/roguelikeitems.png" },
  { "sign", "gfx/sign.png" },
  { "lamppost", "gfx/lamp-1-post-1-tall-single-alt-blue.png" },

  // The end.
  { nullptr, nullptr }
};

