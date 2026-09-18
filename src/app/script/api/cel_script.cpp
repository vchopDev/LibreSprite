// LibreSprite
// Copyright (C) 2021-2026  LibreSprite contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "delta/Extension.hpp"
#include "delta/JSON.hpp"
#include "di.hpp"
#include "app/script/api/script_api_common.h"

#include "app/document.h"
#include "app/document_api.h"
#include "app/transaction.h"
#include "app/ui_context.h"
#include "doc/cel.h"
#include "doc/image.h"

#include <memory>
#include <stdexcept>

class CelExtension : public Extension {
public:
  CelExtension() {
    auto& clazz = addClass<void, doc::Cel>("Cel");
    clazz.setConstructor() = []() -> std::shared_ptr<void> {
      throw std::runtime_error{"Cel cannot be constructed directly"};
    };

    clazz.addGetter("x") = [](doc::Cel& cel) -> JSON::Value {
      return (double)cel.x();
    };
    clazz.addSetter("x") = [](doc::Cel& cel, JSON::Value& v) {
      cel.setPosition(static_cast<int>(v), cel.y());
    };

    clazz.addGetter("y") = [](doc::Cel& cel) -> JSON::Value {
      return (double)cel.y();
    };
    clazz.addSetter("y") = [](doc::Cel& cel, JSON::Value& v) {
      cel.setPosition(cel.x(), static_cast<int>(v));
    };

    clazz.addGetter("image") = [](doc::Cel& cel) -> JSON::Value {
      return JSON::makeNative(script_api::wrap(cel.image()));
    };

    clazz.addGetter("frame") = [](doc::Cel& cel) -> JSON::Value {
      return (double)cel.frame();
    };

    clazz.addMethod("setPosition") = [](doc::Cel& cel, double x, double y) -> JSON::Value {
      cel.setPosition((int)x, (int)y);
      return {};
    };

    clazz.addGetter("opacity") = [](doc::Cel& cel) -> JSON::Value {
      return (double)cel.opacity();
    };
    clazz.addSetter("opacity") = [](doc::Cel& cel, JSON::Value& v) {
      const int opacity = static_cast<int>(v);
      if (opacity < 0 || opacity > 255)
        throw std::runtime_error{"Cel opacity must be between 0 and 255"};
      auto* doc = static_cast<app::Document*>(cel.document());
      auto* spr = cel.sprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).setCelOpacity(spr, script_api::wrap(&cel), opacity);
      tx.commit();
    };
  }
};

static di::provide<Extension, CelExtension> x{"cel"};
