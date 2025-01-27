/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

//
// Motion Menu
//

#include "../../inc/MarlinConfigPre.h"

#if HAS_MARLINUI_MENU

#define LARGE_AREA_TEST ((X_BED_SIZE) >= 1000 || (Y_BED_SIZE) >= 1000 || (Z_MAX_POS) >= 1000)

#include "menu_item.h"
#include "menu_addon.h"

#include "../../module/motion.h"
#include "../../gcode/parser.h" // for inch support
#include "../../module/temperature.h"

#if ENABLED(MANUAL_E_MOVES_RELATIVE)
  float manual_move_e_origin = 0;
#endif

//
// "Motion" > "Move Axis" submenu
//
// // No X/Y/Z/etc. control, only extruders. Should be fine because multiple Extruders are allowed

#if E_MANUAL

  static void lcd_move_e(TERN_(MULTI_E_MANUAL, const int8_t eindex=active_extruder)) {
    if (ui.use_click()) return ui.goto_previous_screen_no_defer();
    if (ui.encoderPosition) {
      if (!ui.manual_move.processing) {
        const float diff = float(int32_t(ui.encoderPosition)) * ui.manual_move.menu_scale;
        TERN(IS_KINEMATIC, ui.manual_move.offset, current_position.e) += diff;
        ui.manual_move.soon(E_AXIS OPTARG(MULTI_E_MANUAL, eindex));
        ui.refresh(LCDVIEW_REDRAW_NOW);
      }
      ui.encoderPosition = 0;
    }
    if (ui.should_draw()) {
      TERN_(MULTI_E_MANUAL, MenuItemBase::init(eindex));
      MenuEditItemBase::draw_edit_screen(
        GET_TEXT_F(TERN(MULTI_E_MANUAL, MSG_MOVE_EN, MSG_MOVE_E)),
        ftostr41sign(current_position.e
          PLUS_TERN0(IS_KINEMATIC, ui.manual_move.offset)
          MINUS_TERN0(MANUAL_E_MOVES_RELATIVE, ui.manual_move.e_origin)
        )
      );
    } // should_draw
  }

#endif // E_MANUAL

//
// "Motion" > "Move Xmm" > "Move XYZ" submenu
//
// // Same as above, no XYZ control

#ifndef FINE_MANUAL_MOVE
  #define FINE_MANUAL_MOVE 0.025
#endif

void _goto_manual_move(const_float_t scale) {
  ui.defer_status_screen();
  ui.manual_move.menu_scale = scale;
  ui.goto_screen(ui.manual_move.screen_ptr);
  thermalManager.set_menu_cold_override(true);
}

//may have to ensure axes don't cause menu items to appear elsewhere, as E is included with others
//this is handled in types.h
//I'm going to temporarily comment out the first two lines of the switch and see if it compiles
void _menu_move_distance(const AxisEnum axis, const screenFunc_t func, const int8_t eindex=active_extruder) {
  ui.manual_move.screen_ptr = func;
  START_MENU();
  if (LCD_HEIGHT >= 4) {
    switch (axis) {
      //#define _CASE_MOVE(N) case N##_AXIS: STATIC_ITEM(MSG_MOVE_##N, SS_DEFAULT|SS_INVERT); break;
      //MAIN_AXIS_MAP(_CASE_MOVE)
      default:
        TERN_(MANUAL_E_MOVES_RELATIVE, ui.manual_move.e_origin = current_position.e);
        STATIC_ITEM_N(eindex, MSG_MOVE_EN, SS_DEFAULT|SS_INVERT);
        break;
    }
  }

  BACK_ITEM(MSG_MOVE_AXIS);
  if (parser.using_inch_units()) {
    if (LARGE_AREA_TEST) SUBMENU(MSG_MOVE_1IN, []{ _goto_manual_move(IN_TO_MM(1.000f)); });
    SUBMENU(MSG_MOVE_01IN,   []{ _goto_manual_move(IN_TO_MM(0.100f)); });
    SUBMENU(MSG_MOVE_001IN,  []{ _goto_manual_move(IN_TO_MM(0.010f)); });
    SUBMENU(MSG_MOVE_0001IN, []{ _goto_manual_move(IN_TO_MM(0.001f)); });
  }
  else {
    if (LARGE_AREA_TEST) SUBMENU(MSG_MOVE_100MM, []{ _goto_manual_move(100); });
    SUBMENU(MSG_MOVE_10MM, []{ _goto_manual_move(10);    });
    SUBMENU(MSG_MOVE_1MM,  []{ _goto_manual_move( 1);    });
    SUBMENU(MSG_MOVE_01MM, []{ _goto_manual_move( 0.1f); });
    if (axis == Z_AXIS && (FINE_MANUAL_MOVE) > 0.0f && (FINE_MANUAL_MOVE) < 0.1f)
      SUBMENU_f(F(STRINGIFY(FINE_MANUAL_MOVE)), MSG_MOVE_N_MM, []{ _goto_manual_move(float(FINE_MANUAL_MOVE)); });
  }
  END_MENU();
}

