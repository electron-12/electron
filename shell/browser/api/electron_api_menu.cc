// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_menu.h"

#include <map>
#include <utility>

#include "shell/browser/api/ui_event.h"
#include "shell/browser/javascript_environment.h"
#include "shell/browser/native_window.h"
#include "shell/common/gin_converters/accelerator_converter.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_converters/gurl_converter.h"
#include "shell/common/gin_converters/image_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"
#include "ui/base/models/image_model.h"

#if defined(OS_MAC)

namespace gin {

using SharingItem = electron::ElectronMenuModel::SharingItem;

template <>
struct Converter<SharingItem> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     SharingItem* out) {
    gin_helper::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict))
      return false;
    dict.GetOptional("texts", &(out->texts));
    dict.GetOptional("filePaths", &(out->file_paths));
    dict.GetOptional("urls", &(out->urls));
    return true;
  }
};

}  // namespace gin

#endif

namespace electron {

namespace api {

gin::WrapperInfo Menu::kWrapperInfo = {gin::kEmbedderNativeGin};

Menu::Menu(gin::Arguments* args) : model_(new ElectronMenuModel(this)) {
  model_->AddObserver(this);

#if defined(OS_MAC)
  gin_helper::Dictionary options;
  if (args->GetNext(&options)) {
    ElectronMenuModel::SharingItem item;
    if (options.Get("sharingItem", &item))
      model_->SetSharingItem(std::move(item));
  }
#endif
}

Menu::~Menu() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

bool InvokeBoolMethod(const Menu* menu,
                      const char* method,
                      int command_id,
                      bool default_value = false) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  // We need to cast off const here because GetWrapper() is non-const, but
  // ui::SimpleMenuModel::Delegate's methods are const.
  v8::Local<v8::Value> val = gin_helper::CallMethod(
      isolate, const_cast<Menu*>(menu), method, command_id);
  bool ret = false;
  return gin::ConvertFromV8(isolate, val, &ret) ? ret : default_value;
}

bool Menu::IsCommandIdChecked(int command_id) const {
  return InvokeBoolMethod(this, "_isCommandIdChecked", command_id);
}

bool Menu::IsCommandIdEnabled(int command_id) const {
  return InvokeBoolMethod(this, "_isCommandIdEnabled", command_id);
}

bool Menu::IsCommandIdVisible(int command_id) const {
  return InvokeBoolMethod(this, "_isCommandIdVisible", command_id);
}

bool Menu::ShouldCommandIdWorkWhenHidden(int command_id) const {
  return InvokeBoolMethod(this, "_shouldCommandIdWorkWhenHidden", command_id);
}

bool Menu::GetAcceleratorForCommandIdWithParams(
    int command_id,
    bool use_default_accelerator,
    ui::Accelerator* accelerator) const {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> val = gin_helper::CallMethod(
      isolate, const_cast<Menu*>(this), "_getAcceleratorForCommandId",
      command_id, use_default_accelerator);
  return gin::ConvertFromV8(isolate, val, accelerator);
}

bool Menu::ShouldRegisterAcceleratorForCommandId(int command_id) const {
  return InvokeBoolMethod(this, "_shouldRegisterAcceleratorForCommandId",
                          command_id);
}

#if defined(OS_MAC)
bool Menu::GetSharingItemForCommandId(
    int command_id,
    ElectronMenuModel::SharingItem* item) const {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> val =
      gin_helper::CallMethod(isolate, const_cast<Menu*>(this),
                             "_getSharingItemForCommandId", command_id);
  return gin::ConvertFromV8(isolate, val, item);
}
#endif

void Menu::ExecuteCommand(int command_id, int flags) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  gin_helper::CallMethod(isolate, const_cast<Menu*>(this), "_executeCommand",
                         CreateEventFromFlags(flags), command_id);
}

void Menu::OnMenuWillShow(ui::SimpleMenuModel* source) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope scope(isolate);
  gin_helper::CallMethod(isolate, const_cast<Menu*>(this), "_menuWillShow");
}

