/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_material.h"
#include "BKE_screen.h"

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "ED_object.h"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MOD_gpencil_ui_common.h" /* Self include */

/**
 * Poll function so these modifier panels only show for grease pencil objects.
 */
static bool gpencil_modifier_ui_poll(const bContext *C, PanelType *UNUSED(pt))
{
  Object *ob = ED_object_active_context(C);

  return (ob != NULL) && (ob->type == OB_GPENCIL);
}

/* -------------------------------------------------------------------- */
/** \name Panel Drag and Drop, Expansion Saving
 * \{ */

/**
 * Move a modifier to the index it's moved to after a drag and drop.
 */
static void gpencil_modifier_reorder(bContext *C, Panel *panel, int new_index)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  GpencilModifierData *md = (GpencilModifierData *)md_ptr->data;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_gpencil_modifier_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "modifier", md->name);
  RNA_int_set(&props_ptr, "index", new_index);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);
  WM_operator_properties_free(&props_ptr);
}

static short get_gpencil_modifier_expand_flag(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  GpencilModifierData *md = (GpencilModifierData *)md_ptr->data;
  return md->ui_expand_flag;
}

static void set_gpencil_modifier_expand_flag(const bContext *UNUSED(C),
                                             Panel *panel,
                                             short expand_flag)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  GpencilModifierData *md = (GpencilModifierData *)md_ptr->data;
  md->ui_expand_flag = expand_flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Panel Layouts
 * \{ */

void gpencil_modifier_masking_panel_draw(Panel *panel, bool use_material, bool use_vertex)
{
  uiLayout *row, *col, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");
  bool has_layer = RNA_string_length(ptr, "layer") != 0;

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, true);
  row = uiLayoutRow(col, true);
  uiItemPointerR(row, ptr, "layer", &obj_data_ptr, "layers", NULL, ICON_GREASEPENCIL);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, has_layer);
  uiLayoutSetPropDecorate(sub, false);
  uiItemR(sub, ptr, "invert_layers", 0, "", ICON_ARROW_LEFTRIGHT);

  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "layer_pass", 0, NULL, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, RNA_int_get(ptr, "layer_pass") != 0);
  uiLayoutSetPropDecorate(sub, false);
  uiItemR(sub, ptr, "invert_layer_pass", 0, "", ICON_ARROW_LEFTRIGHT);

  if (use_material) {
    PointerRNA material_ptr = RNA_pointer_get(ptr, "material");
    bool has_material = !RNA_pointer_is_null(&material_ptr);

    /* Because the Gpencil modifier material property used to be a string in an earlier version of
     * Blender, we need to check if the material is valid and display it differently if so. */
    bool valid = false;
    {
      if (!has_material) {
        valid = true;
      }
      else {
        Material *current_material = material_ptr.data;
        Object *ob = ob_ptr.data;
        for (int i = 0; i <= ob->totcol; i++) {
          Material *mat = BKE_object_material_get(ob, i);
          if (mat == current_material) {
            valid = true;
            break;
          }
        }
      }
    }

    col = uiLayoutColumn(layout, true);
    row = uiLayoutRow(col, true);
    uiLayoutSetRedAlert(row, !valid);
    uiItemPointerR(row,
                   ptr,
                   "material",
                   &obj_data_ptr,
                   "materials",
                   NULL,
                   valid ? ICON_SHADING_TEXTURE : ICON_ERROR);
    sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, has_material);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, ptr, "invert_materials", 0, "", ICON_ARROW_LEFTRIGHT);

    row = uiLayoutRow(col, true);
    uiItemR(row, ptr, "pass_index", 0, NULL, ICON_NONE);
    sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, RNA_int_get(ptr, "pass_index") != 0);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, ptr, "invert_material_pass", 0, "", ICON_ARROW_LEFTRIGHT);
  }

  if (use_vertex) {
    bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;

    row = uiLayoutRow(layout, true);
    uiItemPointerR(row, ptr, "vertex_group", &ob_ptr, "vertex_groups", NULL, ICON_NONE);
    sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, has_vertex_group);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, ptr, "invert_vertex", 0, "", ICON_ARROW_LEFTRIGHT);
  }
}