//void _menu_move_n_distance() { _menu_move_distance(AxisEnum(MenuItemBase::itemIndex), _lcd_move_axis_n); }

#if E_MANUAL

  inline void _goto_menu_move_distance_e() {
    ui.goto_screen([]{ _menu_move_distance(E_AXIS, []{ lcd_move_e(); }); });
  }

  inline void _menu_move_distance_e_maybe() {
    if (thermalManager.tooColdToExtrude(active_extruder)) {
      ui.goto_screen([]{
        MenuItem_confirm::select_screen(
          GET_TEXT_F(MSG_BUTTON_PROCEED), GET_TEXT_F(MSG_BACK),
          _goto_menu_move_distance_e, nullptr,
          GET_TEXT_F(MSG_HOTEND_TOO_COLD), (const char *)nullptr, F("!")
        );
      });
    }
    else
      _goto_menu_move_distance_e();
  }

#endif

void menu_move() {
  START_MENU();
  BACK_ITEM(MSG_MOTION);

  /*
  #if BOTH(HAS_SOFTWARE_ENDSTOPS, SOFT_ENDSTOPS_MENU_ITEM)
    EDIT_ITEM(bool, MSG_LCD_SOFT_ENDSTOPS, &soft_endstop._enabled);
  #endif
  */

  /*
  // Move submenu for each axis
  if (NONE(IS_KINEMATIC, NO_MOTION_BEFORE_HOMING) || all_axes_homed()) {
    if (TERN1(DELTA, current_position.z <= delta_clip_start_height)) {
      SUBMENU_N(X_AXIS, MSG_MOVE_N, []{ _menu_move_distance(X_AXIS, []{ lcd_move_axis(X_AXIS); }); });
      #if HAS_Y_AXIS
        SUBMENU_N(Y_AXIS, MSG_MOVE_N, []{ _menu_move_distance(Y_AXIS, []{ lcd_move_axis(Y_AXIS); }); });
      #endif
    }
    else {
      #if ENABLED(DELTA)
        ACTION_ITEM(MSG_FREE_XY, []{ line_to_z(delta_clip_start_height); ui.synchronize(); });
      #endif
    }
    #if HAS_Z_AXIS
      #define _AXIS_MOVE(N) SUBMENU_N(N, MSG_MOVE_N, []{ _menu_move_distance(AxisEnum(N), []{ lcd_move_axis(AxisEnum(N)); }); });
      REPEAT_S(2, NUM_AXES, _AXIS_MOVE);
    #endif
  }
  else
    GCODES_ITEM(MSG_AUTO_HOME, FPSTR(G28_STR));
    */

  #if ANY(SWITCHING_EXTRUDER, SWITCHING_NOZZLE, MAGNETIC_SWITCHING_TOOLHEAD)

    #if EXTRUDERS >= 4
      switch (active_extruder) {
        case 0: GCODES_ITEM_N(1, MSG_SELECT_E, F("T1")); break;
        case 1: GCODES_ITEM_N(0, MSG_SELECT_E, F("T0")); break;
        case 2: GCODES_ITEM_N(3, MSG_SELECT_E, F("T3")); break;
        case 3: GCODES_ITEM_N(2, MSG_SELECT_E, F("T2")); break;
        #if EXTRUDERS == 6
          case 4: GCODES_ITEM_N(5, MSG_SELECT_E, F("T5")); break;
          case 5: GCODES_ITEM_N(4, MSG_SELECT_E, F("T4")); break;
        #endif
      }
    #elif EXTRUDERS == 3
      if (active_extruder < 2) {
        if (active_extruder)
          GCODES_ITEM_N(0, MSG_SELECT_E, F("T0"));
        else
          GCODES_ITEM_N(1, MSG_SELECT_E, F("T1"));
      }
    #else
      if (active_extruder)
        GCODES_ITEM_N(0, MSG_SELECT_E, F("T0"));
      else
        GCODES_ITEM_N(1, MSG_SELECT_E, F("T1"));
    #endif

  #elif ENABLED(DUAL_X_CARRIAGE)

    if (active_extruder)
      GCODES_ITEM_N(0, MSG_SELECT_E, F("T0"));
    else
      GCODES_ITEM_N(1, MSG_SELECT_E, F("T1"));

  #endif

  #if E_MANUAL

    // The current extruder
    SUBMENU(MSG_MOVE_E, _menu_move_distance_e_maybe);

    #define SUBMENU_MOVE_E(N) SUBMENU_N(N, MSG_MOVE_EN, []{ _menu_move_distance(E_AXIS, []{ lcd_move_e(N); }, N); });

    #if EITHER(SWITCHING_EXTRUDER, SWITCHING_NOZZLE)

      // ...and the non-switching
      #if E_MANUAL == 7 || E_MANUAL == 5 || E_MANUAL == 3
        SUBMENU_MOVE_E(E_MANUAL - 1);
      #endif

    #elif MULTI_E_MANUAL

      // Independent extruders with one E stepper per hotend
      REPEAT(E_MANUAL, SUBMENU_MOVE_E);

    #endif

  #endif // E_MANUAL

  END_MENU();
}