base::OnceClosure Menu::BindSelfToClosure(base::OnceClosure callback) {
  // return ((callback, ref) => { callback() }).bind(null, callback, this)
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::Locker locker(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> self;
  if (GetWrapper(isolate).ToLocal(&self)) {
    v8::Global<v8::Value> ref(isolate, self);
    return base::BindOnce(
        [](base::OnceClosure callback, v8::Global<v8::Value> ref) {
          std::move(callback).Run();
        },
        std::move(callback), std::move(ref));
  } else {
    return base::DoNothing();
  }
}

void Menu::InsertItemAt(int index,
                        int command_id,
                        const base::string16& label) {
  model_->InsertItemAt(index, command_id, label);
}

void Menu::InsertSeparatorAt(int index) {
  model_->InsertSeparatorAt(index, ui::NORMAL_SEPARATOR);
}

void Menu::InsertCheckItemAt(int index,
                             int command_id,
                             const base::string16& label) {
  model_->InsertCheckItemAt(index, command_id, label);
}

void Menu::InsertRadioItemAt(int index,
                             int command_id,
                             const base::string16& label,
                             int group_id) {
  model_->InsertRadioItemAt(index, command_id, label, group_id);
}

void Menu::InsertSubMenuAt(int index,
                           int command_id,
                           const base::string16& label,
                           Menu* menu) {
  menu->parent_ = this;
  model_->InsertSubMenuAt(index, command_id, label, menu->model_.get());
}

void Menu::SetIcon(int index, const gfx::Image& image) {
  model_->SetIcon(index, ui::ImageModel::FromImage(image));
}

void Menu::SetSublabel(int index, const base::string16& sublabel) {
  model_->SetSecondaryLabel(index, sublabel);
}

void Menu::SetToolTip(int index, const base::string16& toolTip) {
  model_->SetToolTip(index, toolTip);
}

void Menu::SetRole(int index, const base::string16& role) {
  model_->SetRole(index, role);
}

void Menu::Clear() {
  model_->Clear();
}

int Menu::GetIndexOfCommandId(int command_id) {
  return model_->GetIndexOfCommandId(command_id);
}

int Menu::GetItemCount() const {
  return model_->GetItemCount();
}

int Menu::GetCommandIdAt(int index) const {
  return model_->GetCommandIdAt(index);
}

base::string16 Menu::GetLabelAt(int index) const {
  return model_->GetLabelAt(index);
}

base::string16 Menu::GetSublabelAt(int index) const {
  return model_->GetSecondaryLabelAt(index);
}

base::string16 Menu::GetToolTipAt(int index) const {
  return model_->GetToolTipAt(index);
}

#ifdef DCHECK_IS_ON
base::string16 Menu::GetAcceleratorTextAtForTesting(int index) const {
  ui::Accelerator accelerator;
  model_->GetAcceleratorAtWithParams(index, true, &accelerator);
  return accelerator.GetShortcutText();
}
#endif

bool Menu::IsItemCheckedAt(int index) const {
  return model_->IsItemCheckedAt(index);
}

bool Menu::IsEnabledAt(int index) const {
  return model_->IsEnabledAt(index);
}

bool Menu::IsVisibleAt(int index) const {
  return model_->IsVisibleAt(index);
}

bool Menu::WorksWhenHiddenAt(int index) const {
  return model_->WorksWhenHiddenAt(index);
}

void Menu::OnMenuWillClose() {
  Unpin();
  Emit("menu-will-close");
}

void Menu::OnMenuWillShow() {
  Pin(v8::Isolate::GetCurrent());
  Emit("menu-will-show");
}

// static
v8::Local<v8::ObjectTemplate> Menu::FillObjectTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> templ) {
  return gin::ObjectTemplateBuilder(isolate, "Menu", templ)
      .SetMethod("insertItem", &Menu::InsertItemAt)
      .SetMethod("insertCheckItem", &Menu::InsertCheckItemAt)
      .SetMethod("insertRadioItem", &Menu::InsertRadioItemAt)
      .SetMethod("insertSeparator", &Menu::InsertSeparatorAt)
      .SetMethod("insertSubMenu", &Menu::InsertSubMenuAt)
      .SetMethod("setIcon", &Menu::SetIcon)
      .SetMethod("setSublabel", &Menu::SetSublabel)
      .SetMethod("setToolTip", &Menu::SetToolTip)
      .SetMethod("setRole", &Menu::SetRole)
      .SetMethod("clear", &Menu::Clear)
      .SetMethod("getIndexOfCommandId", &Menu::GetIndexOfCommandId)
      .SetMethod("getItemCount", &Menu::GetItemCount)
      .SetMethod("getCommandIdAt", &Menu::GetCommandIdAt)
      .SetMethod("getLabelAt", &Menu::GetLabelAt)
      .SetMethod("getSublabelAt", &Menu::GetSublabelAt)
      .SetMethod("getToolTipAt", &Menu::GetToolTipAt)
      .SetMethod("isItemCheckedAt", &Menu::IsItemCheckedAt)
      .SetMethod("isEnabledAt", &Menu::IsEnabledAt)
      .SetMethod("worksWhenHiddenAt", &Menu::WorksWhenHiddenAt)
      .SetMethod("isVisibleAt", &Menu::IsVisibleAt)
      .SetMethod("popupAt", &Menu::PopupAt)
      .SetMethod("closePopupAt", &Menu::ClosePopupAt)
#ifdef DCHECK_IS_ON
      .SetMethod("getAcceleratorTextAt", &Menu::GetAcceleratorTextAtForTesting)
#endif
      .Build();
}

}  // namespace api

}  // namespace electron

namespace {

using electron::api::Menu;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();

  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("Menu", Menu::GetConstructor(context));
#if defined(OS_MAC)
  dict.SetMethod("setApplicationMenu", &Menu::SetApplicationMenu);
  dict.SetMethod("sendActionToFirstResponder",
                 &Menu::SendActionToFirstResponder);
#endif
}

}  // namespace

NODE_LINKED_MODULE_CONTEXT_AWARE(electron_browser_menu, Initialize)