void gpencil_modifier_curve_header_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiItemR(layout, ptr, "use_custom_curve", 0, NULL, ICON_NONE);
}

void gpencil_modifier_curve_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiTemplateCurveMapping(layout, ptr, "curve", 0, false, false, false, false);
}

/**
 * Draw modifier error message.
 */
void gpencil_modifier_panel_end(uiLayout *layout, PointerRNA *ptr)
{
  GpencilModifierData *md = ptr->data;
  if (md->error) {
    uiLayout *row = uiLayoutRow(layout, false);
    uiItemL(row, IFACE_(md->error), ICON_ERROR);
  }
}

/**
 * Gets RNA pointers for the active object and the panel's modifier data.
 */
#define ERROR_LIBDATA_MESSAGE TIP_("External library data")
PointerRNA *gpencil_modifier_panel_get_property_pointers(Panel *panel, PointerRNA *r_ob_ptr)
{
  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  BLI_assert(RNA_struct_is_a(ptr->type, &RNA_GpencilModifier));

  if (r_ob_ptr != NULL) {
    RNA_pointer_create(ptr->owner_id, &RNA_Object, ptr->owner_id, r_ob_ptr);
  }

  uiBlock *block = uiLayoutGetBlock(panel->layout);
  UI_block_lock_clear(block);
  UI_block_lock_set(block, ID_IS_LINKED((Object *)ptr->owner_id), ERROR_LIBDATA_MESSAGE);

  uiLayoutSetContextPointer(panel->layout, "modifier", ptr);

  return ptr;
}

