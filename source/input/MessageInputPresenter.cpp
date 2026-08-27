#include "input/MessageInputPresenter.h"

MessageInputPresenter::MessageInputPresenter(lv_obj_t *textarea, lv_obj_t *modeLabel) : textarea(textarea), modeLabel(modeLabel)
{
    lv_textarea_set_text_selection(textarea, true);
    lv_obj_t *label = lv_textarea_get_label(textarea);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(label, lv_palette_main(LV_PALETTE_BLUE), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_SELECTED);
}

void MessageInputPresenter::clearPreeditSelection()
{
    lv_obj_t *label = lv_textarea_get_label(textarea);
    lv_label_set_text_selection_start(label, LV_LABEL_TEXT_SELECTION_OFF);
    lv_label_set_text_selection_end(label, LV_LABEL_TEXT_SELECTION_OFF);
}

void MessageInputPresenter::render(const MessageInputSession &session)
{
    const auto view = session.viewState();
    lv_textarea_set_text(textarea, view.text.data());
    lv_textarea_set_cursor_pos(textarea, view.cursor);
    lv_label_set_text(modeLabel, session.isKoreanMode() ? "한" : "En");

    if (view.hasPreedit) {
        lv_obj_t *label = lv_textarea_get_label(textarea);
        lv_label_set_text_selection_start(label, view.preeditStart);
        lv_label_set_text_selection_end(label, view.preeditEnd);
    } else {
        clearPreeditSelection();
    }
    lv_obj_invalidate(textarea);
}
