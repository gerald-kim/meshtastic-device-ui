#pragma once

#include "input/MessageInputSession.h"
#include "lvgl.h"

/** Selection-only LVGL renderer for the product Messages input session. */
class MessageInputPresenter
{
  public:
    MessageInputPresenter(lv_obj_t *textarea, lv_obj_t *modeLabel);
    void render(const MessageInputSession &session);

  private:
    void clearPreeditSelection();

    lv_obj_t *textarea;
    lv_obj_t *modeLabel;
};