static void gpencil_modifier_ops_extra_draw(bContext *C, uiLayout *layout, void *md_v)
{
  PointerRNA op_ptr;
  uiLayout *row;
  GpencilModifierData *md = (GpencilModifierData *)md_v;

  PointerRNA ptr;
  Object *ob = ED_object_active_context(C);
  RNA_pointer_create(&ob->id, &RNA_GpencilModifier, md, &ptr);
  uiLayoutSetContextPointer(layout, "modifier", &ptr);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  uiLayoutSetUnitsX(layout, 4.0f);

  /* Duplicate. */
  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
          ICON_DUPLICATE,
          "OBJECT_OT_gpencil_modifier_copy");

  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy to Selected"),
          0,
          "OBJECT_OT_gpencil_modifier_copy_to_selected");

  uiItemS(layout);

  /* Move to first. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "OBJECT_OT_gpencil_modifier_move_to_index",
              IFACE_("Move to First"),
              ICON_TRIA_UP,
              NULL,
              WM_OP_INVOKE_DEFAULT,
              0,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", 0);
  if (!md->prev) {
    uiLayoutSetEnabled(row, false);
  }

  /* Move to last. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "OBJECT_OT_gpencil_modifier_move_to_index",
              IFACE_("Move to Last"),
              ICON_TRIA_DOWN,
              NULL,
              WM_OP_INVOKE_DEFAULT,
              0,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", BLI_listbase_count(&ob->greasepencil_modifiers) - 1);
  if (!md->next) {
    uiLayoutSetEnabled(row, false);
  }
}

/* bfa - Grease pencil modifiers - put apply into the header #2185 */
static void gpencil_modifier_panel_header(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row, *sub, *op_row;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  GpencilModifierData *md = (GpencilModifierData *)ptr->data;

  uiLayoutSetContextPointer(panel->layout, "modifier", ptr);

  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);
  bool narrow_panel = (panel->sizex < UI_UNIT_X * 9 && panel->sizex != 0);

  /* Modifier Icon. */
  row = uiLayoutRow(layout, false);
  if (mti->isDisabled && mti->isDisabled(md, 0)) {
    uiLayoutSetRedAlert(row, true);
  }
  uiItemL(row, "", RNA_struct_ui_icon(ptr->type));

  /* Modifier name. */
  row = uiLayoutRow(layout, true);
  if (!narrow_panel) {
    uiItemR(row, ptr, "name", 0, "", ICON_NONE);
  }
  else {
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
  }

  /* Display mode buttons. */
  if (mti->flags & eGpencilModifierTypeFlag_SupportsEditmode) {
    sub = uiLayoutRow(row, true);
    uiItemR(sub, ptr, "show_in_editmode", 0, "", ICON_NONE);
  }
  uiItemR(row, ptr, "show_viewport", 0, "", ICON_NONE);
  uiItemR(row, ptr, "show_render", 0, "", ICON_NONE);

  op_row = uiLayoutRow(layout, true);

  /* Apply. */
  if (!(mti->flags & eGpencilModifierTypeFlag_NoApply)) {
    uiItemO(op_row, "", ICON_CHECKMARK, "OBJECT_OT_gpencil_modifier_apply");
  }

  /* Extra operators. */
  // row = uiLayoutRow(layout, true);
  uiItemMenuF(op_row, "", ICON_DOWNARROW_HLT, gpencil_modifier_ops_extra_draw, md);

  /* Remove button. */
  uiItemO(op_row, "", ICON_X, "OBJECT_OT_gpencil_modifier_remove");

  /* Extra padding. */
  uiItemS(layout);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Registration Helpers
 * \{ */

/**
 * Create a panel in the context's region
 */
PanelType *gpencil_modifier_panel_register(ARegionType *region_type,
                                           GpencilModifierType type,
                                           PanelDrawFn draw)
{
  PanelType *panel_type = MEM_callocN(sizeof(PanelType), __func__);

  BKE_gpencil_modifierType_panel_id(type, panel_type->idname);
  BLI_strncpy(panel_type->label, "", BKE_ST_MAXNAME);
  BLI_strncpy(panel_type->context, "modifier", BKE_ST_MAXNAME);
  BLI_strncpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA, BKE_ST_MAXNAME);

  panel_type->draw_header = gpencil_modifier_panel_header;
  panel_type->draw = draw;
  panel_type->poll = gpencil_modifier_ui_poll;

  /* Give the panel the special flag that says it was built here and corresponds to a
   * modifier rather than a #PanelType. */
  panel_type->flag = PANEL_TYPE_HEADER_EXPAND | PANEL_TYPE_DRAW_BOX | PANEL_TYPE_INSTANCED;
  panel_type->reorder = gpencil_modifier_reorder;
  panel_type->get_list_data_expand_flag = get_gpencil_modifier_expand_flag;
  panel_type->set_list_data_expand_flag = set_gpencil_modifier_expand_flag;

  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/**
 * Add a child panel to the parent.
 *
 * \note To create the panel type's idname, it appends the \a name argument to the \a parent's
 * idname.
 */
PanelType *gpencil_modifier_subpanel_register(ARegionType *region_type,
                                              const char *name,
                                              const char *label,
                                              PanelDrawFn draw_header,
                                              PanelDrawFn draw,
                                              PanelType *parent)
{
  PanelType *panel_type = MEM_callocN(sizeof(PanelType), __func__);

  BLI_snprintf(panel_type->idname, BKE_ST_MAXNAME, "%s_%s", parent->idname, name);
  BLI_strncpy(panel_type->label, label, BKE_ST_MAXNAME);
  BLI_strncpy(panel_type->context, "modifier", BKE_ST_MAXNAME);
  BLI_strncpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA, BKE_ST_MAXNAME);

  panel_type->draw_header = draw_header;
  panel_type->draw = draw;
  panel_type->poll = gpencil_modifier_ui_poll;
  panel_type->flag = (PANEL_TYPE_DEFAULT_CLOSED | PANEL_TYPE_DRAW_BOX);

  BLI_assert(parent != NULL);
  BLI_strncpy(panel_type->parent_id, parent->idname, BKE_ST_MAXNAME);
  panel_type->parent = parent;
  BLI_addtail(&parent->children, BLI_genericNodeN(panel_type));
  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/** \} */