/*
#define _HOME_ITEM(N) GCODES_ITEM_N(N##_AXIS, MSG_AUTO_HOME_A, F("G28X" STR_##N));

#if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
  //
  // "Motion" > "Homing" submenu
  //
  void menu_home() {
    START_MENU();
    BACK_ITEM(MSG_MOTION);

    GCODES_ITEM(MSG_AUTO_HOME, FPSTR(G28_STR));
    MAIN_AXIS_MAP(_HOME_ITEM);

    END_MENU();
  }
#endif
*/

/*
#if ENABLED(AUTO_BED_LEVELING_UBL)
  void _lcd_ubl_level_bed();
#elif ENABLED(LCD_BED_LEVELING)
  void menu_bed_leveling();
#endif
*/


/*
#if ENABLED(ASSISTED_TRAMMING_WIZARD)
  void goto_tramming_wizard();
#endif
*/

void menu_motion() {
  START_MENU();

  //
  // ^ Main
  //
  BACK_ITEM(MSG_MAIN);

  //
  // Move Axis
  //
  SUBMENU(MSG_MOVE_AXIS, menu_move);

  //
  // Auto Home
  //
  /*
  #if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
    SUBMENU(MSG_HOMING, menu_home);
  #else
    GCODES_ITEM(MSG_AUTO_HOME, FPSTR(G28_STR));
    #if ENABLED(INDIVIDUAL_AXIS_HOMING_MENU)
      MAIN_AXIS_MAP(_HOME_ITEM);
    #endif
  #endif

  //
  // Auto-calibration
  //
  #if ENABLED(CALIBRATION_GCODE)
    GCODES_ITEM(MSG_AUTO_CALIBRATE, F("G425"));
  #endif

  //
  // Auto Z-Align
  //
  #if EITHER(Z_STEPPER_AUTO_ALIGN, MECHANICAL_GANTRY_CALIBRATION)
    GCODES_ITEM(MSG_AUTO_Z_ALIGN, F("G34"));
  #endif

  //
  // Assisted Bed Tramming
  //
  #if ENABLED(ASSISTED_TRAMMING_WIZARD)
    SUBMENU(MSG_TRAMMING_WIZARD, goto_tramming_wizard);
  #endif

  //
  // Level Bed
  //
  #if ENABLED(AUTO_BED_LEVELING_UBL)

    SUBMENU(MSG_UBL_LEVEL_BED, _lcd_ubl_level_bed);

  #elif ENABLED(LCD_BED_LEVELING)

    if (!g29_in_progress)
      SUBMENU(MSG_BED_LEVELING, menu_bed_leveling);

  #elif HAS_LEVELING && DISABLED(SLIM_LCD_MENUS)

    #if DISABLED(PROBE_MANUALLY)
      GCODES_ITEM(MSG_LEVEL_BED, F("G29N"));
    #endif

    if (all_axes_homed() && leveling_is_valid()) {
      bool show_state = planner.leveling_active;
      EDIT_ITEM(bool, MSG_BED_LEVELING, &show_state, _lcd_toggle_bed_leveling);
    }

    #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
      editable.decimal = planner.z_fade_height;
      EDIT_ITEM_FAST(float3, MSG_Z_FADE_HEIGHT, &editable.decimal, 0, 100, []{ set_z_fade_height(editable.decimal); });
    #endif

  #endif

  #if ENABLED(LCD_BED_TRAMMING) && DISABLED(LCD_BED_LEVELING)
    SUBMENU(MSG_BED_TRAMMING, _lcd_level_bed_corners);
  #endif

  #if ENABLED(Z_MIN_PROBE_REPEATABILITY_TEST)
    GCODES_ITEM(MSG_M48_TEST, F("G28O\nM48 P10"));
  #endif
  */

  //
  // Disable Steppers
  //
  GCODES_ITEM(MSG_DISABLE_STEPPERS, F("M84"));

  END_MENU();
}

#endif // HAS_MARLINUI_MENU
